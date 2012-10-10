#include "../mpd/client_private.h"
#include "../mpd/signal_helper.h"
#include "../mpd/signal.h"

#include "db_stored_playlists.h"
#include "db.h"

#include "../util/gzip.h"

#include <glib.h>
#include <glib/gstdio.h>

#define _return_if_locked(store, return_code)            \
    if(store->db_is_locked) {                            \
        if (store->wait_for_db_finish == false) {        \
            return return_code;                          \
        } else {                                         \
            g_rec_mutex_lock (&store->db_update_lock);   \
            g_rec_mutex_unlock (&store->db_update_lock); \
        }                                                \
    }                                                    \
     
#define return_if_locked(store) \
    _return_if_locked (store, ;)

#define return_val_if_locked(store, val) \
    _return_if_locked (store, val)


/*
 * Will return true, if the database located on disk is still valid.
 * Checks include:
 *
 *  #1 check if db path is accessible and we have read permissions.
 *  #2 try to open the database (no :memory: connection)
 *  #3 check if hostname/port matches the current one.
 *  #4 check if db_version and sc_version are equal.
 *
 *  - in order to use the old database all checks must succeed.
 *  - there may not be a connection open already on the store!
 *
 * Returns: the number of songs in the songs table, or -1 on failure.
 */
static int mc_store_check_if_db_is_still_valid (mc_StoreDB * self, const char * db_path)
{
    /* result */
    int song_count = -1;

    /* result of check #1 */
    bool exist_check = FALSE;

    char * actual_db_path = (char *) db_path;

    /* check #1 */
    if (g_file_test (db_path, G_FILE_TEST_IS_REGULAR) == TRUE) {
        exist_check = TRUE;
    } else if (self->settings->use_compression) {
        char * zip_path = g_strdup_printf ("%s%s", db_path, MC_GZIP_ENDING);
        exist_check = g_file_test (zip_path, G_FILE_TEST_IS_REGULAR);

        GTimer * zip_timer = g_timer_new();
        if ( (exist_check = mc_gunzip (zip_path) ) == false)
            mc_shelper_report_progress (self->client, true, "database: Unzipping %s failed.", zip_path);
        else
            mc_shelper_report_progress (self->client, true, "database: Unzipping %s done (took %2.3fs).",
                                        zip_path, g_timer_elapsed (zip_timer, NULL) );

        g_timer_destroy (zip_timer);
        g_free (zip_path);
    }

    if (exist_check) {
        /* check #2 */
        if (sqlite3_open (actual_db_path, &self->handle) == SQLITE_OK) {
            /* needed in order to select metadata */
            mc_stprv_create_song_table (self);
            mc_stprv_prepare_all_statements (self);

            /* check #3 */
            char * cached_hostname = mc_stprv_get_mpd_host (self);
            int cached_port = mc_stprv_get_mpd_port (self);

            if (cached_port == self->client->_port &&
                g_strcmp0 (cached_hostname, self->client->_host) == 0) {
                /* check #4 */
                size_t cached_db_version = mc_stprv_get_db_version (self);
                size_t cached_sc_version = mc_stprv_get_sc_version (self);

                if (cached_db_version == mpd_stats_get_db_update_time (self->client->stats) &&
                    cached_sc_version == MC_DB_SCHEMA_VERSION) {
                    song_count = mc_stprv_get_song_count (self);
                }
            }

            mc_stprv_close_handle (self);

            g_free (cached_hostname);
            self->handle = NULL;
        }
    }

    return song_count;
}

///////////////

/* Perhaps a bit hacky, but NULL is not accepted by GAsyncQueue,
 * and 0x1 seemed much simpler than extra allocating
 * a new dummy mpd_song. */
const gpointer async_queue_terminator = (gpointer) 0x1;

///////////////

/* Hopefully we can exchange this by using
 * only filename/pos/id instead of getting a full featured song,
 * and throwing away most of it */
static gpointer mc_store_do_plchanges_sql_thread (mc_StoreDB * self)
{
    g_assert (self);

    mpd_song * song = NULL;

    /* start a transaction */
    mc_stprv_begin (self);

    GTimer * timer = g_timer_new ();
    gdouble clip_time = 0.0, posid_time = 0.0, stack_time = 0.0;

    /* set the queue_update column all to 0
     * Other queue functions set it back to 1 */
    //mc_stprv_queue_clear_update_flag (self);

    while ( (gpointer) (song = g_async_queue_pop (self->sqltonet_queue) ) != async_queue_terminator) {
        mc_stprv_queue_update_posid (self, mpd_song_get_pos (song), mpd_song_get_id (song), mpd_song_get_uri (song) );
        mpd_song_free (song);
    }
    posid_time = g_timer_elapsed (timer, NULL);


    /* Clip off songs that were deleted at the end of the queue */
    g_timer_start (timer);
    int clipped = mc_stprv_queue_clip (self, mpd_status_get_queue_length (self->client->status) );
    if (clipped > 0) {
        mc_shelper_report_progress (self->client, true, "database: Clipped %d songs at end of playlist.", clipped);
    }
    clip_time = g_timer_elapsed (timer, NULL);

    /* Commit all those update statements */
    mc_stprv_commit (self);

    /* Update the pos/id data in the song stack (=> sync with db) */
    g_timer_start (timer);
    mc_stprv_queue_update_stack_posid (self);
    stack_time = g_timer_elapsed (timer, NULL);

    g_timer_destroy (timer);

    mc_shelper_report_progress (self->client, true, "database: QueueSQL Timing: %2.3fs Clip | %2.3fs Posid | %2.3fs Stack",
                                clip_time, posid_time, stack_time);

    return NULL;
}

///////////////

static void mc_store_do_plchanges (mc_StoreDB * store, bool lock_self)
{
    g_assert (store);

    mc_Client * self = store->client;
    GThread * sql_thread = NULL;
    GTimer * timer = NULL;
    int progress_counter = 0;
    size_t last_pl_version = 0;

    if (lock_self) {
        LOCK_UPDATE_MTX (store);
    }

    /* get last version of the queue (which we're having already. */
    last_pl_version = mc_stprv_get_pl_version (store);

    if (last_pl_version == mpd_status_get_queue_version (store->client->status)
        && store->need_full_queue == false) {
        mc_shelper_report_progress (self, true,
                                    "database: Will not update queue, version didn't change (%d == %d, forced: %s)",
                                    (int) last_pl_version, mpd_status_get_queue_version (store->client->status),
                                    store->need_full_queue ? "Yes" : "No");

        if (lock_self) {
            UNLOCK_UPDATE_MTX (store);
        }
        return;
    }

    /* need_full_queue basically means that we threw away the db lately,
     * so we need to update every song's pos/id information */
    if (store->need_full_queue)
        last_pl_version = 0;

    /* needs to be started after inserting meta attributes, since it calls 'begin;' */
    sql_thread = g_thread_new ("queue-update", (GThreadFunc) mc_store_do_plchanges_sql_thread, store);

    /* timing */
    timer = g_timer_new();

    /* Now do the actual hard work, send the actual plchanges command,
     * loop over the retrieved contents, and push them to the SQL Thread */
    BEGIN_COMMAND {
        g_timer_start (timer);

        mc_shelper_report_progress (self, true, "database: Queue was updated. Will do ,,plchanges %d''", (int) last_pl_version);
        if (mpd_send_queue_changes_meta (conn, last_pl_version) ) {
            mpd_song * song = NULL;
            while ( (song = mpd_recv_song (conn) ) != NULL) {
                g_async_queue_push (store->sqltonet_queue, song);
                if (progress_counter++ % 50 == 0)
                    mc_shelper_report_progress (self, false, "database: receiving queue contents ... [%d/%d]",
                    progress_counter, mpd_status_get_queue_length (store->client->status) );
            }
        }

        /* Killing the SQL thread softly. */
        g_async_queue_push (store->sqltonet_queue, async_queue_terminator);
        g_thread_join (sql_thread);
    }
    END_COMMAND

    if (lock_self) {
        UNLOCK_UPDATE_MTX (store);
    }

    /* we updated the queue fully, so next time we want
     * to use a more recent pl version */
    store->need_full_queue = FALSE;

    /* Since we modify data in the db,
     * we gonna need to save the changes
     * to disk later.
     */
    store->write_to_disk = TRUE;

    /* a bit of timing report */
    mc_shelper_report_progress (self, true, "database: updated %d song's pos/id (took %2.3fs)",
                                progress_counter, g_timer_elapsed (timer, NULL) );

    g_timer_destroy (timer);
}

///////////////

static gpointer mc_store_do_list_all_info_sql_thread (gpointer user_data)
{
    mc_StoreDB * self = user_data;
    struct mpd_entity * ent = NULL;

    g_assert (self);

    mc_stprv_begin (self);

    while ( (gpointer) (ent = g_async_queue_pop (self->sqltonet_queue) ) > async_queue_terminator) {
        switch (mpd_entity_get_type (ent) ) {
        case MPD_ENTITY_TYPE_SONG: {
            struct mpd_song * song = (struct mpd_song *) mpd_entity_get_song (ent) ;
            mc_stack_append (self->stack, (mpd_song *) song);
            mc_stprv_insert_song (self, (mpd_song *) song);

            /* Not sure if this is a nice way,
             * but for now it works. There might be a proper way:
             * duplicate the song with mpd_song_dup, and use mpd_entity_free,
             * but that adds extra memory usage, and costs about 0.2 seconds. */
            g_free (ent);
        }
        break;
        case MPD_ENTITY_TYPE_DIRECTORY: {
            const struct mpd_directory * dir = mpd_directory_dup (mpd_entity_get_directory (ent) );
            if (dir != NULL) {
                mc_stprv_dir_insert (self, g_strdup (mpd_directory_get_path (dir) ) );
                mpd_entity_free (ent);
            }
        }
        break;
        case MPD_ENTITY_TYPE_PLAYLIST: {
            const struct mpd_playlist * pl = mpd_entity_get_playlist (ent);
            if (pl != NULL) {
                mc_stack_append (self->spl.stack, (struct mpd_playlist *) pl);
            }
            g_free (ent);
        }
        break;
        default: {
            mpd_entity_free (ent);
            ent = NULL;
        }
        break;
        }
    }
    mc_stprv_commit (self);
    return NULL;
}

///////////////

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
 *
 * lock_self: lock db while update. (Otherwise do it yourself!)
 */
static void mc_store_do_list_all_info (mc_StoreDB * store, bool lock_self)
{
    g_assert (store);
    g_assert (store->client);

    mc_Client * self = store->client;
    int progress_counter = 0;
    int number_of_songs = mpd_stats_get_number_of_songs (self->stats);
    size_t db_version = 0;

    GTimer * timer = NULL;
    GThread * sql_thread = NULL;

    if (lock_self) {
        LOCK_UPDATE_MTX (store);
    }

    db_version = mc_stprv_get_db_version (store);
    if (store->need_full_db == false && mpd_stats_get_db_update_time (self->stats) == db_version) {
        mc_shelper_report_progress (self, true,
                                    "database: Will not update database, timestamp didn't change.");
        return;
    }

    /* If we tried to stop a listallinfo,
     * but none was running, the flag will still be true.
     * So we set it here to false again, at the risk
     * that we may do a listallinfo too much in very rare cases. */
    store->stop_listallinfo = FALSE;

    /* We're building the whole table new,
     * so throw away old data */
    mc_stprv_delete_songs_table (store);
    mc_stprv_dir_delete (store);

    /* Also throw away current stack, and reallocate it
     * to the fitting size.
     *
     * Apparently GPtrArray does not support reallocating,
     * without adding NULL-cells to the array? */
    if (store->stack != NULL) {
        mc_stack_free (store->stack);
    }

    store->stack = mc_stack_create (
                       mpd_stats_get_number_of_songs (self->stats) + 1,
                       (GDestroyNotify) mpd_song_free);


    /* Profiling */
    timer = g_timer_new();
    sql_thread = g_thread_new ("sql-thread", mc_store_do_list_all_info_sql_thread, store);

    BEGIN_COMMAND {
        struct mpd_entity * ent = NULL;

        g_timer_start (timer);

        /* Order the real big list */
        mpd_send_list_all_meta (conn, "/");

        while ( (ent = mpd_recv_entity (conn) ) != NULL) {
            if (++progress_counter % 50 == 0)
                mc_shelper_report_progress (self, false, "database: retrieving entities from mpd ... [%d/%d]",
                progress_counter, number_of_songs);

            g_async_queue_push (store->sqltonet_queue, ent);

            if (store->stop_listallinfo == TRUE) {
                mc_shelper_report_progress (self, true, "database: stopped, attempting more recent update.");
                break;
            }
        }
    }
    END_COMMAND;

    /* tell SQL thread kindly to die, but wait for him to bleed */
    g_async_queue_push (store->sqltonet_queue, async_queue_terminator);
    g_thread_join (sql_thread);

    mc_shelper_report_progress (self, true, "database: retrieved %d songs from mpd (took %2.3fs)",
                                number_of_songs, g_timer_elapsed (timer, NULL) );

    g_timer_destroy (timer);

    if (lock_self) {
        UNLOCK_UPDATE_MTX (store);
    }

    /* we got the full db, next time check again if it's needed */
    store->need_full_db = FALSE;

    /* we got stopped, but we don't want the next listallinfo to be stopped */
    store->stop_listallinfo = FALSE;

    /* If we do a plchanges now, we gonna need the full queue */
    store->need_full_queue = TRUE;
}

///////////////

static gpointer mc_store_do_plchanges_wrapper (mc_StoreDB * self)
{
    g_assert (self);

    mc_store_do_plchanges (self, true);
    mc_stprv_insert_meta_attributes (self);

    g_thread_unref (g_thread_self () );
    return NULL;
}

///////////////

static void mc_store_do_plchanges_bkgd (mc_StoreDB * self)
{
    g_assert (self);

    g_thread_new ("db-queue-update-thread",
                  (GThreadFunc) mc_store_do_plchanges_wrapper, self);
}

///////////////

static gpointer mc_store_do_listall_and_plchanges (mc_StoreDB * self)
{
    g_assert (self);

    self->stop_listallinfo = TRUE;
    LOCK_UPDATE_MTX (self);
    mc_store_do_list_all_info (self, false);
    mc_store_do_plchanges (self, false);
    mc_stprv_spl_update (self);
    mc_stprv_insert_meta_attributes (self);
    UNLOCK_UPDATE_MTX (self);
    g_thread_unref (g_thread_self () );
    return NULL;
}

///////////////

static void mc_store_do_listall_and_plchanges_bkgd (mc_StoreDB * self)
{
    g_assert (self);

    g_thread_new ("db-listall-and-plchanges",
                  (GThreadFunc) mc_store_do_listall_and_plchanges, self);
}

///////////////

static gpointer mc_store_deserialize_songs_and_plchanges (mc_StoreDB * self)
{
    g_assert (self);

    LOCK_UPDATE_MTX (self);
    mc_stprv_deserialize_songs (self, false);
    mc_store_do_plchanges (self, false);
    mc_stprv_spl_update (self);
    mc_stprv_insert_meta_attributes (self);
    UNLOCK_UPDATE_MTX (self);
    g_thread_unref (g_thread_self () );
    return NULL;
}

static void mc_store_deserialize_songs_and_plchanges_bkgd (mc_StoreDB * self)
{
    g_assert (self);

    g_thread_new ("db-deserialize-and-plchanges-thread",
                  (GThreadFunc) mc_store_deserialize_songs_and_plchanges, self);
}

///////////////

static gpointer mc_stprv_spl_update_wrapper (mc_StoreDB * self)
{
    mc_stprv_spl_update (self);
    g_thread_unref (g_thread_self () );
    return NULL;
}

static void mc_stprv_spl_update_bkgd (mc_StoreDB * self)
{
    g_assert (self);

    g_thread_new ("db-spl-update-bkgd", 
            (GThreadFunc) mc_stprv_spl_update_wrapper, self);
}

///////////////

/*
 * On DB update we have no chance, but updating the whole tables.
 * (we do not need to update, if db version did not changed though.)
 *
 * #1 If events contains MPD_IDLE_DATABASE
 *    #2 If db_version changed
 *       #3 Insert new database metadata
 *       #4 DELETE * FROM songs;
 *       #5 clear previous contents of song stack.
 *       #6 Update song metadata (i.e. read stuff from mpd)
 *       #7 Since the data is fresher than what might be on disk, we'll write it there later.
 *
 */
void mc_store_update_callback (mc_Client * client, enum mpd_idle events, mc_StoreDB * self)
{
    g_assert (self && client && self->client == client);

    if (events & MPD_IDLE_DATABASE) {
        mc_store_do_listall_and_plchanges_bkgd (self);
    } else if (events & MPD_IDLE_QUEUE) {
        mc_store_do_plchanges_bkgd (self);
    } else if (events & MPD_IDLE_STORED_PLAYLIST) {
        mc_stprv_spl_update_bkgd (self);
    }

    /* needs to happen in both cases (post) */
    if (events & (MPD_IDLE_QUEUE | MPD_IDLE_DATABASE | MPD_IDLE_STORED_PLAYLIST) ) {
        self->write_to_disk = TRUE;
    }
}

///////////////

static void mc_store_connectivity_callback (
    mc_Client * client,
    bool server_changed,
    mc_StoreDB * self)
{
    g_assert (self && client && self->client == client);

    if (mc_proto_is_connected (client) ) {
        if (server_changed) {
            self->need_full_db = TRUE;
            self->need_full_queue = TRUE;
        }
        mc_store_update_callback (client, MPD_IDLE_DATABASE, self);
    }
}

///////////////

static char * mc_store_construct_full_dbpath (mc_Client * client, const char * directory)
{
    g_assert (client);

    return g_strdup_printf ("%s%cmoosecat_%s:%d.sqlite", (directory) ? directory : ".",
                            G_DIR_SEPARATOR, client->_host, client->_port);
}

///////////////
/// PUBLIC ////
///////////////

mc_StoreDB * mc_store_create (mc_Client * client, mc_StoreSettings * settings)
{
    g_assert (client);

    /* allocated memory for the mc_StoreDB struct */
    mc_StoreDB * store = g_new0 (mc_StoreDB, 1);

    /* Settings */
    store->settings = (settings) ? settings : mc_store_settings_new ();

    /* either songs in 'songs' table or -1 on error */
    int song_count = -1;

    /* create the full path to the db */
    char * db_path = mc_store_construct_full_dbpath (client, store->settings->db_directory);

    /* init the background mutex */
    g_rec_mutex_init (&store->db_update_lock);
    store->db_is_locked = FALSE;

    /* client is used to keep the db content updated */
    store->client = client;

    /* do not wait by default */
    store->wait_for_db_finish = false;

    /* used to exchange songs between network <-> sql threads */
    store->sqltonet_queue = g_async_queue_new ();

    if ( (song_count = mc_store_check_if_db_is_still_valid (store, db_path) ) < 0) {
        mc_shelper_report_progress (store->client, true, "database: will fetch stuff from mpd.");

        /* open a sqlite handle, pointing to a database, either a new one will be created,
         * or an backup will be loaded into memory */
        mc_strprv_open_memdb (store);
        mc_stprv_prepare_all_statements (store);

        /* make sure we inserted the meta info at least once */
        mc_stprv_insert_meta_attributes (store);

        /* the database is new, so no pos/id information is there yet */
        store->need_full_queue = TRUE;
        store->need_full_db = TRUE;

        /* It's new, so replace the one on the disk, if it's there */
        store->write_to_disk = TRUE;

        /* we need to query mpd, update db && queue info */
        mc_store_do_listall_and_plchanges_bkgd (store);
    } else {
        /* open a sqlite handle, pointing to a database, either a new one will be created,
         * or an backup will be loaded into memory */
        mc_strprv_open_memdb (store);
        mc_stprv_prepare_all_statements (store);
        mc_shelper_report_progress (store->client, true, "database: %s exists already.", db_path);

        /* stack is allocated to the old size */
        store->stack = mc_stack_create (song_count + 1, (GDestroyNotify) mpd_song_free);

        /* load the old database into memory */
        mc_stprv_load_or_save (store, false, db_path);

        /* try to keep the old database,
         * saves write-time, and might reduce bugs,
         * when songs are deserialized wrong
         */
        store->write_to_disk = FALSE;

        /* the database is created from possibly valid data,
         *  so we might only need update it a bit */
        store->need_full_queue = FALSE;
        store->need_full_db = FALSE;

        /* Update queue information in bkgd */
        mc_store_deserialize_songs_and_plchanges_bkgd (store);
    }

    mc_signal_add_masked (
        &store->client->_signals,
        "client-event", true, /* call first */
        (mc_ClientEventCallback) mc_store_update_callback, store,
        MPD_IDLE_DATABASE | MPD_IDLE_QUEUE | MPD_IDLE_STORED_PLAYLIST);

    mc_signal_add (
        &store->client->_signals,
        "connectivity", true, /* call first */
        (mc_ConnectivityCallback) mc_store_connectivity_callback, store);

    /* Playlist support */
    mc_stprv_spl_init (store);

    g_free (db_path);

    return store;
}

///////////////

/*
 * close everything down.
 *
 * #1 remove the update watcher (might be called still otherwise, if client lives longer than the store)
 * #2 free the stack and all associated memory.
 * #3 Save database dump to disk if it hadn't been read from there.
 * #4 Close the handle.
 */
void mc_store_close (mc_StoreDB * self)
{
    if (self == NULL)
        return;

    return_if_locked (self);

    mc_proto_signal_rm (self->client, "client-event", mc_store_update_callback);
    mc_proto_signal_rm (self->client, "connectivity", mc_store_connectivity_callback);
    mc_stack_free (self->stack);
    mc_stprv_spl_destroy (self);

    char * full_path = mc_store_construct_full_dbpath (self->client, self->db_directory);
    if (self->write_to_disk)
        mc_stprv_load_or_save (self, true, full_path);

    if (self->settings->use_compression && mc_gzip (full_path) == false)
        g_print ("Note: Nothing to zip.\n");

    mc_stprv_close_handle (self);

    if (self->settings->use_memory_db == false)
        g_unlink (MC_STORE_TMP_DB_PATH);

    mc_store_settings_destroy (self->settings);
    g_rec_mutex_clear (&self->db_update_lock);
    g_async_queue_unref (self->sqltonet_queue);
    g_free (self->db_directory);
    g_free (full_path);
    g_free (self);
}

///////////////

int mc_store_search_out ( mc_StoreDB * self, const char * match_clause, bool queue_only, mpd_song ** song_buffer, int buf_len)
{
    /* we should not operate on changing data */
    return_val_if_locked (self, -1);

    int rc = mc_stprv_select_to_buf (self, match_clause, queue_only, song_buffer, buf_len);

    return rc;
}

///////////////

void mc_store_set_wait_mode (mc_StoreDB * self, bool wait_for_db_finish)
{
    g_assert (self);
    self->wait_for_db_finish = wait_for_db_finish;
}

///////////////

void mc_store_playlist_load (mc_StoreDB * self, const char * playlist_name)
{
    mc_stprv_spl_load_by_playlist_name (self, playlist_name);
}

///////////////

int mc_store_playlist_select_to_stack (mc_StoreDB * self, mc_Stack * stack, const char * playlist_name, const char * match_clause) 
{
    return mc_stprv_spl_select_playlist (self, stack, playlist_name, match_clause);
}

///////////////

int mc_store_playlist_get_all_loaded (mc_StoreDB * self, mc_Stack * stack)
{
    return mc_stprv_spl_get_loaded_playlists (self, stack);
}

///////////////

int mc_store_dir_select_to_stack (mc_StoreDB * self, mc_Stack * stack, const char * directory, int depth)
{
    return mc_stprv_dir_select_to_stack (self, stack, directory, depth);
}

///////////////

struct mpd_song * mc_store_song_at (mc_StoreDB * self, int idx)
{
    g_assert (self);
    g_assert (idx >= 0);

    return (struct mpd_song *)mc_stack_at (self->stack, idx);
}

///////////////

int mc_store_total_songs (mc_StoreDB * self)
{
    g_assert (self);

    return mc_stack_length (self->stack);
}
