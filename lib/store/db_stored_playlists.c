
/*
 * Random thoughts about storing playlists:
 *
 * Create pl table:
 * CREATE TABLE spl_%q (song_idx integer, FOREIGN KEY(song_idx) REFERENCES songs(rowid));
 * 
 * Update a playlist:
 * BEGIN IMMEDIATE;
 * DELETE FROM spl_%q;
 * INSERT INTO spl_%q VALUES((SELECT rowid FROM songs_content WHERE c0uri = %Q)); 
 * ...
 * COMMIT;
 *
 * Select songs from playlist:
 * SELECT rowid FROM songs JOIN spl_%q ON songs.rowid = spl_%q.song_idx WHERE artist MATCH ?;
 *  
 *
 * Delete playlist:
 * DROP spl_%q;
 *
 * List all loaded playlists:
 * SELECT name FROM sqlite_master WHERE type = 'table' AND name LIKE 'spl_%' ORDER BY name;
 *
 * The plan:
 * On startup OR on MPD_IDLE_STORED_PLAYLIST:
 *    Get all playlists, either through:
 *        listplaylists
 *    or during 
 *        listallinfo
 *
 *    The mpd_playlist objects are stored in a stack.
 *
 *    Sync those with the database, i.e. playlists that are not up-to-date,
 *    or were deleted are dropped. New playlists are __not__ created.
 *    If the playlist was out of date it gets recreated.
 *   
 * On playlist load:
 *    Send 'listplaylist <pl_name>', create a table named spl_<pl_name>,
 *    and fill in all the song-ids
 */

///////////////////

#include "db_private.h"
#include "../mpd/client_private.h"
#include "../mpd/signal_helper.h"

#include <glib.h>
#include <string.h>

enum {
    STMT_SELECT_TABLES = 0
};

const char * spl_sql_stmts[] = {
    [STMT_SELECT_TABLES] = "SELECT name FROM sqlite_master WHERE type = 'table' AND name LIKE 'spl_%' ORDER BY name;"
};

void mc_stprv_spl_init (mc_StoreDB * self)
{
    g_assert (self);

    if (sqlite3_prepare_v2 (self->handle, spl_sql_stmts[STMT_SELECT_TABLES], -1, &self->spl.select_tables_stmt, NULL) != SQLITE_OK)
        REPORT_SQL_ERROR (self, "Cannot initialize Stored Playlist Support.");
}

///////////////////

static char * mc_stprv_spl_create_table_name (struct mpd_playlist * playlist)
{
    g_assert (playlist);

    return g_strdup_printf ("%s_%x",
            mpd_playlist_get_path (playlist),
            (unsigned) mpd_playlist_get_last_modified (playlist));
}

///////////////////

static char * mc_stprv_spl_parse_table_name (const char * table_name, /* OUT */ time_t * last_modified)
{
    g_assert (table_name);

    size_t table_name_len = strlen (table_name);
    for (size_t i = table_name_len; i != 0; --i) {
        if (table_name [i] == '_') {
            if (last_modified != NULL) {
                *last_modified = g_ascii_strtoll (&table_name[i + 1], NULL, 16);
            }
            return (i == 0) ? NULL : g_strndup (table_name, i - 1);
        }
    }

    return NULL;
}

/////////////////// PUBLIC AREA /////////////////// 

void mc_stprv_spl_update (mc_StoreDB * self)
{
    g_assert (self);

    LOCK_UPDATE_MTX (self);

    mc_stprv_begin (self);

    while (sqlite3_step (self->spl.select_tables_stmt) == SQLITE_ROW) {
        char * table_name = (char *) sqlite3_column_name (self->spl.select_tables_stmt, 0);
        if (table_name != NULL) {
            bool found = false;

            time_t last_modified = 0;
            char * pl_name = mc_stprv_spl_parse_table_name (table_name, &last_modified);

            if (pl_name != NULL && last_modified != 0) {
                for (unsigned i = 0; i < mc_store_stack_length (self->spl.stack); ++i) {
                    struct mpd_playlist * pl = mc_store_stack_at (self->spl.stack, i);
                    if (pl != NULL) {
                        if (g_strcmp0 (mpd_playlist_get_path(pl), pl_name) == 0) {
                            found = true;

                            if (last_modified < mpd_playlist_get_last_modified (pl)) {
                                /* call load(tabe_name) here */
                            }
                            break;
                        }
                    }
                }
            }

            if (found == false) {
                /* drop table */
            }
        }
    }

    int error = sqlite3_errcode(self->handle);
    if (error != SQLITE_DONE && error != SQLITE_OK) {
        REPORT_SQL_ERROR (self, "Some error while selecting all playlists");
    } 

    mc_stprv_commit (self);
    UNLOCK_UPDATE_MTX (self);
}

///////////////////

#if 0
void mc_stprv_spl_load (mc_StoreDB * store, const char * pl_name, bool lock_self, bool do_commit)
{
    if (lock_self) {
        LOCK_UPDATE_MTX (store);
    }

    mc_Client * self = store->client;

    BEGIN_COMMAND {

        int pos_idx = 1;
        int error_id = 0;

        sqlite3_stmt * update_song_stmt = SQL_STMT (store, SPL_UPDATE_SONG);
        g_assert (update_song_stmt);

        if (do_commit) {
            mc_stprv_begin (store);
        }

        mc_shelper_report_progress (self, true, "database: Loading stored playlist ,,%s'' from MPD", pl_name);
        if (mpd_send_list_playlist (conn, pl_name) ) {
            struct mpd_pair * file_pair = NULL;

            while ( (file_pair = mpd_recv_pair_named (conn, "file")) != NULL) {
                bind_txt (store, SPL_UPDATE_SONG, pos_idx, "1 2 3", error_id); // TODO: actual value?
                bind_txt (store, SPL_UPDATE_SONG, pos_idx, file_pair->value, error_id);

                if (sqlite3_step (update_song_stmt) != SQLITE_DONE) {
                    REPORT_SQL_ERROR (store, "Cannot update spl_info of songs table");
                }
            }

            if (mpd_response_finish (conn) == FALSE) {
                mc_shelper_report_error (self, conn);
            }
        }

        if (do_commit) {
            mc_stprv_commit (store);
        }
    }
    END_COMMAND;

    if (lock_self) {
        UNLOCK_UPDATE_MTX (store);
    }
}
#endif

///////////////////
