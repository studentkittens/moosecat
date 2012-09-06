#include "db.h"
#include "../mpd/client_private.h"

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
    if (g_file_test (self->db_path, G_FILE_TEST_EXISTS) == TRUE)
    {
        /* check #2 */
        if (sqlite3_open (self->db_path, &self->handle) == SQLITE_OK)
        {
            mc_stprv_prepare_all_statements (self);

            /* check #3 */
            char * cached_hostname = mc_stprv_get_mpd_host (self);
            int cached_port = mc_stprv_get_mpd_port (self);

            if (cached_port == self->client->_port &&
                    g_strcmp0 (cached_hostname, self->client->_host) == 0)
            {
                /* check #4 */
                size_t cached_db_version = mc_stprv_get_db_version (self);
                size_t cached_sc_version = mc_stprv_get_sc_version (self);

                if (cached_db_version == mpd_stats_get_db_update_time (self->client->stats) &&
                        cached_sc_version == MC_DB_SCHEMA_VERSION)
                {
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
*       Also, this value is adjustable in the config.
*/
static void mc_store_do_list_all_info (mc_StoreDB * store)
{
    mc_Client * self = store->client;
    BEGIN_COMMAND
    {
        struct mpd_entity * ent = NULL;
        const struct mpd_song * song = NULL;

        /* Order the real big list */
        mpd_send_list_all_meta (conn, "/");

        /* Start a DB transaction */
        mc_stprv_begin (store);

        /* Now get the results */
        while ( (ent = mpd_recv_entity (conn) ) != NULL)
        {
            if (mpd_entity_get_type (ent) == MPD_ENTITY_TYPE_SONG)
            {
                song = mpd_entity_get_song (ent);

                /* Store metadata */
                mc_store_stack_append (store->stack, (mpd_song *) song);
                mc_stprv_insert_song (store, (mpd_song *) song);
            }
            else
            {
                mpd_entity_free (ent);
                ent = NULL;
            }
        }

        /* End DB transaction */
        mc_stprv_commit (store);
    }
    END_COMMAND
}

///////////////
/// PUBLIC ////
///////////////

mc_StoreDB * mc_store_create (mc_Client * client, const char * directory, const char * dbname)
{
    if (client == NULL)
        return NULL;

    /* allocated memory for the mc_StoreDB struct */
    mc_StoreDB * store = g_new0 (mc_StoreDB, 1);

    /* client is used to keep the db content updated */
    store->client = client;

    if (dbname == NULL)
        dbname = "moosecat.db";

    if (directory == NULL)
        directory = ".";

    /* create the full path to the db */
    store->db_path = g_strjoin (G_DIR_SEPARATOR_S, directory, dbname, NULL);

    int song_count = -1;
    if ( (song_count = mc_store_check_if_db_is_still_valid (store) ) < 0)
    {
        g_print ("database: will fetch it from mpd.\n");

        /* open a sqlite handle, pointing to a database, either a new one will be created,
         * or an backup will be loaded into memory */
        mc_strprv_open_memdb (store);
        mc_stprv_prepare_all_statements (store);
        /* stack is preallocated to fit the whole db in */
        store->stack = mc_store_stack_create (
                           mpd_stats_get_number_of_songs (client->stats) + 1
                       );

        /* we need to query mpd */
        mc_store_do_list_all_info (store);

        /* It's new, so replace the one on the disk, if it's there */
        store->write_to_disk = TRUE;
    }
    else
    {

        /* open a sqlite handle, pointing to a database, either a new one will be created,
         * or an backup will be loaded into memory */
        mc_strprv_open_memdb (store);
        mc_stprv_prepare_all_statements (store);
        g_print ("database: %s exists already.\n", store->db_path);

        /* stack is allocated to the old size */
        store->stack = mc_store_stack_create (song_count + 1);

        /* load the old database into memory */
        mc_stprv_load_or_save (store, false);

        /* deserialize all songs from there */
        mc_stprv_deserialize_songs (store);

        /* try to keep the old database,
         * saves write time, and might reduce bugs,
         * when songs are deserialized wrong
         */
        store->write_to_disk = FALSE;
    }

    return store;
}

///////////////

void mc_store_close (mc_StoreDB * self)
{
    if (self == NULL)
        return;

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
    return mc_stprv_select_out (self, match_clause, song_buffer, buf_len);
}
