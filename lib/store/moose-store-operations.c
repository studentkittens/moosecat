#include "moose-store-store.h"
#include "moose-store-dirs.h"
#include "moose-store-macros.h"
#include "moose-store-private.h"

#include "../mpd/moose-mpd-client.h"
#include "../mpd/moose-mpd-signal-helper.h"
#include "../mpd/moose-mpd-signal.h"

typedef struct {
    MooseStore * store;
    GAsyncQueue * queue;
} MooseStoreQueueTag;


/* Popping this from a GAsyncQueue means
 * the queue is empty and we night to do some actions
 */
#define EMPTY_QUEUE_INDICATOR 0x1

///////////////////////////////////

static gpointer moose_store_do_list_all_info_sql_thread(gpointer user_data)
{
    MooseStoreQueueTag * tag = user_data;
    MooseStore * self = tag->store;
    GAsyncQueue * queue = tag->queue;

    struct mpd_entity * ent = NULL;

    /* Begin a new transaction */
    moose_stprv_begin(self);

    while ((gpointer)(ent = g_async_queue_pop(queue)) != queue) {
        switch (mpd_entity_get_type(ent)) {
        case MPD_ENTITY_TYPE_SONG: {

            MooseSong * song = moose_song_new_from_struct(
                (struct mpd_song *)mpd_entity_get_song(ent)
                );

            moose_playlist_append(self->stack, song);
            moose_stprv_insert_song(self, song);
            mpd_entity_free(ent);

            break;
        }

        case MPD_ENTITY_TYPE_DIRECTORY: {
            const struct mpd_directory * dir = mpd_entity_get_directory(ent);

            if (dir != NULL) {
                moose_stprv_dir_insert(self, mpd_directory_get_path(dir));
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
    moose_stprv_commit(self);

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
void moose_store_oper_listallinfo(MooseStore * store, volatile bool * cancel)
{
    g_assert(store);
    g_assert(store->client);

    MooseClient * self = store->client;
    int progress_counter = 0;
    size_t db_version = 0;
    MooseStatus * status = moose_client_ref_status(store->client);

    int number_of_songs = moose_status_stats_get_number_of_songs(status);

    db_version = moose_stprv_get_db_version(store);

    if (store->force_update_listallinfo == false) {
        size_t db_update_time = moose_status_stats_get_db_update_time(status);

        if (db_update_time  == db_version) {
            moose_shelper_report_progress(
                self, true,
                "database: Will not update database, timestamp didn't change."
                );
            return;
        } else {
            moose_shelper_report_progress(
                self, true,
                "database: Will update database (%u != %u)",
                (unsigned)db_update_time, (unsigned)db_version
                );
        }
    } else {
        moose_shelper_report_progress(self, true, "database: Doing forced listallinfo");
    }
    moose_status_unref(status);

    GTimer * timer = NULL;
    GAsyncQueue * queue = g_async_queue_new();
    GThread * sql_thread = NULL;

    MooseStoreQueueTag tag;
    tag.queue = queue;
    tag.store = store;

    /* We're building the whole table new,
     * so throw away old data */
    moose_stprv_delete_songs_table(store);
    moose_stprv_dir_delete(store);

    /* Also throw away current stack, and reallocate it
     * to the fitting size.
     *
     * Apparently GPtrArray does not support reallocating,
     * without adding NULL-cells to the array? */
    if (store->stack != NULL) {
        g_object_unref(store->stack);
    }

    store->stack = moose_playlist_new_full(
        number_of_songs + 1,
        (GDestroyNotify)moose_song_unref
        );

    /* Profiling */
    timer = g_timer_new();

    sql_thread = g_thread_new(
        "sql-thread",
        moose_store_do_list_all_info_sql_thread,
        &tag
        );

    struct mpd_connection * conn = moose_client_get(store->client);
    if (conn != NULL) {
        struct mpd_entity * ent = NULL;

        g_timer_start(timer);

        /* Order the real big list */
        mpd_send_list_all_meta(conn, "/");

        while ((ent = mpd_recv_entity(conn)) != NULL) {
            if (moose_jm_check_cancel(store->jm, cancel)) {
                moose_shelper_report_progress(
                    self, false, "database: listallinfo cancelled!"
                    );
                break;
            }

            ++progress_counter;

            g_async_queue_push(queue, ent);
        }

        /* This should only happen if the operation was cancelled */
        if (mpd_response_finish(conn) == false) {
            moose_shelper_report_error(store->client, conn);
        }
    }
    moose_client_put(store->client);

    /* tell SQL thread kindly to die, but wait for him to bleed */
    g_async_queue_push(queue, queue);
    g_thread_join(sql_thread);

    moose_shelper_report_progress(
        self, true, "database: retrieved %d songs from mpd (took %2.3fs)",
        number_of_songs, g_timer_elapsed(timer, NULL)
        );

    moose_shelper_report_operation_finished(self, MOOSE_OP_DB_UPDATED);

    g_async_queue_unref(queue);
    g_timer_destroy(timer);
}


///////////////////////////////////

gpointer moose_store_do_plchanges_sql_thread(gpointer user_data)
{
    MooseStoreQueueTag * tag = user_data;
    MooseStore * self = tag->store;
    GAsyncQueue * queue = tag->queue;

    int clipped = 0;
    bool first_song_passed = false;
    MooseSong * song = NULL;
    GTimer * timer = g_timer_new();
    gdouble clip_time = 0.0, posid_time = 0.0, stack_time = 0.0;

    /* start a transaction */
    moose_stprv_begin(self);

    while ((gpointer)(song = g_async_queue_pop(queue)) != queue) {
        if (first_song_passed == false || song == (gpointer)EMPTY_QUEUE_INDICATOR) {
            g_timer_start(timer);

            int start_position = -1;
            if (song != (gpointer)EMPTY_QUEUE_INDICATOR) {
                start_position = moose_song_get_pos(song);
            }
            clipped = moose_stprv_queue_clip(self, start_position);
            clip_time = g_timer_elapsed(timer, NULL);
            g_timer_start(timer);

            first_song_passed = true;
        }

        if (song != (gpointer)EMPTY_QUEUE_INDICATOR) {
            moose_stprv_queue_insert_posid(
                self,
                moose_song_get_pos(song),
                moose_song_get_id(song),
                moose_song_get_uri(song)
                );
            moose_song_unref(song);
        }
    }

    posid_time = g_timer_elapsed(timer, NULL);

    if (clipped > 0) {
        moose_shelper_report_progress(
            self->client,
            true,
            "database: Clipped %d songs.",
            clipped
            );
    }

    /* Commit all those update statements */
    moose_stprv_commit(self);

    /* Update the pos/id data in the song stack (=> sync with db) */
    g_timer_start(timer);
    moose_stprv_queue_update_stack_posid(self);
    stack_time = g_timer_elapsed(timer, NULL);
    g_timer_destroy(timer);

    moose_shelper_report_progress(
        self->client, true,
        "database: QueueSQL Timing: %2.3fs Clip | %2.3fs Posid | %2.3fs Stack",
        clip_time, posid_time, stack_time
        );

    return NULL;
}

///////////////////////////////////

void moose_store_oper_plchanges(MooseStore * store, volatile bool * cancel)
{
    g_assert(store);

    MooseClient * self = store->client;

    int progress_counter = 0;
    size_t last_pl_version = 0;

    /* get last version of the queue (which we're having already.
     * (in case full queue is wanted take first version) */
    if (store->force_update_plchanges == false) {
        last_pl_version = moose_stprv_get_pl_version(store);
    }

    if (store->force_update_plchanges == false) {
        size_t current_pl_version = -1;
        MooseStatus * status = moose_client_ref_status(store->client);
        if (status != NULL) {
            current_pl_version = moose_status_get_queue_version(status);
        } else {
            current_pl_version = last_pl_version;
        }
        moose_status_unref(status);

        if (last_pl_version == current_pl_version) {
            moose_shelper_report_progress(
                self, true,
                "database: Will not update queue, version didn't change (%d == %d)",
                (int)last_pl_version, (int)current_pl_version
                );
            return;
        }
    }

    GAsyncQueue * queue = g_async_queue_new();
    GThread * sql_thread = NULL;
    GTimer * timer = NULL;

    MooseStoreQueueTag tag;
    tag.queue = queue;
    tag.store = store;

    /* needs to be started after inserting meta attributes, since it calls 'begin;' */
    sql_thread = g_thread_new("queue-update", moose_store_do_plchanges_sql_thread, &tag);

    /* timing */
    timer = g_timer_new();

    /* Now do the actual hard work, send the actual plchanges command,
     * loop over the retrieved contents, and push them to the SQL Thread */
    struct mpd_connection * conn = moose_client_get(store->client);
    if (conn != NULL) {
        g_timer_start(timer);

        moose_shelper_report_progress(
            self, true,
            "database: Queue was updated. Will do ,,plchanges %d''",
            (int)last_pl_version);

        if (mpd_send_queue_changes_meta(conn, last_pl_version)) {
            struct mpd_song * song_struct = NULL;

            while ((song_struct = mpd_recv_song(conn)) != NULL) {
                MooseSong * song = moose_song_new_from_struct(song_struct);
                mpd_song_free(song_struct);

                if (moose_jm_check_cancel(store->jm, cancel)) {
                    moose_shelper_report_progress(
                        self, false, "database: plchanges canceled!"
                        );

                    break;
                }

                ++progress_counter;

                g_async_queue_push(queue, song);
            }

            /* Empty queue - we need to notift he other thread
             * */
            if (progress_counter == 0) {
                g_async_queue_push(queue, (gpointer)EMPTY_QUEUE_INDICATOR);
            }
        }

        if (mpd_response_finish(conn) == false) {
            moose_shelper_report_error(store->client, conn);
        }
    }
    moose_client_put(store->client);

    /* Killing the SQL thread softly. */
    g_async_queue_push(queue, queue);
    g_thread_join(sql_thread);

    /* a bit of timing report */
    moose_shelper_report_progress(
        self, true,
        "database: updated %d song's pos/id (took %2.3fs)",
        progress_counter, g_timer_elapsed(timer, NULL)
        );

    moose_shelper_report_operation_finished(self, MOOSE_OP_QUEUE_UPDATED);

    g_async_queue_unref(queue);
    g_timer_destroy(timer);
}
