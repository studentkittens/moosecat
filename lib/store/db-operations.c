#include "db-store.h"
#include "db-dirs.h"
#include "db-macros.h"
#include "db-private.h"

#include "../mpd/client.h"
#include "../mpd/signal-helper.h"
#include "../mpd/signal.h"
#include "../mpd/update.h"

typedef struct {
    mc_Store *store;
    GAsyncQueue *queue;
} mc_StoreQueueTag;


///////////////////////////////////

static gpointer mc_store_do_list_all_info_sql_thread(gpointer user_data)
{
    mc_StoreQueueTag *tag = user_data;
    mc_Store *self = tag->store;
    GAsyncQueue *queue = tag->queue;

    struct mpd_entity *ent = NULL;

    /* Begin a new transaction */
    mc_stprv_begin(self);

    while ((gpointer)(ent = g_async_queue_pop(queue)) != queue) {
        switch (mpd_entity_get_type(ent)) {
            case MPD_ENTITY_TYPE_SONG: {

                struct mpd_song *song = (struct mpd_song *) mpd_entity_get_song(ent);
                mc_stack_append(self->stack, song);
                mc_stprv_insert_song(self, song);

                /* Not sure if this is a nice way,
                * but for now it works. There might be a proper way:
                * duplicate the song with mpd_song_dup, and use mpd_entity_free,
                * but that adds extra memory usage, and costs about 0.2 seconds.
                * on my setup here - I think this will work fine.
                * */
                g_free(ent);

                break;
            }

            case MPD_ENTITY_TYPE_DIRECTORY: {
                const struct mpd_directory *dir = mpd_entity_get_directory(ent);

                if (dir != NULL) {
                    mc_stprv_dir_insert(self, mpd_directory_get_path(dir));
                    mpd_entity_free(ent);
                }

                break;
            }

            case MPD_ENTITY_TYPE_PLAYLIST:
            default: {
                mpd_entity_free(ent);
                ent = NULL;
            }
        }
    }

    /* Commit changes */
    mc_stprv_commit(self);

    return NULL;
}

///////////////////////////////////

/*
 * Query a 'listallinfo' from the MPD Server, and insert all returned
 * song into the database and the pointer stack.
 *
 * Other items like directories and playlists are discarded at the moment.
 *
 * BUGS: mpd's protocol reference states that
 *       very large query-responses might cause
 *       a client disconnect by the server...
 *
 *       If this is happening, it might be because of this,
 *       but till now this did not happen.
 *
 *       Also, this value is adjustable in mpd.conf
 *       e.g. max_command_list_size "16192"
 */
void mc_store_oper_listallinfo(mc_Store *store, volatile bool *cancel)
{
    g_assert(store);
    g_assert(store->client);

    mc_Client *self = store->client;
    int progress_counter = 0;
    size_t db_version = 0;
    struct mpd_stats * stats = mc_lock_statistics(store->client);

    if(stats == NULL) {
        mc_unlock_statistics(store->client);
        return;
    }
    
    int number_of_songs = mpd_stats_get_number_of_songs(stats);
    mc_unlock_statistics(store->client);

    db_version = mc_stprv_get_db_version(store);

    if(store->force_update_listallinfo == false) {
        struct mpd_stats * stats = mc_lock_statistics(store->client);
        if(stats == NULL) {
            return;
        }

        size_t db_update_time = mpd_stats_get_db_update_time(stats);
        mc_unlock_statistics(store->client);

        if (db_update_time  == db_version) {
            mc_shelper_report_progress(
                    self, true,
                    "database: Will not update database, timestamp didn't change."
            );
            return;
        } else {
            mc_shelper_report_progress(
                    self, true,
                    "database: Will update database (%u != %u)",
                    (unsigned)db_update_time, (unsigned)db_version
            );
        }
    } else {
        mc_shelper_report_progress(self, true, "database: Doing forced listallinfo");
    }

    GTimer *timer = NULL;
    GAsyncQueue *queue = g_async_queue_new();
    GThread *sql_thread = NULL;

    mc_StoreQueueTag tag;
    tag.queue = queue;
    tag.store = store;

    /* We're building the whole table new,
     * so throw away old data */
    mc_stprv_delete_songs_table(store);
    mc_stprv_dir_delete(store);

    /* Also throw away current stack, and reallocate it
     * to the fitting size.
     *
     * Apparently GPtrArray does not support reallocating,
     * without adding NULL-cells to the array? */
    if (store->stack != NULL) {
        mc_stack_free(store->stack);
    }

    store->stack = mc_stack_create(
            number_of_songs + 1,
            (GDestroyNotify) mpd_song_free
    );

    /* Profiling */
    timer = g_timer_new();

    sql_thread = g_thread_new(
            "sql-thread",
            mc_store_do_list_all_info_sql_thread,
            &tag
    );

    struct mpd_connection *conn = mc_get(store->client);
    if(conn != NULL) {
        struct mpd_entity *ent = NULL;

        g_timer_start(timer);

        /* Order the real big list */
        mpd_send_list_all_meta(conn, "/");

        while ((ent = mpd_recv_entity(conn)) != NULL) {
            if(mc_jm_check_cancel(store->jm, cancel)) {
                mc_shelper_report_progress(
                        self, false, "database: listallinfo cancelled!"
                );
                break;
            }

            ++progress_counter;

            g_async_queue_push(queue, ent);
        }

        /* This should only happen if the operation was cancelled */
        if (mpd_response_finish(conn) == false) {       
            mc_shelper_report_error(store->client, conn);        
        }                                               
    }
    mc_put(store->client);

    /* tell SQL thread kindly to die, but wait for him to bleed */
    g_async_queue_push(queue, queue);
    g_thread_join(sql_thread);

    mc_shelper_report_progress(
            self, true, "database: retrieved %d songs from mpd (took %2.3fs)",
            number_of_songs, g_timer_elapsed(timer, NULL)
    );

    mc_shelper_report_operation_finished(self, MC_OP_DB_UPDATED);

    g_async_queue_unref(queue);
    g_timer_destroy(timer);
}


///////////////////////////////////

gpointer mc_store_do_plchanges_sql_thread(gpointer user_data)
{
    mc_StoreQueueTag *tag = user_data;
    mc_Store *self = tag->store;
    GAsyncQueue *queue = tag->queue;

    int clipped = 0;
    bool first_song_passed = false;
    struct mpd_song *song = NULL;
    GTimer *timer = g_timer_new();
    gdouble clip_time = 0.0, posid_time = 0.0, stack_time = 0.0;

    /* start a transaction */
    mc_stprv_begin(self);

    while ((gpointer)(song = g_async_queue_pop(queue)) != queue) {
        if(first_song_passed == false) {
            g_timer_start(timer);
            clipped = mc_stprv_queue_clip(
                    self,
                    mpd_song_get_pos(song)
            );
            clip_time = g_timer_elapsed(timer, NULL);
            g_timer_start(timer);

            first_song_passed = true;
        }

        mc_stprv_queue_insert_posid(
                self,
                mpd_song_get_pos(song),
                mpd_song_get_id(song),
                mpd_song_get_uri(song)
        );
        mpd_song_free(song);
    }

    posid_time = g_timer_elapsed(timer, NULL);

    if (clipped > 0) {
        mc_shelper_report_progress(
                self->client,
                true,
                "database: Clipped %d songs.",
                clipped
        );
    }

    /* Commit all those update statements */
    mc_stprv_commit(self);

    /* Update the pos/id data in the song stack (=> sync with db) */
    g_timer_start(timer);
    mc_stprv_queue_update_stack_posid(self);
    stack_time = g_timer_elapsed(timer, NULL);
    g_timer_destroy(timer);

    mc_shelper_report_progress(
            self->client, true,
            "database: QueueSQL Timing: %2.3fs Clip | %2.3fs Posid | %2.3fs Stack",
            clip_time, posid_time, stack_time
    );

    return NULL;
}

///////////////////////////////////

void mc_store_oper_plchanges(mc_Store *store, volatile bool *cancel)
{
    g_assert(store);

    mc_Client *self = store->client;

    int progress_counter = 0;
    size_t last_pl_version = 0;

    /* get last version of the queue (which we're having already. 
     * (in case full queue is wanted take first version) */
    if(store->force_update_plchanges == false) {
        last_pl_version = mc_stprv_get_pl_version(store);
    }

    if(store->force_update_plchanges == false) {
        size_t current_pl_version = -1;
        struct mpd_status * status = mc_lock_status(store->client);
        if(status != NULL) {
            current_pl_version = mpd_status_get_queue_version(status);
        } else {
            current_pl_version = last_pl_version;
        }
        mc_unlock_status(store->client);

        if (last_pl_version == current_pl_version) {
            mc_shelper_report_progress(
                    self, true,
                    "database: Will not update queue, version didn't change (%d == %d)",
                    (int) last_pl_version, (int) current_pl_version
            );
            return;
        }
    }
    
    GAsyncQueue *queue = g_async_queue_new();
    GThread *sql_thread = NULL;
    GTimer *timer = NULL;

    mc_StoreQueueTag tag;
    tag.queue = queue;
    tag.store = store;

    /* needs to be started after inserting meta attributes, since it calls 'begin;' */
    sql_thread = g_thread_new("queue-update", mc_store_do_plchanges_sql_thread, &tag);

    /* timing */
    timer = g_timer_new();

    /* Now do the actual hard work, send the actual plchanges command,
     * loop over the retrieved contents, and push them to the SQL Thread */
    struct mpd_connection *conn = mc_get(store->client);
    if(conn != NULL) {
        g_timer_start(timer);

        mc_shelper_report_progress(
                self, true,
                "database: Queue was updated. Will do ,,plchanges %d''",
                (int) last_pl_version);

        if (mpd_send_queue_changes_meta(conn, last_pl_version)) {
            struct mpd_song *song = NULL;

            while ((song = mpd_recv_song(conn)) != NULL) {
                if(mc_jm_check_cancel(store->jm, cancel)) {
                    mc_shelper_report_progress(
                            self, false, "database: plchanges cancelled!"
                    );

                    break;
                }

                ++progress_counter;

                g_async_queue_push(queue, song);
            }
        }

        if (mpd_response_finish(conn) == false) {       
            mc_shelper_report_error(store->client, conn);        
        }                                               
    }
    mc_put(store->client);

    /* Killing the SQL thread softly. */
    g_async_queue_push(queue, queue);
    g_thread_join(sql_thread);

    /* a bit of timing report */
    mc_shelper_report_progress(
            self, true,
            "database: updated %d song's pos/id (took %2.3fs)",
            progress_counter, g_timer_elapsed(timer, NULL)
    );

    mc_shelper_report_operation_finished(self, MC_OP_QUEUE_UPDATED);

    g_async_queue_unref(queue);
    g_timer_destroy(timer);
}
