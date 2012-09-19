#include "db.h"
#include "../mpd/client_private.h"
#include "../mpd/signal_helper.h"

/* macro to exit early if db is currently locked */
#define return_if_locked(store, return_code)             \
    if(store->db_is_locked) {                            \
        if (store->wait_for_db_finish == false) {        \
            return return_code;                          \
        } else {                                         \
            g_rec_mutex_lock (&store->db_update_lock);   \
            g_rec_mutex_unlock (&store->db_update_lock); \
        }                                                \
    }                                                    \
     
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
static int mc_store_check_if_db_is_still_valid (mc_StoreDB * self)
{
    /* result */
    int song_count = -1;

    /* check #1 */
    if (g_file_test (self->db_path, G_FILE_TEST_IS_REGULAR) == TRUE) {
        /* check #2 */
        if (sqlite3_open (self->db_path, &self->handle) == SQLITE_OK) {
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

            int sqlite_err = 0;
            mc_stprv_finalize_all_statements (self);
            if ( (sqlite_err = sqlite3_close (self->handle) ) != SQLITE_OK)
                g_print ("Warning: Unable to close db connection: #%d: %s\n",
                         sqlite_err, sqlite3_errmsg (self->handle) );

            g_free (cached_hostname);
            self->handle = NULL;
        }
    }

    return song_count;
}

///////////////

const gpointer async_queue_terminator = (gpointer) 0x1;

///////////////

static gpointer mc_store_do_plchanges_sql_thread (mc_StoreDB * self)
{
    mpd_song * song = NULL;

    /* BEGIN IMMEDIATE; */
    mc_stprv_begin (self);

    while ( (gpointer) (song = g_async_queue_pop (self->listalltosql_queue) ) > async_queue_terminator) {
        mc_stprv_queue_update_posid (self,
                                     mpd_song_get_pos (song),
                                     mpd_song_get_id (song),
                                     mpd_song_get_uri (song)
                                    );
        mpd_song_free (song);
    }

    /* COMMIT; */
    mc_stprv_commit (self);

    return NULL;
}

///////////////

static gpointer mc_store_do_plchanges (mc_StoreDB * store)
{
    mc_Client * self = store->client;
    g_assert (self != NULL);

    /* get last version of the queue (which we're having already. */
    size_t last_pl_version = mc_stprv_get_pl_version (store);

    if (last_pl_version == mpd_status_get_queue_version (store->client->status) && store->need_full_queue == false) {
        mc_shelper_report_progress (self, true, "database: Will not update queue, version didn't change.");
        return NULL;
    }

    if (store->need_full_queue)
        last_pl_version = 0;

    /* update info in the db */
    mc_stprv_insert_meta_attributes (store);
    store->need_full_queue = FALSE;

    /* needs to be started after inserting meta attributes, since it calls 'begin;' */
    GThread * sql_thread = g_thread_new ("queue-update", (GThreadFunc) mc_store_do_plchanges_sql_thread, store);

    /* Profiling */
    GTimer * timer = g_timer_new();
    int progress_counter = 0;

    /* Since we modify data in the db,
     * we gonna need to save the changes
     * to disk later.
     */
    store->write_to_disk = TRUE;

    BEGIN_COMMAND {
        g_timer_start (timer);

        mc_shelper_report_progress (self, true, "database: Will do ,,plchanges %d''", (int) last_pl_version);
        if (mpd_send_queue_changes_meta (conn, last_pl_version) ) {
            mpd_song * song = NULL;
            while ( (song = mpd_recv_song (conn) ) != NULL) {
                g_async_queue_push (store->listalltosql_queue, song);
                if (progress_counter++ % 50 == 0)
                    mc_shelper_report_progress (self, false, "database: receiving queue contents ... [%d/%d]",
                    progress_counter, mpd_status_get_queue_length (store->client->status) );
            }
        }
    }
    END_COMMAND

    g_async_queue_push (store->listalltosql_queue, async_queue_terminator);
    g_thread_join (sql_thread);

    mc_shelper_report_progress (self, true, "database: updated %d song's pos/id (took %2.3fs)",
                                progress_counter, g_timer_elapsed (timer, NULL) );

    return NULL;
}

///////////////

static gpointer mc_store_do_list_all_info_sql_thread (gpointer user_data)
{
    mc_StoreDB * self = user_data;
    mpd_song * song = NULL;

    /* BEGIN IMMEDIATE; */
    mc_stprv_begin (self);

    while ( (gpointer) (song = g_async_queue_pop (self->listalltosql_queue) ) > async_queue_terminator) {
        mc_store_stack_append (self->stack, (mpd_song *) song);
        mc_stprv_insert_song (self, (mpd_song *) song);
    }

    /* COMMIT; */
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
 */
static gpointer mc_store_do_list_all_info (mc_StoreDB * store)
{
    mc_Client * self = store->client;
    g_assert (self != NULL);

    store->db_is_locked = TRUE;
    g_rec_mutex_lock (&store->db_update_lock);

    /* progress */
    int progress_counter = 0;

    int number_of_songs = mpd_stats_get_number_of_songs (self->stats);

    /* Profiling */
    GTimer * timer = g_timer_new();
    GThread * sql_thread = g_thread_new ("sql-thread", mc_store_do_list_all_info_sql_thread, store);

    BEGIN_COMMAND {
        struct mpd_entity * ent = NULL;

        g_timer_start (timer);

        /* Order the real big list */
        mpd_send_list_all_meta (conn, "/");

        while ( (ent = mpd_recv_entity (conn) ) != NULL) {
            if (mpd_entity_get_type (ent) == MPD_ENTITY_TYPE_SONG) {
                if (++progress_counter % 50 == 0)
                    mc_shelper_report_progress (self, false, "database: retrieving songs from mpd ... [%d/%d]",
                    progress_counter, number_of_songs);

                g_async_queue_push (store->listalltosql_queue,
                (gpointer) mpd_entity_get_song (ent) );
            } else {
                mpd_entity_free (ent);
                ent = NULL;
            }
        }
    }
    END_COMMAND;

    /* tell SQL thread kindly to die, but wait for him to bleed */
    g_async_queue_push (store->listalltosql_queue, async_queue_terminator);
    g_thread_join (sql_thread);

    mc_shelper_report_progress (self, true, "database: retrieved %d songs from mpd (took %2.3fs)",
                                number_of_songs, g_timer_elapsed (timer, NULL) );

    mc_store_do_plchanges (store);

    store->db_is_locked = FALSE;
    g_rec_mutex_unlock (&store->db_update_lock);

    return NULL;
}

///////////////

/*
 * Same as mc_store_do_list_all_info, but starts job in background.
 * Locking is done in mc_store_do_list_all_info itself.
 */
static void mc_store_do_list_all_info_bkgd (mc_StoreDB * self)
{
    g_thread_new ("db-update-thread", (GThreadFunc) mc_store_do_list_all_info, self);
}

///////////////

static void mc_store_do_plchanges_bkgd (mc_StoreDB * self)
{
    g_thread_new ("db-queue-update-thread", (GThreadFunc) mc_store_do_plchanges, self);
}

///////////////

/*
 * On DB update we have no chance, but updating the whole tables.
 * (we do not need to update, if db version did not changed though.)
 *
 * TODO: make sure this is called as first callback.
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
static void mc_store_update_callback (
    mc_Client * client,
    enum mpd_idle events,
    void * user_data)
{
    mc_StoreDB * self = user_data;

    g_assert (self != NULL);

    if (events & MPD_IDLE_DATABASE) {
        gsize db_version = mc_stprv_get_db_version (self);
        if (mpd_stats_get_db_update_time (client->stats) > db_version) {
            /* #3, #4 */
            mc_stprv_insert_meta_attributes (self);
            mc_stprv_delete_songs_table (self);

            /* #5 */
            mc_store_stack_free (self->stack);
            self->stack = mc_store_stack_create (
                              mpd_stats_get_number_of_songs (client->stats) + 1,
                              (GDestroyNotify) mpd_song_free);

            /* #6 */
            mc_store_do_list_all_info_bkgd (self);

            /* #7 */
            self->write_to_disk = TRUE;
        }
    }

    if (events & MPD_IDLE_QUEUE) {
        mc_store_do_plchanges_bkgd (self);
        self->write_to_disk = TRUE;
    }
}

///////////////
/// PUBLIC ////
///////////////

mc_StoreDB * mc_store_create (mc_Client * client, const char * directory, const char * dbname)
{
    /* allocated memory for the mc_StoreDB struct */
    mc_StoreDB * store = g_new0 (mc_StoreDB, 1);
    g_assert (store);
    g_assert (client);

    /* init the background mutex */
    g_rec_mutex_init (&store->db_update_lock);
    store->db_is_locked = FALSE;

    /* client is used to keep the db content updated */
    store->client = client;

    if (dbname == NULL)
        dbname = "moosecat.db";

    if (directory == NULL)
        directory = ".";

    /* do not wait by default */
    store->wait_for_db_finish = false;

    /* create the full path to the db */
    store->db_path = g_strjoin (G_DIR_SEPARATOR_S, directory, dbname, NULL);

    /* used to exchange songs between network <-> sql threads */
    store->listalltosql_queue = g_async_queue_new ();

    int song_count = -1;

    if ( (song_count = mc_store_check_if_db_is_still_valid (store) ) < 0) {
        mc_shelper_report_progress (store->client, true, "database: will fetch stuff from mpd.");

        /* open a sqlite handle, pointing to a database, either a new one will be created,
         * or an backup will be loaded into memory */
        mc_strprv_open_memdb (store);
        mc_stprv_prepare_all_statements (store);

        /* stack is preallocated to fit the whole db in */
        store->stack = mc_store_stack_create (
                           mpd_stats_get_number_of_songs (client->stats) + 1,
                           (GDestroyNotify) mpd_song_free
                       );

        /* we need to query mpd */
        mc_store_do_list_all_info_bkgd (store);

        /* It's new, so replace the one on the disk, if it's there */
        store->write_to_disk = TRUE;

        /* the database is new, so no pos/id information is there yet */
        store->need_full_queue = TRUE;
    } else {
        /* open a sqlite handle, pointing to a database, either a new one will be created,
         * or an backup will be loaded into memory */
        mc_strprv_open_memdb (store);
        mc_stprv_prepare_all_statements (store);
        mc_shelper_report_progress (store->client, true, "database: %s exists already.", store->db_path);

        /* stack is allocated to the old size */
        store->stack = mc_store_stack_create (song_count + 1, (GDestroyNotify) mpd_song_free);

        /* load the old database into memory */
        mc_stprv_load_or_save (store, false);

        /* deserialize all songs from there */
        mc_stprv_deserialize_songs (store);

        /* try to keep the old database,
         * saves write-time, and might reduce bugs,
         * when songs are deserialized wrong
         */
        store->write_to_disk = FALSE;

        /* the database is new, so no pos/id information is there yet */
        store->need_full_queue = FALSE;
    }

    /* be ready for db updates */
    mc_proto_signal_add_masked (
        store->client, "client-event",
        mc_store_update_callback, store,
        MPD_IDLE_DATABASE | MPD_IDLE_QUEUE);

    return store;
}

///////////////

/*
 * close everything down.
 *
 * #1 remove the update watcher (might be called sill otherwise, if client lives longer than the store)
 * #2 kill all sqlite3_stmt objects
 * #3 free the stack and all associated memory.
 * #4 Save database dump to disk if it hadn't been read from there.
 * #5 Close the handle.
 */
void mc_store_close (mc_StoreDB * self)
{
    if (self == NULL)
        return;

    mc_proto_signal_rm (self->client, "client-event", mc_store_update_callback);
    mc_stprv_finalize_all_statements (self);
    mc_store_stack_free (self->stack);

    if (self->write_to_disk)
        mc_stprv_load_or_save (self, true);

    g_free (self->db_path);

    int sqlite_err = SQLITE_OK;
    if ( (sqlite_err = sqlite3_close (self->handle) ) != SQLITE_OK)
        g_print ("Warning: Unable to close db connection: #%d: %s\n",
                 sqlite_err, sqlite3_errmsg (self->handle) );

    g_free (self);
}

///////////////

int mc_store_search_out ( mc_StoreDB * self, const char * match_clause, mpd_song ** song_buffer, int buf_len)
{
    /* we should not operate on changing data */
    return_if_locked (self, -1);

    return mc_stprv_select_out (self, match_clause, song_buffer, buf_len);
}

///////////////

/*
 * Try to acquire the mutex,
 * and release it short-after
 *
 * TODO: this is silly. Between function return and lock,
 *       lock might be acquired again.
 *
 *       Introduce a store_wait_mode API, so functions
 *       that need the lock either return immediately,
 *       or lock till death.
 */
void mc_store_wait (mc_StoreDB * self, bool wait_for_db_finish)
{
    g_assert (self);
    self->wait_for_db_finish = wait_for_db_finish;
}
