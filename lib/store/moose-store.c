#include "../moose-config.h"
#include "../misc/moose-misc-gzip.h"

#include "moose-store.h"
#include "sqlite3.h"

/* g_unlink() */
#include <glib-object.h>
#include <glib/gstdio.h>

/* log2() */
#include <math.h>

/* strncpy */
#include <string.h>

typedef struct _MooseStorePrivate {
    /* directory db lies in */
    char * db_directory;

    /* songstack - a place for mpd_songs to live in */
    MoosePlaylist * stack;

    /* handle to sqlite */
    sqlite3 * handle;

    /* prepared statements for normal operations */
    sqlite3_stmt ** sql_prep_stmts;

    /* client associated with this store */
    MooseClient * client;

    /* Write database to disk?
     * on changes this gets set to True
     * */
    bool write_to_disk;

    /* Support for stored playlists */
    GPtrArray * spl_stack;

    /* If this flag is set listallinfo will retrieve all songs,
     * and skipping the check if is actually necessary.
     * */
    bool force_update_listallinfo;

    /* If this flag is set plchanges will take last_version as 0,
     * even if, judging from the queue version, no full update is necessary.
     * */
    bool force_update_plchanges;

    /* Job manager used to process database tasks in the background */
    struct MooseJobManager * jm;

    /* Locked when setting an attribute, or reading from one
     * Attributes are:
     *    - stack
     *    - spl.stack
     *
     * db-dirs.c append copied data onto the stack.
     *
     * */
    GMutex attr_set_mtx;

    /*
     * The Port of the Server this Store mirrors
     */
    int mirrored_port;

    /*
     * The Host this Store mirrors
     */
    char * mirrored_host;
    GMutex mirrored_mtx;

    MooseStoreCompletion * completion;

    struct {
        bool use_memory_db;
        bool use_compression;
        char tokenizer[32];
    } settings;
} MooseStorePrivate;

enum {
    PROP_CLIENT,
    PROP_FULL_PLAYLIST,
    PROP_DB_DIRECTORY,
    PROP_USE_COMPRESSION,
    PROP_USE_MEMORY_DB,
    PROP_TOKENIZER,
    PROP_N
};

G_DEFINE_TYPE_WITH_PRIVATE(
    MooseStore, moose_store, G_TYPE_OBJECT
);

#include "moose-store-private.c"

/**
 * Data passed along a Job.
 * Not all fields are filled, only "op" is always filled,
 * and most of the time "out_stack".t
 */
typedef struct {
    MooseStoreOperation op;
    const char * match_clause;
    const char * playlist_name;
    const char * dir_directory;
    bool queue_only;
    int length_limit;
    int dir_depth;
    MoosePlaylist * out_stack;
    unsigned needle_song_id;
} MooseJobData;

/* List of Priorities for all Operations.
 *
 */
int MooseJobPrios[] = {
    [MOOSE_OPER_DESERIALIZE]     = -1,
    [MOOSE_OPER_LISTALLINFO]     = -1,
    [MOOSE_OPER_PLCHANGES]       = +0,
    [MOOSE_OPER_SPL_LOAD]        = +1,
    [MOOSE_OPER_SPL_UPDATE]      = +1,
    [MOOSE_OPER_UPDATE_META]     = +1,
    [MOOSE_OPER_DB_SEARCH]       = +2,
    [MOOSE_OPER_DIR_SEARCH]      = +2,
    [MOOSE_OPER_SPL_LIST]        = +2,
    [MOOSE_OPER_SPL_LIST_ALL]    = +2,
    [MOOSE_OPER_SPL_QUERY]       = +2,
    [MOOSE_OPER_WRITE_DATABASE]  = +3,
    [MOOSE_OPER_FIND_SONG_BY_ID] = +4,
    [MOOSE_OPER_UNDEFINED]       = 10
};

/**
 * Map MooseOpFinishedEnum members to meaningful strings
 */
const char * MooseJobNames[] = {
    [MOOSE_OPER_DESERIALIZE]     = "DESERIALIZE",
    [MOOSE_OPER_LISTALLINFO]     = "LISTALLINFO",
    [MOOSE_OPER_PLCHANGES]       = "PLCHANGES",
    [MOOSE_OPER_SPL_LOAD]        = "SPL_LOAD",
    [MOOSE_OPER_SPL_UPDATE]      = "SPL_UPDATE",
    [MOOSE_OPER_UPDATE_META]     = "UPDATE_META",
    [MOOSE_OPER_DB_SEARCH]       = "DB_SEARCH",
    [MOOSE_OPER_DIR_SEARCH]      = "DIR_SEARCH",
    [MOOSE_OPER_SPL_LIST]        = "SPL_LIST",
    [MOOSE_OPER_SPL_LIST_ALL]    = "SPL_LIST_ALL",
    [MOOSE_OPER_SPL_QUERY]       = "SPL_QUERY",
    [MOOSE_OPER_WRITE_DATABASE]  = "WRITE_DATABASE",
    [MOOSE_OPER_FIND_SONG_BY_ID] = "FIND_SONG_BY_ID",
    [MOOSE_OPER_UNDEFINED]       = "[Unknown]"
};

/**
 * @brief Send a job to the JobProcessor, with only necessary fields filled.
 *
 * @param self send it to the JobProcess of this Store
 * @param op What operation.
 *
 * @return a job id.
 */
static long moose_store_send_job_no_args(
    MooseStore * self,
    MooseStoreOperation op) {
    g_assert(0 <= op && op <= MOOSE_OPER_ENUM_MAX);

    MooseJobData * data = g_new0(MooseJobData, 1);
    data->op = op;

    return moose_jm_send(self->priv->jm, MooseJobPrios[data->op], data);
}

/**
 * @brief Convert a MooseStoreOperation mask to a string.
 *
 *
 * >>> printf(moose_store_op_to_string(MOOSE_OPER_LISTALLINFO | MOOSE_OPER_SPL_QUERY));
 * LISTALLINFO, SPL_QUERY
 *
 * This function is used for debugging only currently.
 *
 * @param op a bitmask of operations
 *
 * @return a newly allocated string. Free it when done with g_free
 */
static char * moose_store_op_to_string(MooseStoreOperation op) {
    g_assert(0 <= op && op <= MOOSE_OPER_ENUM_MAX);

    if (op == 0) {
        return NULL;
    }

    unsigned name_index = 0;
    unsigned base = (unsigned)log2(MOOSE_OPER_ENUM_MAX);
    const char * names[base + 1];

    for (unsigned i = 0; i < base; ++i) {
        if (op & (1 << i)) {
            names[name_index++] = MooseJobNames[1 << i];
            names[name_index] = NULL;
        }
    }

    return g_strjoinv(", ", (char **)names);
}

/**
 * @brief Construct a full path from a host, port and root directory
 *
 * @param client Client to get the host and port from.
 * @param directory The root directory of the database
 *
 * @return a newly allocated path, use free once done.
 */
static char * moose_store_construct_full_dbpath(MooseStore * self, const char * directory) {
    MooseStorePrivate *priv = self->priv;

    g_mutex_lock(&priv->mirrored_mtx);
    char * path = g_strdup_printf(
                      "%s%cmoosecat_%s:%d.sqlite",
                      (directory) ? directory : ".",
                      G_DIR_SEPARATOR,
                      priv->mirrored_host,
                      priv->mirrored_port
                  );
    g_mutex_unlock(&priv->mirrored_mtx);
    return path;
}

/**
 * @brief See moose_store_find_song_by_id for what this does.
 *
 * @return a Stack with one element containg one song or NULL.
 */
MoosePlaylist * moose_store_find_song_by_id_impl(MooseStore * self, unsigned needle_song_id) {
    unsigned length = moose_playlist_length(self->priv->stack);
    for (unsigned i = 0; i < length; ++i) {
        MooseSong * song = moose_playlist_at(self->priv->stack, i);
        if (song != NULL && moose_song_get_id(song) == needle_song_id) {
            MoosePlaylist * stack = moose_playlist_new();
            if (stack != NULL) {
                /* return a stack with one element
                 * (because that's what the interfaces wants :/)
                 */
                moose_playlist_append(stack, song);
                return stack;
            }
        }
    }
    return NULL;
}

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
static int moose_store_check_if_db_is_still_valid(MooseStore * self, const char * db_path) {
    /* result */
    int song_count = -1;

    /* result of check #1 */
    bool exist_check = FALSE;

    char * cached_hostname = NULL;

    /* check #1 */
    if (g_file_test(db_path, G_FILE_TEST_IS_REGULAR) == TRUE) {
        exist_check = TRUE;
    } else if (self->priv->settings.use_compression) {
        char * zip_path = g_strdup_printf("%s%s", db_path, MOOSE_GZIP_ENDING);
        exist_check = g_file_test(zip_path, G_FILE_TEST_IS_REGULAR);
        GTimer * zip_timer = g_timer_new();

        if ((exist_check = moose_gunzip(zip_path)) == false) {
            moose_warning("database: Unzipping %s failed.", zip_path);
        } else
            moose_message(
                "database: Unzipping %s done (took %2.3fs).",
                zip_path, g_timer_elapsed(zip_timer, NULL)
            );

        g_timer_destroy(zip_timer);
        g_free(zip_path);
    }

    if (exist_check == false) {
        moose_message("database: %s does not exist, creating new.", db_path);
        return -1;
    }

    /* check #2 */
    if (sqlite3_open(db_path, &self->priv->handle) != SQLITE_OK) {
        moose_warning(
            "database: %s cannot be opened (corrupted?), creating new.", db_path
        );
        goto close_handle;
    }

    /* needed in order to select metadata */
    moose_stprv_create_song_table(self->priv);
    moose_stprv_prepare_all_statements(self->priv);

    /* check #3 */
    unsigned cached_port = moose_stprv_get_mpd_port(self->priv);
    if (cached_port != moose_client_get_port(self->priv->client)) {
        moose_warning(
            "database: %s's port changed (old=%d, new=%d), creating new.",
            db_path, cached_port, moose_client_get_port(self->priv->client)
        );
        goto close_handle;
    }

    cached_hostname = moose_stprv_get_mpd_host(self->priv);
    if (g_strcmp0(cached_hostname, moose_client_get_host(self->priv->client)) != 0) {
        moose_warning(
            "database: %s's host changed (old=%s, new=%s), creating new.",
            db_path, cached_hostname, moose_client_get_host(self->priv->client)
        );
        goto close_handle;
    }

    /* check #4 */
    size_t cached_db_version = moose_stprv_get_db_version(self->priv);
    size_t current_db_version = 0;
    MooseStatus * status = moose_client_ref_status(self->priv->client);

    current_db_version = moose_status_stats_get_db_update_time(status);
    moose_status_unref(status);

    if (cached_db_version != current_db_version) {
        goto close_handle;
    }

    size_t cached_sc_version = moose_stprv_get_sc_version(self->priv);
    if (cached_sc_version != MOOSE_DB_SCHEMA_VERSION) {
        goto close_handle;
    }

    /* All okay! we can use the old database */
    song_count = moose_stprv_get_song_count(self->priv);

    moose_message("database: %s exists already and is valid.", db_path);

close_handle:
    moose_stprv_close_handle(self->priv, true);
    g_free(cached_hostname);
    return song_count;
}

static void moose_store_shutdown(MooseStore * self) {
    g_assert(self);

    MooseStorePrivate * priv = self->priv;

    /* Free the song stack */
    g_object_unref(priv->stack);
    priv->stack = NULL;

    char * db_path = moose_store_construct_full_dbpath(self, priv->db_directory);

    if (priv->write_to_disk) {
        moose_stprv_load_or_save(self->priv, true, db_path);
    }

    if (priv->settings.use_compression && moose_gzip(db_path) == false) {
        moose_warning("Weird, zipping failed.");
    }

    moose_stprv_close_handle(self->priv, true);
    priv->handle = NULL;

    if (priv->settings.use_memory_db == false) {
        g_unlink(MOOSE_STORE_TMP_DB_PATH);
    }

    if (priv->completion != NULL) {
        moose_store_completion_unref(priv->completion);
    }

    g_free(db_path);
}

static void moose_store_buildup(MooseStore * self) {
    /* either number of songs in 'songs' table or -1 on error */
    int song_count = -1;

    char * db_path = moose_store_construct_full_dbpath(
                         self, self->priv->db_directory
                     );

    if ((song_count = moose_store_check_if_db_is_still_valid(self, db_path)) < 0) {
        moose_message("database: will fetch stuff from mpd.");

        /* open a sqlite handle, pointing to a database, either a new one will be created,
         * or an backup will be loaded into memory */
        moose_strprv_open_memdb(self->priv);
        moose_stprv_prepare_all_statements(self->priv);

        /* make sure we inserted the meta info at least once */
        moose_stprv_insert_meta_attributes(self->priv);

        /* Forces updates (even if queue version seems to be the same) */
        self->priv->force_update_listallinfo = true;
        self->priv->force_update_plchanges = true;
        self->priv->stack = moose_playlist_new_full(100, (GDestroyNotify)moose_song_unref);

        /* the database is new, so no pos/id information is there yet
         * we need to query mpd, update db && queue info */
        moose_store_send_job_no_args(self, MOOSE_OPER_LISTALLINFO);
    } else {
        /* open a sqlite handle, pointing to a database, either a new one will be created,
         * or an backup will be loaded into memory */
        moose_strprv_open_memdb(self->priv);
        moose_stprv_prepare_all_statements(self->priv);

        /* stack is allocated to the old size */
        self->priv->stack = moose_playlist_new_full(
                                song_count + 1, (GDestroyNotify)moose_song_unref
                            );

        /* load the old database into memory */
        moose_stprv_load_or_save(self->priv, false, db_path);

        moose_store_send_job_no_args(self, MOOSE_OPER_DESERIALIZE);
    }

    g_free(db_path);
}

/**
 * @brief Gets called on client events
 *
 * On DB update we have no chance, but updating the whole tables.
 * (we do not need to update, if db version did not changed though.)
 *
 * #1 If events contains MOOSE_IDLE_DATABASE
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
static void moose_store_update_callback(
    MooseClient * client,
    MooseIdle events,
    MooseStore * self) {
    if ((events & (MOOSE_IDLE_DATABASE | MOOSE_IDLE_QUEUE | MOOSE_IDLE_STORED_PLAYLIST)) == 0) {
        return;  /* No interesting events */
    }

    g_assert(self && client && self->priv->client == client);

    if (events & MOOSE_IDLE_DATABASE) {
        moose_store_send_job_no_args(self, MOOSE_OPER_LISTALLINFO);
    } else if (events & MOOSE_IDLE_QUEUE) {
        moose_store_send_job_no_args(self, MOOSE_OPER_PLCHANGES);
    } else if (events & MOOSE_IDLE_STORED_PLAYLIST) {
        moose_store_send_job_no_args(self, MOOSE_OPER_SPL_UPDATE);
    }
}

/**
 * @brief Called when the connection status changes.
 *
 * @param client the client to watch.
 * @param server_changed did the server changed since last connect()?
 * @param self the store to operate on
 */
static void moose_store_connectivity_callback(
    MooseClient * client,
    bool server_changed,
    MooseStore * self) {
    g_assert(self && client && self->priv->client == client);

    if (moose_client_is_connected(client)) {
        if (server_changed) {

            moose_stprv_lock_attributes(self->priv);
            {
                moose_store_shutdown(self);
                g_free(self->priv->mirrored_host);
                self->priv->mirrored_host = g_strdup(moose_client_get_host(client));
                self->priv->mirrored_port = moose_client_get_port(client);
                moose_store_buildup(self);
            }
            moose_stprv_unlock_attributes(self->priv);
        } else {
            /* Important to send those two seperate (different priorities */
            moose_store_send_job_no_args(self, MOOSE_OPER_LISTALLINFO);
            moose_store_send_job_no_args(self, MOOSE_OPER_WRITE_DATABASE);
        }
    }
}

void * moose_store_job_execute_callback(
    G_GNUC_UNUSED struct MooseJobManager * jm,   /* store->jm */
    volatile bool * cancel_op,                   /* Check if the operation should be cancelled */
    void * user_data,                            /* store */
    void * job_data                              /* operation data */
) {
    void * result = NULL;
    MooseStore * self = user_data;
    MooseJobData * data = job_data;

    if (data->op == 0) {
        /* Beware. The devil is here. */
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
    moose_stprv_lock_attributes(self->priv);
    {
        moose_debug("Processing: %s", MooseJobNames[data->op]);

        /* NOTE:
         *
         * The order of the operations is important here.
         * Operations with highest priority are on top and may include
         * dependencies by binary OR'ing them to data->op.
         *
         * The order here loosely models the on on MooseJobPrios,
         * but is not restricted to that.
         */

        if (data->op & MOOSE_OPER_DESERIALIZE) {
            moose_stprv_deserialize_songs(self->priv);
            moose_stprv_queue_update_stack_posid(self->priv);
            data->op |= (MOOSE_OPER_PLCHANGES | MOOSE_OPER_SPL_UPDATE | MOOSE_OPER_UPDATE_META);
            self->priv->force_update_listallinfo = false;
        }

        if (data->op & MOOSE_OPER_LISTALLINFO) {
            moose_stprv_oper_listallinfo(self->priv, cancel_op);
            data->op |= (MOOSE_OPER_PLCHANGES | MOOSE_OPER_SPL_UPDATE | MOOSE_OPER_UPDATE_META);
            self->priv->force_update_listallinfo = false;
        }

        if (data->op & MOOSE_OPER_PLCHANGES) {
            moose_stprv_oper_plchanges(self->priv, cancel_op);
            data->op |= (MOOSE_OPER_SPL_UPDATE | MOOSE_OPER_UPDATE_META);
            self->priv->force_update_plchanges = false;
        }

        if (data->op & MOOSE_OPER_UPDATE_META) {
            moose_stprv_insert_meta_attributes(self->priv);
        }

        if (data->op & MOOSE_OPER_SPL_UPDATE) {
            moose_stprv_spl_update(self->priv);
        }

        if (data->op & MOOSE_OPER_SPL_LOAD) {
            moose_stprv_spl_load_by_playlist_name(self->priv, data->playlist_name);
        }

        if (data->op & MOOSE_OPER_SPL_LIST) {
            moose_stprv_spl_get_loaded_playlists(self->priv, data->out_stack);
            result = data->out_stack;
        }

        if (data->op & MOOSE_OPER_SPL_LIST_ALL) {
            // TODO!
            // moose_stprv_spl_get_known_playlists(self, data->out_stack);
            result = data->out_stack;
        }

        if (data->op & MOOSE_OPER_DB_SEARCH) {
            moose_stprv_select_to_stack(
                self->priv, data->match_clause, data->queue_only,
                data->out_stack, data->length_limit
            );
            result = data->out_stack;
        }

        if (data->op & MOOSE_OPER_DIR_SEARCH) {
            moose_stprv_dir_select_to_stack(
                self->priv, data->out_stack,
                data->dir_directory, data->dir_depth
            );
            result = data->out_stack;
        }

        if (data->op & MOOSE_OPER_SPL_QUERY) {
            moose_stprv_spl_select_playlist(
                self->priv, data->out_stack,
                data->playlist_name, data->match_clause
            );
            result = data->out_stack;
        }

        if (data->op & MOOSE_OPER_WRITE_DATABASE) {
            if (self->priv->write_to_disk) {
                char * full_path = moose_store_construct_full_dbpath(
                                       self, self->priv->db_directory
                                   );
                moose_stprv_load_or_save(self->priv, true, full_path);
                g_free(full_path);
            }
        }

        if (data->op & MOOSE_OPER_FIND_SONG_BY_ID) {
            data->out_stack = moose_store_find_song_by_id_impl(self, data->needle_song_id);
            result = data->out_stack;
        }

        /* If the operation includes writing stuff,
         * we need to remember to save the database to
         * disk */
        if (data->op & (0
                        | MOOSE_OPER_LISTALLINFO
                        | MOOSE_OPER_PLCHANGES
                        | MOOSE_OPER_SPL_UPDATE
                        | MOOSE_OPER_SPL_LOAD
                        | MOOSE_OPER_UPDATE_META)) {
            self->priv->write_to_disk = TRUE;
        }

    }
    /* ATTENTION:
     * We only unlock if we have an operation that does not deliver a result.
     * Otherwise the user has to unlock it himself!
     */
    if ((data->op & (0
                     | MOOSE_OPER_DB_SEARCH
                     | MOOSE_OPER_SPL_QUERY
                     | MOOSE_OPER_SPL_LIST
                     | MOOSE_OPER_SPL_LIST_ALL
                     | MOOSE_OPER_DIR_SEARCH)) == 0) {
        moose_stprv_unlock_attributes(self->priv);
    }

    char * processed_ops = moose_store_op_to_string(data->op);
    moose_debug("Processing done: %s", processed_ops);
    g_free(processed_ops);

cleanup:
    /* Free the data pack */
    g_free((char *)data->match_clause);
    g_free((char *)data->playlist_name);
    g_free((char *)data->dir_directory);
    g_free(data);

    return result;
}

MooseSong * moose_store_song_at(MooseStore * self, int idx) {
    g_assert(self);
    g_assert(idx >= 0);

    return (MooseSong *)moose_playlist_at(self->priv->stack, idx);
}

int moose_store_total_songs(MooseStore * self) {
    g_assert(self);

    return moose_playlist_length(self->priv->stack);
}

long moose_store_playlist_load(MooseStore * self, const char * playlist_name) {
    g_assert(self);

    MooseJobData * data = g_new0(MooseJobData, 1);
    data->op = MOOSE_OPER_SPL_LOAD;
    data->playlist_name = g_strdup(playlist_name);

    return moose_jm_send(self->priv->jm, MooseJobPrios[MOOSE_OPER_SPL_LOAD], data);
}

long moose_store_playlist_select_to_stack(
    MooseStore * self, MoosePlaylist * stack,
    const char * playlist_name, const char * match_clause) {
    g_assert(self);
    g_assert(stack);
    g_assert(playlist_name);

    MooseJobData * data = g_new0(MooseJobData, 1);
    data->op = MOOSE_OPER_SPL_QUERY;

    data->playlist_name = g_strdup(playlist_name);
    data->match_clause = g_strdup(match_clause);
    data->out_stack = stack;

    return moose_jm_send(self->priv->jm, MooseJobPrios[MOOSE_OPER_SPL_QUERY], data);
}

long moose_store_dir_select_to_stack(MooseStore * self, MoosePlaylist * stack, const char * directory, int depth) {
    g_assert(self);
    g_assert(stack);

    MooseJobData * data = g_new0(MooseJobData, 1);
    data->op = MOOSE_OPER_DIR_SEARCH;
    data->dir_directory = g_strdup(directory);
    data->dir_depth = depth;
    data->out_stack = stack;

    return moose_jm_send(self->priv->jm, MooseJobPrios[MOOSE_OPER_DIR_SEARCH], data);
}

long moose_store_playlist_get_all_loaded(MooseStore * self, MoosePlaylist * stack) {
    g_assert(self);
    g_assert(stack);

    MooseJobData * data = g_new0(MooseJobData, 1);
    data->op = MOOSE_OPER_SPL_LIST;
    data->out_stack = stack;

    return moose_jm_send(self->priv->jm, MooseJobPrios[MOOSE_OPER_SPL_LIST], data);
}

long moose_store_playlist_get_all_known(MooseStore * self, MoosePlaylist * stack) {
    g_assert(self);
    g_assert(stack);

    MooseJobData * data = g_new0(MooseJobData, 1);
    data->op = MOOSE_OPER_SPL_LIST_ALL;
    data->out_stack = stack;

    return moose_jm_send(self->priv->jm, MooseJobPrios[MOOSE_OPER_SPL_LIST_ALL], data);
}

long moose_store_search_to_stack(
    MooseStore * self, const char * match_clause,
    bool queue_only, MoosePlaylist * stack, int limit_len
) {
    MooseJobData * data = g_new0(MooseJobData, 1);
    data->op = MOOSE_OPER_DB_SEARCH;

    data->match_clause = g_strdup(match_clause);
    data->queue_only = queue_only;
    data->length_limit = limit_len;
    data->out_stack = stack;

    return moose_jm_send(self->priv->jm, MooseJobPrios[MOOSE_OPER_DB_SEARCH], data);
}

void moose_store_wait(MooseStore * self) {
    moose_jm_wait(self->priv->jm);
}

void moose_store_wait_for_job(MooseStore * self, int job_id) {
    moose_jm_wait_for_id(self->priv->jm, job_id);
}

MoosePlaylist * moose_store_get_result(MooseStore * self, int job_id) {
    return moose_jm_get_result(self->priv->jm, job_id);
}

MoosePlaylist * moose_store_gw(MooseStore * self, int job_id) {
    moose_store_wait_for_job(self, job_id);
    return moose_store_get_result(self, job_id);
}

void moose_store_release(MooseStore * self) {
    g_assert(self);
    moose_stprv_unlock_attributes(self->priv);
}

MooseSong * moose_store_find_song_by_id(MooseStore * self, unsigned needle_song_id) {
    g_assert(self);

    MooseJobData * data = g_new0(MooseJobData, 1);
    data->op = MOOSE_OPER_FIND_SONG_BY_ID;
    data->needle_song_id = needle_song_id;

    unsigned job_id = moose_jm_send(
                          self->priv->jm, MooseJobPrios[MOOSE_OPER_FIND_SONG_BY_ID], data
                      );
    moose_store_wait_for_job(self, job_id);
    MoosePlaylist * stack = moose_store_get_result(self, job_id);
    if (stack != NULL && moose_playlist_length(stack) > 0) {
        MooseSong * song = moose_playlist_at(stack, 0);
        return song;
    }
    return NULL;
}

MooseStoreCompletion * moose_store_get_completion(MooseStore * self) {
    g_assert(self);

    if (self->priv->completion == NULL) {
        self->priv->completion = g_object_new(
                                     MOOSE_TYPE_STORE_COMPLETION, "store", self->priv, NULL
                                 );
    }
    return self->priv->completion;
}

static void moose_store_init(MooseStore * self) {
    MooseStorePrivate *priv = self->priv = moose_store_get_instance_private(self);

    /* Initialize the Attribute mutex early */
    g_mutex_init(&priv->attr_set_mtx);
    g_mutex_init(&priv->mirrored_mtx);

    /* Initialize the job manager used to background jobs */
    priv->jm = moose_jm_create(moose_store_job_execute_callback, self);

    /* Remember the host/port */
    priv->mirrored_host = g_strdup(moose_client_get_host(priv->client));
    priv->mirrored_port = moose_client_get_port(priv->client);

    /* Do the actual hard work */
    moose_store_buildup(self);

    /* Register for client events */
    g_signal_connect(
        priv->client,
        "client-event",
        G_CALLBACK(moose_store_update_callback),
        self
    );

    /* Register to be notifed when the connection status changes */
    g_signal_connect(
        priv->client,
        "connectivity",
        G_CALLBACK(moose_store_connectivity_callback),
        self
    );
}

/*
 * Close everything down.
 *
 * #1 remove the update watcher
 *    (might be called still otherwise, if client lives longer than the store)
 * #2 free the stack and all associated memory.
 * #3 Save database dump to disk if it hadn't been read from there.
 * #4 Close the handle.
 */
static void moose_store_finalize(GObject * gobject) {
    MooseStore * self = MOOSE_STORE(gobject);
    if (self == NULL) {
        return;
    }

    g_signal_handlers_disconnect_by_func(
        self->priv->client, moose_store_update_callback, self
    );
    g_signal_handlers_disconnect_by_func(
        self->priv->client, moose_store_connectivity_callback, self
    );

    moose_stprv_lock_attributes(self->priv);

    /* Close the job pool (still finishes current operation) */
    moose_jm_close(self->priv->jm);

    moose_store_shutdown(self);

    moose_stprv_unlock_attributes(self->priv);
    g_mutex_clear(&self->priv->attr_set_mtx);
    g_mutex_clear(&self->priv->mirrored_mtx);

    /* NOTE: Settings should be destroyed by caller,
     *       Since it should be valid to call close()
     *       several times.
     */
    g_free(self->priv->mirrored_host);
    g_free(self->priv->db_directory);

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

static void moose_store_get_property(
    GObject * object,
    guint property_id,
    GValue * value,
    GParamSpec * pspec) {
    MooseStorePrivate * priv = MOOSE_STORE(object)->priv;

    switch(property_id) {
    case PROP_CLIENT:
        g_value_set_object(value, priv->client);
        break;
    case PROP_FULL_PLAYLIST:
        g_value_set_object(value, priv->stack);
        break;
    case PROP_USE_COMPRESSION:
        g_value_set_boolean(value, priv->settings.use_compression);
        break;
    case PROP_USE_MEMORY_DB:
        g_value_set_boolean(value, priv->settings.use_memory_db);
        break;
    case PROP_TOKENIZER:
        g_value_set_string(value, priv->settings.tokenizer);
        break;
    case PROP_DB_DIRECTORY:
        g_value_set_string(value, priv->db_directory);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_store_set_property(
    GObject * object,
    guint property_id,
    const GValue * value,
    GParamSpec * pspec) {
    MooseStorePrivate * priv = MOOSE_STORE(object)->priv;

    switch(property_id) {
    case PROP_USE_COMPRESSION:
        priv->settings.use_compression = g_value_get_boolean(value);
        break;
    case PROP_USE_MEMORY_DB:
        priv->settings.use_memory_db = g_value_get_boolean(value);
        break;
    case PROP_TOKENIZER:
        strncpy(
            priv->settings.tokenizer,
            g_value_get_string(value),
            sizeof(priv->settings.tokenizer)
        );
        break;
    case PROP_DB_DIRECTORY:
        g_free(priv->db_directory);
        priv->db_directory = g_value_dup_string(value);
        break;
    case PROP_FULL_PLAYLIST:
    case PROP_CLIENT:
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_store_class_init(MooseStoreClass * klass) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_store_finalize;
    gobject_class->get_property = moose_store_get_property;
    gobject_class->set_property = moose_store_set_property;

    GParamSpec * pspec = NULL;
    pspec = g_param_spec_object(
                "client",
                "Client",
                "Client this store is associated to",
                MOOSE_TYPE_CLIENT,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
            );

    /**
     * MooseStore:client: (type MOOSE_TYPE_CLIENT)
     *
     * Compress the backup on the filesystem?
     * Adv: Less HD usage (12 MB vs. 3 MB)
     * Dis: Little bit slower startup.
     */
    g_object_class_install_property(gobject_class, PROP_CLIENT, pspec);

    pspec = g_param_spec_object(
                "full-playlist",
                "Full playlist",
                "Full playlist with all currently known MooseSongs.",
                MOOSE_TYPE_PLAYLIST,
                G_PARAM_READABLE
            );

    /**
     * MooseStore:full-playlist: (type MOOSE_TYPE_PLAYLIST)
     *
     * Full Playlist.
     */
    g_object_class_install_property(gobject_class, PROP_FULL_PLAYLIST, pspec);

    pspec = g_param_spec_string(
                "db-directory",
                "Database Directory",
                "Directory where the moosecat-cache is stored",
                NULL,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
            );

    /**
     * MooseStore:db-directory: (type utf-8)
     *
     * Directory where the moosecat-cache is stored.
     */
    g_object_class_install_property(gobject_class, PROP_DB_DIRECTORY, pspec);

    pspec = g_param_spec_boolean(
                "use-compression",
                "Use compression",
                "Compress the backup on the filesystem?",
                TRUE, /* default value */
                G_PARAM_READWRITE
            );

    /**
     * MooseStore:use-compression: (type boolean)
     *
     * Compress the backup on the filesystem?
     * Adv: Less HD usage (12 MB vs. 3 MB)
     * Dis: Little bit slower startup.
     */
    g_object_class_install_property(gobject_class, PROP_USE_COMPRESSION, pspec);

    pspec = g_param_spec_boolean(
                "use-memory-db",
                "Use Memory DB",
                "Hold the database in memory?",
                TRUE, /* default value */
                G_PARAM_READWRITE
            );

    /**
     * MooseStore:use-memory-db: (type boolean)
     *
     * Hold the database in memory?
     *
     * Adv: Little bit faster on few systems.
     * Dis: Higher memory usage. (~5-10 MB)
     *
     */
    g_object_class_install_property(gobject_class, PROP_USE_MEMORY_DB, pspec);

    pspec = g_param_spec_string(
                "tokenizer",
                "Tokenizer",
                "Tokernizer to use for FTS",
                "porter", /* default value */
                G_PARAM_READWRITE
            );

    /**
     * MooseStore:tokenizer: (type utf8)
     *
     * Tokenizer to use for FTS.
     *
     * Possible values: porter, default, icu
     *
     */
    g_object_class_install_property(gobject_class, PROP_USE_MEMORY_DB, pspec);
}

MooseStore *moose_store_new(MooseClient *client) {
    return g_object_new(
               MOOSE_TYPE_STORE,
               "client", client,
               NULL
           );
}

MooseStore *moose_store_new_full(
    MooseClient *client,
    const char *db_directory,
    const char *tokenizer,
    bool use_memory_db,
    bool use_compression
) {
    return g_object_new(
               MOOSE_TYPE_STORE,
               "client", client,
               "db-directory", db_directory,
               "tokenizer", tokenizer,
               "use-memory-db", use_memory_db,
               "use-compression", use_compression,
               NULL
           );
}

void moose_store_unref(MooseStore *self) {
    if(self != NULL) {
        g_object_unref(self);
    }
}
