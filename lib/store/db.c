#include "db.h"
#include "../mpd/client_private.h"

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
            }
        }

        /* End DB transaction */
        mc_stprv_commit (store);
    }
    END_COMMAND
}

///////////////

mc_StoreDB * mc_store_create (mc_Client * client, const char * directory, const char * dbname)
{
    if (client == NULL)
        return NULL;

    mc_StoreDB * store = g_new0 (mc_StoreDB, 1);
    store->client = client;
    store->stack = mc_store_stack_create ( mpd_stats_get_number_of_songs (client->stats));

    if (dbname == NULL)
        dbname = "moosecat.db";

    if (directory == NULL)
        directory = ".";

    store->db_path = g_strjoin (G_DIR_SEPARATOR_S, directory, dbname, NULL);

    /* -- open actual database connection -- */
    mc_strprv_open_memdb (store);
    mc_stprv_prepare_all_statements (store);
    mc_store_do_list_all_info (store);
    return store;
}

///////////////

int mc_store_update ( mc_StoreDB * self)
{
    (void) self;
    return 0;
}

///////////////

void mc_store_close ( mc_StoreDB * self)
{
    // Write to disk
    (void) self;
}

///////////////

mpd_song ** mc_store_search ( mc_StoreDB * self, struct mc_StoreQuery * qry)
{
    (void) self;
    (void) qry;
    return NULL;
}

int mc_store_search_out ( mc_StoreDB * self, const char * match_clause, mpd_song ** song_buffer, int buf_len)
{
    return mc_stprv_select_out (self, match_clause, song_buffer, buf_len);
}
