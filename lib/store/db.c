#include "db.h"
#include "../mpd/client_private.h"
#include "../mpd/signal_helper.h"
#include "../mpd/signal.h"

#include <glib.h>

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
 * and throwing away most of it
 */
static gpointer mc_store_do_plchanges_sql_thread (mc_StoreDB * self)
{
    mpd_song * song = NULL;

    /* BEGIN IMMEDIATE; */
    mc_stprv_begin (self);

    while ( (gpointer) (song = g_async_queue_pop (self->sqltonet_queue) ) > async_queue_terminator) {
        mc_stprv_queue_update_posid (self,
                                     mpd_song_get_pos (song),
                                     mpd_song_get_id (song),
                                     mpd_song_get_uri (song)
                                    );

        mpd_song_free (song);
    }

    /* COMMIT; */
    mc_stprv_commit (self);

    mc_stprv_queue_update_stack_posid (self);

    return NULL;
}

///////////////

static void mc_store_do_plchanges (mc_StoreDB * store, bool lock_self)
{
    mc_Client * self = store->client;
    g_assert (self != NULL);

    if (lock_self) {
        LOCK_UPDATE_MTX (store);
    }

    /* get last version of the queue (which we're having already. */
    size_t last_pl_version = mc_stprv_get_pl_version (store);

    if (last_pl_version == mpd_status_get_queue_version (store->client->status) && store->need_full_queue == false) {
        mc_shelper_report_progress (self, true,
                "database: Will not update queue, version didn't change. (%d == %d)", 
                 (int)last_pl_version, mpd_status_get_queue_version (store->client->status));
    
        if (lock_self) {
            UNLOCK_UPDATE_MTX (store);
        }
        return;
    }

    if (store->need_full_queue)
        last_pl_version = 0;

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
    }
    END_COMMAND

    g_async_queue_push (store->sqltonet_queue, async_queue_terminator);
    g_thread_join (sql_thread);

    if (lock_self) {
        UNLOCK_UPDATE_MTX (store);
    }

    mc_shelper_report_progress (self, true, "database: updated %d song's pos/id (took %2.3fs)",
                                progress_counter, g_timer_elapsed (timer, NULL) );
}

///////////////

static gpointer mc_store_do_list_all_info_sql_thread (gpointer user_data)
{
    mc_StoreDB * self = user_data;
    mpd_song * song = NULL;

    /* BEGIN IMMEDIATE; */
    mc_stprv_begin (self);

    while ( (gpointer) (song = g_async_queue_pop (self->sqltonet_queue) ) > async_queue_terminator) {
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
 *
 * lock_self: lock db while update. (Otherwise do it yourself!)
 */
static void mc_store_do_list_all_info (mc_StoreDB * store, bool lock_self)
{
    g_assert (store);
    mc_Client * self = store->client;
    g_assert (self != NULL);

    if (lock_self) {
        LOCK_UPDATE_MTX (store);
    }
        
    size_t db_version = mc_stprv_get_db_version (store);
    g_print("DB VERSION: %d == %d?\n", (int)db_version, (int)mpd_stats_get_db_update_time (self->stats));
    if (store->need_full_db == false && mpd_stats_get_db_update_time (self->stats) == db_version) {
        mc_shelper_report_progress (self, true,
                "database: Will not update database, timestamp didn't change.");
        return;
    }

    mc_stprv_delete_songs_table (store);

    if (store->stack != NULL) {
        mc_store_stack_free (store->stack);
    }

    store->stack = mc_store_stack_create (
            mpd_stats_get_number_of_songs (self->stats) + 1,
            (GDestroyNotify) mpd_song_free);

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

                g_async_queue_push (store->sqltonet_queue,
                        (gpointer) mpd_entity_get_song (ent) );
            } else {
                mpd_entity_free (ent);
                ent = NULL;
            }
        }
    }
    END_COMMAND;

    /* tell SQL thread kindly to die, but wait for him to bleed */
    g_async_queue_push (store->sqltonet_queue, async_queue_terminator);
    g_thread_join (sql_thread);

    mc_shelper_report_progress (self, true, "database: retrieved %d songs from mpd (took %2.3fs)",
            number_of_songs, g_timer_elapsed (timer, NULL) );

    if (lock_self) {
        UNLOCK_UPDATE_MTX (store);
    }
    
    /* we got the full db, next time check again if it's needed */
    store->need_full_db = FALSE;
}

///////////////

static gpointer mc_store_do_plchanges_wrapper (mc_StoreDB * self)
{
    mc_store_do_plchanges (self, true);
    mc_stprv_insert_meta_attributes (self);
    return NULL;
}

///////////////

static void mc_store_do_plchanges_bkgd (mc_StoreDB * self)
{
    g_thread_new ("db-queue-update-thread", 
            (GThreadFunc) mc_store_do_plchanges_wrapper, self);
}

///////////////

static gpointer mc_store_do_listall_and_plchanges (mc_StoreDB * self)
{
    LOCK_UPDATE_MTX (self);
    mc_store_do_list_all_info (self, false);
    mc_store_do_plchanges (self, false);
    mc_stprv_insert_meta_attributes (self);
    UNLOCK_UPDATE_MTX (self);
    return NULL;
}

///////////////

static void mc_store_do_listall_and_plchanges_bkgd (mc_StoreDB * self)
{
    g_thread_new ("db-listall-and-plchanges",
            (GThreadFunc) mc_store_do_listall_and_plchanges, self);
}

///////////////

static gpointer mc_store_deserialize_songs_and_plchanges (mc_StoreDB * self)
{
    LOCK_UPDATE_MTX (self);
    mc_stprv_deserialize_songs (self, false);
    mc_store_do_plchanges (self, false);
    mc_stprv_insert_meta_attributes (self);
    UNLOCK_UPDATE_MTX (self);
    return NULL;   
}

static void mc_store_deserialize_songs_and_plchanges_bkgd (mc_StoreDB * self)
{
    g_thread_new ("db-deserialize-and-plchanges-thread",
            (GThreadFunc) mc_store_deserialize_songs_and_plchanges, self);
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
    g_assert (self && self->client == client);

    g_print ("-- UPDATE [%d] --\n", events);

    if (events & MPD_IDLE_DATABASE) {
        mc_store_do_listall_and_plchanges_bkgd (self);
    } else if (events & MPD_IDLE_QUEUE) {
        mc_store_do_plchanges_bkgd (self);
    }

    /* needs to happen in both cases (post) */
    if (events & (MPD_IDLE_QUEUE | MPD_IDLE_DATABASE)) {
        self->write_to_disk = TRUE;
    }
}

///////////////

static void mc_store_connectivity_callback (
        mc_Client * client,
        bool is_connected,
        bool server_changed,
        mc_StoreDB * self)
{
    g_assert (self && client && self->client == client);

    g_print ("-- CONNECTIVITY --\n");

    if (is_connected) {
        if (server_changed) {
            self->need_full_db = TRUE;
            self->need_full_queue = TRUE;
        }
        mc_store_update_callback (client, MPD_IDLE_DATABASE, self);
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
    store->sqltonet_queue = g_async_queue_new ();

    int song_count = -1;

    if ( (song_count = mc_store_check_if_db_is_still_valid (store) ) < 0) {
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
        mc_shelper_report_progress (store->client, true, "database: %s exists already.", store->db_path);

        /* stack is allocated to the old size */
        store->stack = mc_store_stack_create (song_count + 1, (GDestroyNotify) mpd_song_free);

        /* load the old database into memory */
        mc_stprv_load_or_save (store, false);

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

    /* be ready for db updates */
    mc_signal_add_masked (
            &store->client->_signals,
            "client-event", true, /* call first */
            (mc_ClientEventCallback) mc_store_update_callback, store,
            MPD_IDLE_DATABASE | MPD_IDLE_QUEUE);

    mc_signal_add (
            &store->client->_signals,
            "connectivity", true, /* call first */
            (mc_ConnectivityCallback) mc_store_connectivity_callback, store);

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
    mc_store_stack_free (self->stack);

    if (self->write_to_disk)
        mc_stprv_load_or_save (self, true);

    mc_stprv_close_handle (self);

    g_free (self->db_path);
    g_free (self);
}

///////////////

int mc_store_search_out ( mc_StoreDB * self, const char * match_clause, bool queue_only, mpd_song ** song_buffer, int buf_len)
{
    /* we should not operate on changing data */
    return_val_if_locked (self, -1);

    return mc_stprv_select_out (self, match_clause, queue_only, song_buffer, buf_len);
}

///////////////

void mc_store_set_wait_mode (mc_StoreDB * self, bool wait_for_db_finish)
{
    g_assert (self);
    self->wait_for_db_finish = wait_for_db_finish;
}
