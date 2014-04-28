#include "../mpd/signal-helper.h"
#include "../mpd/signal.h"
#include "../mpd/update.h"
#include "../util/gzip.h"

#include "db-stored-playlists.h"
#include "db-operations.h"
#include "db-private.h"
#include "db-macros.h"
#include "db-dirs.h"
#include "db.h"

/* g_unlink() */
#include <glib/gstdio.h>

/* log2() */
#include <math.h>


/**
 * Data passed along a Job. 
 * Not all fields are filled, only "op" is always filled,
 * and most of the time "out_stack".t
 */
typedef struct {
    mc_StoreOperation op;
    const char *match_clause;
    const char *playlist_name;
    const char *dir_directory;
    bool queue_only;
    int length_limit;
    int dir_depth;
    mc_Stack *out_stack;
    unsigned needle_song_id;
} mc_JobData;


/* List of Priorities for all Operations.
 *
 */ 
int mc_JobPrios[] = {
    [MC_OPER_DESERIALIZE]     = -1,
    [MC_OPER_LISTALLINFO]     = -1,
    [MC_OPER_PLCHANGES]       = +0,
    [MC_OPER_SPL_LOAD]        = +1,
    [MC_OPER_SPL_UPDATE]      = +1,
    [MC_OPER_UPDATE_META]     = +1,
    [MC_OPER_DB_SEARCH]       = +2,
    [MC_OPER_DIR_SEARCH]      = +2,
    [MC_OPER_SPL_LIST]        = +2,
    [MC_OPER_SPL_LIST_ALL]    = +2,
    [MC_OPER_SPL_QUERY]       = +2,
    [MC_OPER_WRITE_DATABASE]  = +3,
    [MC_OPER_FIND_SONG_BY_ID] = +4,
    [MC_OPER_UNDEFINED]       = 10
};

/**
 * Map mc_OpFinishedEnum members to meaningful strings
 */
const char * mc_JobNames[] = {
    [MC_OPER_DESERIALIZE]     = "DESERIALIZE",
    [MC_OPER_LISTALLINFO]     = "LISTALLINFO",
    [MC_OPER_PLCHANGES]       = "PLCHANGES",
    [MC_OPER_SPL_LOAD]        = "SPL_LOAD",
    [MC_OPER_SPL_UPDATE]      = "SPL_UPDATE",
    [MC_OPER_UPDATE_META]     = "UPDATE_META",
    [MC_OPER_DB_SEARCH]       = "DB_SEARCH",
    [MC_OPER_DIR_SEARCH]      = "DIR_SEARCH",
    [MC_OPER_SPL_LIST]        = "SPL_LIST",
    [MC_OPER_SPL_LIST_ALL]    = "SPL_LIST_ALL",
    [MC_OPER_SPL_QUERY]       = "SPL_QUERY",
    [MC_OPER_WRITE_DATABASE]  = "WRITE_DATABASE",
    [MC_OPER_FIND_SONG_BY_ID] = "FIND_SONG_BY_ID",
    [MC_OPER_UNDEFINED]       = "[Unknown]"
};

//////////////////////////////
//                          //
//     Utility Functions    //
//                          //
//////////////////////////////

/**
 * @brief Send a job to the JobProcessor, with only necessary fields filled.
 *
 * @param self send it to the JobProcess of this Store
 * @param op What operation.
 *
 * @return a job id.
 */
static long mc_store_send_job_no_args(
        mc_Store *self,
        mc_StoreOperation op)
{
    g_assert(0 <= op && op <= MC_OPER_ENUM_MAX);

    mc_JobData *data = g_new0(mc_JobData, 1);
    data->op = op;

    return mc_jm_send(self->jm, mc_JobPrios[data->op], data);
}

//////////////////////////////

/**
 * @brief Convert a mc_StoreOperation mask to a string.
 *
 *
 * >>> printf(mc_store_op_to_string(MC_OPER_LISTALLINFO | MC_OPER_SPL_QUERY));
 * LISTALLINFO, SPL_QUERY
 *
 * This function is used for debugging only currently.
 *
 * @param op a bitmask of operations
 *
 * @return a newly allocated string. Free it when done with g_free
 */
static char * mc_store_op_to_string(mc_StoreOperation op)
{
    g_assert(0 <= op && op <= MC_OPER_ENUM_MAX);

    if(op == 0) {
        return NULL;
    }

    unsigned name_index = 0;
    unsigned base = (unsigned)log2(MC_OPER_ENUM_MAX);
    const char * names[base + 1];

    for(unsigned i = 0; i < base; ++i) {
        if(op & (1 << i)) {
            names[name_index++] = mc_JobNames[1 << i];
            names[name_index] = NULL;
        }
    }

    return g_strjoinv(", ", (char **)names);
}

//////////////////////////////

/**
 * @brief Construct a full path from a host, port and root directory
 *
 * @param client Client to get the host and port from.
 * @param directory The root directory of the database
 *
 * @return a newly allocated path, use free once done.
 */
static char *mc_store_construct_full_dbpath(mc_Store *self, const char *directory)
{
    g_mutex_lock(&self->mirrored_mtx);
    char * path = g_strdup_printf(
            "%s%cmoosecat_%s:%d.sqlite",
            (directory) ? directory : ".",
            G_DIR_SEPARATOR,
            self->mirrored_host,
            self->mirrored_port
    );
    g_mutex_unlock(&self->mirrored_mtx);
    return path;
}

//////////////////////////////

/**
 * @brief See mc_store_find_song_by_id for what this does.
 *
 * @return a Stack with one element containg one song or NULL.
 */
mc_Stack * mc_store_find_song_by_id_impl(mc_Store *self, unsigned needle_song_id) { 
    unsigned length = mc_stack_length(self->stack);
    for(unsigned i = 0; i < length; ++i) {
        struct mpd_song * song = mc_stack_at(self->stack, i);
        if(song != NULL && mpd_song_get_id(song) == needle_song_id) {
            mc_Stack * stack = mc_stack_create(1, NULL);
            if(stack != NULL) {
                /* return a stack with one element
                 * (because that's what the interfaces wants :/)
                 */
                mc_stack_append(stack, song);
                return stack;
            }
        }
    }
    return NULL;
}

//////////////////////////////

/**
 * @brief Will return true, if the database located on disk is still valid.
 *
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
 *  @param self The database to check
 *  @db_path path to the directory
 *
 * @return the number of songs in the songs table, or -1 on failure.
 */
static int mc_store_check_if_db_is_still_valid(mc_Store *self, const char *db_path)
{
    /* result */
    int song_count = -1;

    /* result of check #1 */
    bool exist_check = FALSE;

    char *cached_hostname = NULL;

    /* check #1 */
    if (g_file_test(db_path, G_FILE_TEST_IS_REGULAR) == TRUE) {
        exist_check = TRUE;
    } else if (self->settings->use_compression) {
        char *zip_path = g_strdup_printf("%s%s", db_path, MC_GZIP_ENDING);
        exist_check = g_file_test(zip_path, G_FILE_TEST_IS_REGULAR);
        GTimer *zip_timer = g_timer_new();

        if ((exist_check = mc_gunzip(zip_path)) == false)
            mc_shelper_report_progress(
                    self->client, true, 
                    "database: Unzipping %s failed.",
                    zip_path
            );
        else
            mc_shelper_report_progress(
                    self->client, true,
                    "database: Unzipping %s done (took %2.3fs).",
                    zip_path, g_timer_elapsed(zip_timer, NULL)
            );

        g_timer_destroy(zip_timer);
        g_free(zip_path);
    }

    if (exist_check == false) {
        mc_shelper_report_progress(self->client, true,
                "database: %s does not exist, creating new.", db_path
        );
        return -1;
    }

    /* check #2 */
    if (sqlite3_open(db_path, &self->handle) != SQLITE_OK) {
        mc_shelper_report_progress(self->client, true,
                "database: %s cannot be opened (corrupted?), creating new.", db_path
        );
        goto close_handle;
    }

    /* needed in order to select metadata */
    mc_stprv_create_song_table(self);
    mc_stprv_prepare_all_statements(self);

    /* check #3 */
    unsigned cached_port = mc_stprv_get_mpd_port(self);
    if (cached_port != mc_get_port(self->client)) {
        mc_shelper_report_progress(self->client, true,
                "database: %s's port changed (old=%d, new=%d), creating new.",
                db_path, cached_port, mc_get_port(self->client)
        );
        goto close_handle;
    }

    cached_hostname = mc_stprv_get_mpd_host(self);
    if(g_strcmp0(cached_hostname, mc_get_host(self->client)) != 0) {
        mc_shelper_report_progress(self->client, true,
                "database: %s's host changed (old=%s, new=%s), creating new.",
                db_path, cached_hostname, mc_get_host(self->client)
        );
        goto close_handle;
    }

    /* check #4 */
    size_t cached_db_version = mc_stprv_get_db_version(self);
    size_t current_db_version = 0;
    struct mpd_stats * stats = mc_lock_statistics(self->client);
    if(stats != NULL) { 
        current_db_version = mpd_stats_get_db_update_time(stats);
    }
    mc_unlock_statistics(self->client);

    if(cached_db_version != current_db_version)
        goto close_handle;

    size_t cached_sc_version = mc_stprv_get_sc_version(self);
    if (cached_sc_version != MC_DB_SCHEMA_VERSION)
        goto close_handle;
    
    /* All okay! we can use the old database */
    song_count = mc_stprv_get_song_count(self);

    mc_shelper_report_progress(self->client, true, "database: %s exists already and is valid.", db_path);

close_handle:

    /* clean up */
    mc_stprv_close_handle(self, true);
    g_free(cached_hostname);

    return song_count;
}

//////////////////////////////

static void mc_store_shutdown(mc_Store * self) 
{
    g_assert(self);

    /* Free the song stack */
    mc_stack_free(self->stack);
    self->stack = NULL;

    /* Free list of stored playlists */
    mc_stprv_spl_destroy(self);

    char *db_path = mc_store_construct_full_dbpath(self, self->db_directory);

    if (self->write_to_disk)
        mc_stprv_load_or_save(self, true, db_path);

    if (self->settings->use_compression && mc_gzip(db_path) == false)
        mc_shelper_report_progress(self->client, true, "Note: Nothing to zip.\n");

    mc_stprv_dir_finalize_statements(self);
    mc_stprv_close_handle(self, true);
    self->handle = NULL;

    if (self->settings->use_memory_db == false)
        g_unlink(MC_STORE_TMP_DB_PATH);

    g_free(db_path);
}

//////////////////////////////

static void mc_store_buildup(mc_Store * self) 
{
    /* either number of songs in 'songs' table or -1 on error */
    int song_count = -1;

    char *db_path = mc_store_construct_full_dbpath(self, self->db_directory);

    if ((song_count = mc_store_check_if_db_is_still_valid(self, db_path)) < 0) {
        mc_shelper_report_progress(self->client, true, "database: will fetch stuff from mpd.");

        /* open a sqlite handle, pointing to a database, either a new one will be created,
         * or an backup will be loaded into memory */
        mc_strprv_open_memdb(self);
        mc_stprv_prepare_all_statements(self);
        mc_stprv_dir_prepare_statemtents(self);

        /* make sure we inserted the meta info at least once */
        mc_stprv_insert_meta_attributes(self);
        
        /* Forces updates (even if queue version seems to be the same) */
        self->force_update_listallinfo = true;
        self->force_update_plchanges = true;

        /* the database is new, so no pos/id information is there yet
         * we need to query mpd, update db && queue info */
        mc_store_send_job_no_args(self, MC_OPER_LISTALLINFO);
    } else {
        /* open a sqlite handle, pointing to a database, either a new one will be created,
         * or an backup will be loaded into memory */
        mc_strprv_open_memdb(self);
        mc_stprv_prepare_all_statements(self);
        mc_stprv_dir_prepare_statemtents(self);

        /* stack is allocated to the old size */
        self->stack = mc_stack_create(song_count + 1, (GDestroyNotify) mpd_song_free);

        /* load the old database into memory */
        mc_stprv_load_or_save(self, false, db_path);

        mc_store_send_job_no_args(self, MC_OPER_DESERIALIZE);
    }

    /* Playlist support */
    mc_stprv_spl_init(self);

    g_free(db_path);
}

//////////////////////////////
//                          //
//     Signal Callbacks     //
//                          //
//////////////////////////////

/**
 * @brief Gets called on client events
 *
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
 * @param client the client that noticed the event.
 * @param events event bitmask 
 * @param self Store to operate on.
 */
static void mc_store_update_callback(
        mc_Client *client,
        enum mpd_idle events,
        mc_Store *self)
{
    g_assert(self && client && self->client == client);

    if (events & MPD_IDLE_DATABASE) {
        mc_store_send_job_no_args(self, MC_OPER_LISTALLINFO);
    } else if (events & MPD_IDLE_QUEUE) {
        mc_store_send_job_no_args(self, MC_OPER_PLCHANGES);
    } else if (events & MPD_IDLE_STORED_PLAYLIST) {
        mc_store_send_job_no_args(self, MC_OPER_SPL_UPDATE);
    }
}

//////////////////////////////

/**
 * @brief Called when the connection status changes.
 *
 * @param client the client to watch.
 * @param server_changed did the server changed since last connect()?
 * @param self the store to operate on
 */
static void mc_store_connectivity_callback(
    mc_Client *client,
    bool server_changed,
    bool was_connected,
    mc_Store *self)
{
    g_assert(self && client && self->client == client);

    if (mc_is_connected(client)) {
        if (server_changed) {

            mc_stprv_lock_attributes(self);

            mc_store_shutdown(self);
            g_free(self->mirrored_host);
            self->mirrored_host = g_strdup(mc_get_host(client));
            self->mirrored_port = mc_get_port(client);
            mc_store_buildup(self);

            //mc_store_rebuild(self);
            mc_stprv_unlock_attributes(self);
        } else {
            /* Important to send those two seperate (different priorities */
            mc_store_send_job_no_args(self, MC_OPER_LISTALLINFO);
            mc_store_send_job_no_args(self, MC_OPER_WRITE_DATABASE);
        }
    }
}


//////////////////////////////
//                          //
//    Callback Functions    //
//                          //
//////////////////////////////

void *mc_store_job_execute_callback(
        struct mc_JobManager *jm,  /* store->jm */
        volatile bool *cancel_op,  /* Check if the operation should be cancelled */
        void *user_data,           /* store */
        void *job_data             /* operation data */
)
{
    void * result = NULL;
    mc_Store *self = user_data;
    mc_JobData *data = job_data;

    if(data->op == 0) {
        /* Oooooh! a goto! How evil :-) */
        goto cleanup;
    }

    /* Lock the whole Processing once. more. Why?
     *
     * In case of searching something from the database 
     * we return a result. This result is passed to the user of the API.
     * When selecting for example something from the Query we just 
     * create a new stack and append pointers to songs in store->stack to it.
     * Now during a client-update these songs could be freed at any time. 
     * (The curse of multithreaded programs) 
     *
     * So we lock a mutex here, and let the user unlock it. 
     * What happens if the user does not unlock it? Bad things like deadlocks.
     * */
    mc_stprv_lock_attributes(self);
    {
        mc_shelper_report_progress(self->client, true, "Processing: %s", mc_JobNames[data->op]);

        /* NOTE:
         * 
         * The order of the operations is important here.
         * Operations with highest priority are on top and may include
         * dependencies by binary OR'ing them to data->op. 
         *
         * The order here loosely models the on on mc_JobPrios,
         * but is not restricted to that.
         */

        if(data->op & MC_OPER_DESERIALIZE) {
            mc_stprv_deserialize_songs(self);
            mc_stprv_queue_update_stack_posid(self);
            data->op |= (MC_OPER_PLCHANGES | MC_OPER_SPL_UPDATE | MC_OPER_UPDATE_META);
            self->force_update_listallinfo = false;
        }

        if(data->op & MC_OPER_LISTALLINFO) {
            mc_store_oper_listallinfo(self, cancel_op);
            data->op |= (MC_OPER_PLCHANGES | MC_OPER_SPL_UPDATE | MC_OPER_UPDATE_META);
            self->force_update_listallinfo = false;
        }
        
        if(data->op & MC_OPER_PLCHANGES) {
            mc_store_oper_plchanges(self, cancel_op);
            data->op |= (MC_OPER_SPL_UPDATE | MC_OPER_UPDATE_META);
            self->force_update_plchanges = false;
        }
        
        if(data->op & MC_OPER_UPDATE_META) {
            mc_stprv_insert_meta_attributes(self);
        }

        if(data->op & MC_OPER_SPL_UPDATE) {
            mc_stprv_spl_update(self);
        }
        
        if(data->op & MC_OPER_SPL_LOAD) {
            mc_stprv_spl_load_by_playlist_name(self, data->playlist_name);
        }
        
        if(data->op & MC_OPER_SPL_LIST) {
            mc_stprv_spl_get_loaded_playlists(self, data->out_stack);
            result = data->out_stack;
        }

        if(data->op & MC_OPER_SPL_LIST_ALL) {
            mc_stprv_spl_get_known_playlists(self, data->out_stack);
            result = data->out_stack;
        }

        if(data->op & MC_OPER_DB_SEARCH) {
            mc_stprv_select_to_stack(
                    self, data->match_clause, data->queue_only,
                    data->out_stack, data->length_limit
            );
            result = data->out_stack;
        }

        if(data->op & MC_OPER_DIR_SEARCH) {
            mc_stprv_dir_select_to_stack(
                    self, data->out_stack,
                    data->dir_directory, data->dir_depth
            );
            result = data->out_stack;
        }

        if(data->op & MC_OPER_SPL_QUERY) {
            mc_stprv_spl_select_playlist(
                    self, data->out_stack, 
                    data->playlist_name, data->match_clause
            );
            result = data->out_stack;
        }

        if(data->op & MC_OPER_WRITE_DATABASE) {
            if (self->write_to_disk) {
                char *full_path = mc_store_construct_full_dbpath(self, self->db_directory);
                mc_stprv_load_or_save(self, true, full_path);
                g_free(full_path);
            }
        }

        if(data->op & MC_OPER_FIND_SONG_BY_ID) { 
            data->out_stack = mc_store_find_song_by_id_impl(self, data->needle_song_id);
            result = data->out_stack;
        }

        /* If the operation includes writing stuff,
         * we need to remember to save the database to
         * disk */
        if(data->op & (0
                    | MC_OPER_LISTALLINFO 
                    | MC_OPER_PLCHANGES 
                    | MC_OPER_SPL_UPDATE 
                    | MC_OPER_SPL_LOAD 
                    | MC_OPER_UPDATE_META)) {
            self->write_to_disk = TRUE;
        }

    } 
    /* ATTENTION: 
     * We only unlock if we have an operation that does not deliver a result.
     * Otherwise the user has to unlock it himself! 
     */
    if((data->op & (0 
                | MC_OPER_DB_SEARCH 
                | MC_OPER_SPL_QUERY 
                | MC_OPER_SPL_LIST 
                | MC_OPER_SPL_LIST_ALL
                | MC_OPER_DIR_SEARCH)) == 0) {
        mc_stprv_unlock_attributes(self);
    }

    char * processed_ops = mc_store_op_to_string(data->op);
    mc_shelper_report_progress(self->client, true, "Processing done: %s", processed_ops);
    g_free(processed_ops);

cleanup:
    /* Free the data pack */
    g_free((char *)data->match_clause);
    g_free((char *)data->playlist_name);
    g_free((char *)data->dir_directory);
    g_free(data);

    return result;
}

//////////////////////////////
//                          //
//     Public Functions     //
//                          //
//////////////////////////////

mc_Store *mc_store_create(mc_Client *client, mc_StoreSettings *settings)
{
    g_assert(client);

    /* allocated memory for the mc_Store struct */
    mc_Store *store = g_new0(mc_Store, 1);

    /* Initialize the Attribute mutex early */
    g_mutex_init(&store->attr_set_mtx);
    g_mutex_init(&store->mirrored_mtx);

    /* Initialize the job manager used to background jobs */
    store->jm = mc_jm_create(mc_store_job_execute_callback, store);

    /* Settings */
    store->settings = (settings) ? settings : mc_store_settings_new();

    /* client is used to keep the db content updated */
    store->client = client;

    /* Remember the host/port */
    store->mirrored_host = g_strdup(mc_get_host(client));
    store->mirrored_port = mc_get_port(client);

    /* create the full path to the db */
    store->db_directory = g_strdup(store->settings->db_directory);

    /* Do the actual hard work */
    mc_store_buildup(store);

    /* Register for client events */
    mc_priv_signal_add_masked(
        &store->client->_signals,
        "client-event", true, /* call first */
        (mc_ClientEventCallback) mc_store_update_callback, store,
        MPD_IDLE_DATABASE | MPD_IDLE_QUEUE | MPD_IDLE_STORED_PLAYLIST
    );

    /* Register to be notifed when the connection status changes */
    mc_priv_signal_add(
        &store->client->_signals,
        "connectivity", true, /* call first */
        (mc_ConnectivityCallback) mc_store_connectivity_callback, store
    );

    return store;
}

//////////////////////////////

/*
 * close everything down.
 *
 * #1 remove the update watcher (might be called still otherwise, if client lives longer than the store)
 * #2 free the stack and all associated memory.
 * #3 Save database dump to disk if it hadn't been read from there.
 * #4 Close the handle.
 */
void mc_store_close(mc_Store *self)
{
    if (self == NULL)
        return;

    mc_signal_rm(self->client, "client-event", mc_store_update_callback);
    mc_signal_rm(self->client, "connectivity", mc_store_connectivity_callback);

    mc_stprv_lock_attributes(self);

    /* Close the job pool (still finishes current operation) */
    mc_jm_close(self->jm);

    mc_store_shutdown(self);

    mc_stprv_unlock_attributes(self);
    g_mutex_clear(&self->attr_set_mtx);
    g_mutex_clear(&self->mirrored_mtx);

    /* NOTE: Settings should be destroyed by caller,
     *       Since it should be valid to call close()
     *       several times.
     * mc_store_settings_destroy (self->settings);
     */
    g_free(self->mirrored_host);
    g_free(self->db_directory);
    g_free(self);
}

//////////////////////////////

struct mpd_song *mc_store_song_at(mc_Store *self, int idx)
{
    g_assert(self); 
    g_assert(idx >= 0);

    return (struct mpd_song *)mc_stack_at(self->stack, idx);
}

//////////////////////////////

int mc_store_total_songs(mc_Store *self)
{
    g_assert(self);

    return mc_stack_length(self->stack);
}

//////////////////////////////
//                          //
//    Playlist Functions    //
//                          //
//////////////////////////////

long mc_store_playlist_load(mc_Store *self, const char *playlist_name)
{
    g_assert(self);

    mc_JobData * data = g_new0(mc_JobData, 1);
    data->op = MC_OPER_SPL_LOAD;
    data->playlist_name = g_strdup(playlist_name);

    return mc_jm_send(self->jm, mc_JobPrios[MC_OPER_SPL_LOAD], data);
}

//////////////////////////////

long mc_store_playlist_select_to_stack(mc_Store *self, mc_Stack *stack, const char *playlist_name, const char *match_clause)
{
    g_assert(self);
    g_assert(stack);
    g_assert(playlist_name);

    mc_JobData * data = g_new0(mc_JobData, 1);
    data->op = MC_OPER_SPL_QUERY;

    data->playlist_name = g_strdup(playlist_name);
    data->match_clause = g_strdup(match_clause);
    data->out_stack = stack;

    return mc_jm_send(self->jm, mc_JobPrios[MC_OPER_SPL_QUERY], data);
}

//////////////////////////////

long mc_store_dir_select_to_stack(mc_Store *self, mc_Stack *stack, const char *directory, int depth)
{ 
    g_assert(self);
    g_assert(stack);

    mc_JobData * data = g_new0(mc_JobData, 1);
    data->op = MC_OPER_DIR_SEARCH;
    data->dir_directory = g_strdup(directory);
    data->dir_depth = depth;
    data->out_stack = stack;

    return mc_jm_send(self->jm, mc_JobPrios[MC_OPER_DIR_SEARCH], data);
}

//////////////////////////////

long mc_store_playlist_get_all_loaded(mc_Store *self, mc_Stack *stack)
{
    g_assert(self);
    g_assert(stack);

    mc_JobData * data = g_new0(mc_JobData, 1);
    data->op = MC_OPER_SPL_LIST;
    data->out_stack = stack;

    return mc_jm_send(self->jm, mc_JobPrios[MC_OPER_SPL_LIST], data);
}

//////////////////////////////

long mc_store_playlist_get_all_known(mc_Store *self, mc_Stack *stack)
{
    g_assert(self);
    g_assert(stack);

    mc_JobData * data = g_new0(mc_JobData, 1);
    data->op = MC_OPER_SPL_LIST_ALL;
    data->out_stack = stack;

    return mc_jm_send(self->jm, mc_JobPrios[MC_OPER_SPL_LIST_ALL], data);
}

//////////////////////////////

long mc_store_search_to_stack(
        mc_Store *self, const char *match_clause,
        bool queue_only, mc_Stack *stack, int limit_len
    )
{
    mc_JobData * data = g_new0(mc_JobData, 1);
    data->op = MC_OPER_DB_SEARCH;

    data->match_clause = g_strdup(match_clause);
    data->queue_only = queue_only;
    data->length_limit = limit_len;
    data->out_stack = stack;

    return mc_jm_send(self->jm, mc_JobPrios[MC_OPER_DB_SEARCH], data);
}

//////////////////////////////

mc_Stack * mc_store_get_full_stack_copy(mc_Store *self) 
{
    g_assert(self);
    
    mc_stprv_lock_attributes(self);
    return mc_stack_copy(self->stack);
}

//////////////////////////////

void mc_store_wait(mc_Store *self)
{
    mc_jm_wait(self->jm);
}

//////////////////////////////

void mc_store_wait_for_job(mc_Store *self, int job_id)
{
    mc_jm_wait_for_id(self->jm, job_id);
}

//////////////////////////////

mc_Stack *mc_store_get_result(mc_Store *self, int job_id)
{
    return mc_jm_get_result(self->jm, job_id);
}

//////////////////////////////

mc_Stack *mc_store_gw(mc_Store *self, int job_id)
{
    mc_store_wait_for_job(self, job_id);
    return mc_store_get_result(self, job_id);
}

//////////////////////////////

void mc_store_release(mc_Store *self)
{
    g_assert(self);

    mc_stprv_unlock_attributes(self);
}

//////////////////////////////

struct mpd_song * mc_store_find_song_by_id(mc_Store * self, unsigned needle_song_id)
{
    g_assert(self);

    mc_JobData * data = g_new0(mc_JobData, 1);
    data->op = MC_OPER_FIND_SONG_BY_ID;
    data->needle_song_id = needle_song_id;

    unsigned job_id = mc_jm_send(self->jm, mc_JobPrios[MC_OPER_FIND_SONG_BY_ID], data);
    mc_store_wait_for_job(self, job_id);
    mc_Stack * stack = mc_store_get_result(self, job_id);
    if(stack != NULL && mc_stack_length(stack) > 0) {
        struct mpd_song * song = mc_stack_at(stack, 0);
        return song;
    }
    return NULL;
}
