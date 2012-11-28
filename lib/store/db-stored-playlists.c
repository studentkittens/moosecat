/*
 * Random thoughts about storing playlists:
 *
 * Create pl table:
 * CREATE TABLE %q (song_idx integer, FOREIGN KEY(song_idx) REFERENCES songs(rowid));
 *
 * Update a playlist:
 * BEGIN IMMEDIATE;
 * DELETE FROM spl_%q;
 * INSERT INTO spl_%q VALUES((SELECT rowid FROM songs_content WHERE c0uri = %Q));
 * ...
 * COMMIT;
 *
 * Select songs from playlist:
 * SELECT rowid FROM songs JOIN spl%q ON songs.rowid = spl_%q.song_idx WHERE artist MATCH ?;
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
 *    or during:
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

#include "db-private.h"
#include "db-stored-playlists.h"
#include "db-macros.h"

#include "../mpd/client-private.h"
#include "../mpd/signal-helper.h"

#include <glib.h>
#include <string.h>

enum {
    STMT_SELECT_TABLES = 0,
    STMT_DROP_SPLTABLE,
    STMT_CREATE_SPLTABLE,
    STMT_INSERT_SONG_IDX,
    STMT_PLAYLIST_SEARCH,
    STMT_PLAYLIST_DUMPIT
};

const char * spl_sql_stmts[] = {
    [STMT_SELECT_TABLES] = "SELECT name FROM sqlite_master WHERE type = 'table' AND name LIKE 'spl_%' ORDER BY name;",
    [STMT_DROP_SPLTABLE] = "DROP TABLE IF EXISTS %q;",
    [STMT_CREATE_SPLTABLE] = "CREATE TABLE IF NOT EXISTS %q (song_idx INTEGER);",
    [STMT_INSERT_SONG_IDX] = "INSERT INTO %q VALUES((SELECT rowid FROM songs_content WHERE c0uri = ?));",
    [STMT_PLAYLIST_SEARCH] = "SELECT songs.rowid FROM songs JOIN %q AS pl ON songs.rowid = pl.song_idx WHERE songs.artist MATCH %Q;",
    [STMT_PLAYLIST_DUMPIT] = "SELECT songs.rowid FROM songs_content AS songs JOIN %q AS pl ON songs.rowid = pl.song_idx;"
};

void mc_stprv_spl_init (mc_StoreDB * self)
{
    g_assert (self);

    if (sqlite3_prepare_v2 (self->handle, spl_sql_stmts[STMT_SELECT_TABLES], -1, &self->spl.select_tables_stmt, NULL) != SQLITE_OK)
        REPORT_SQL_ERROR (self, "Cannot initialize Stored Playlist Support.");

    self->spl.stack = NULL;
}

///////////////////

void mc_stprv_spl_destroy (mc_StoreDB * self)
{
    g_assert (self);

    if (sqlite3_finalize (self->spl.select_tables_stmt) != SQLITE_OK)
        REPORT_SQL_ERROR (self, "Cannot finalize SELECT ALL TABLES statement.");

    if (self->spl.stack != NULL)
        mc_stack_free (self->spl.stack);
}

///////////////////

static char * mc_stprv_spl_construct_table_name (struct mpd_playlist * playlist)
{
    g_assert (playlist);

    return g_strdup_printf ("spl_%x_%s",
                            (unsigned) mpd_playlist_get_last_modified (playlist),
                            mpd_playlist_get_path (playlist) );
}

///////////////////

static char * mc_stprv_spl_parse_table_name (const char * table_name, /* OUT */ time_t * last_modified)
{
    g_assert (table_name);

    for (size_t i = 0; table_name[i] != 0; ++i) {
        if (table_name [i] == '_') {
            if (last_modified != NULL) {
                *last_modified = g_ascii_strtoll (&table_name[i + 1], NULL, 16);
            }

            char * name_begin = strchr (&table_name[i + 1], '_');
            if (name_begin != NULL)
                return g_strdup (name_begin + 1);
            else
                return NULL;
        }
    }

    return NULL;
}

///////////////////

static void mc_stprv_spl_drop_table (mc_StoreDB  * self, const char * table_name)
{
    g_assert (table_name);
    g_assert (self);

    char * sql = sqlite3_mprintf (spl_sql_stmts [STMT_DROP_SPLTABLE], table_name);
    if (sql != NULL) {
        if (sqlite3_exec (self->handle, sql, 0, 0, 0) != SQLITE_OK) {
            REPORT_SQL_ERROR (self, "Cannot DROP some stored playlist table...");
        }

        sqlite3_free (sql);
    }
}

///////////////////

static void mc_stprv_spl_create_table (mc_StoreDB * self, const char * table_name)
{
    g_assert (self);
    g_assert (table_name);

    char * sql = sqlite3_mprintf (spl_sql_stmts [STMT_CREATE_SPLTABLE], table_name);

    if (sql != NULL) {
        if (sqlite3_exec (self->handle, sql, 0, 0, 0) != SQLITE_OK) {
            REPORT_SQL_ERROR (self, "Cannot CREATE some stored playlist table...");
        }
        sqlite3_free (sql);
    }
}

///////////////////

static void mc_stprv_spl_delete_content (mc_StoreDB * self, const char * table_name)
{
    g_assert (self);
    g_assert (table_name);

    mc_stprv_spl_drop_table (self, table_name);
    mc_stprv_spl_create_table (self, table_name);
}

///////////////////

static struct mpd_playlist * mc_stprv_spl_name_to_playlist (mc_StoreDB * store, const char * playlist_name) {
    g_assert (store);

    for (unsigned i = 0; i < mc_stack_length (store->spl.stack); ++i) {
        struct mpd_playlist * playlist = mc_stack_at (store->spl.stack, i);
        if (playlist && g_strcmp0 (playlist_name, mpd_playlist_get_path (playlist) ) == 0) {
            return playlist;
        }
    }

    return NULL;
}

///////////////////

static void mc_stprv_spl_listplaylists (mc_StoreDB * store)
{
    g_assert (store);
    g_assert (store->client);

    mc_Client * self = store->client;

    LOCK_UPDATE_MTX (store);

    if (store->spl.stack != NULL)
        mc_stack_free (store->spl.stack);

    store->spl.stack = mc_stack_create (10, (GDestroyNotify) mpd_playlist_free);

    BEGIN_COMMAND {
        struct mpd_playlist * playlist = NULL;
        mpd_send_list_playlists (conn);

        while ( (playlist = mpd_recv_playlist (conn) ) != NULL) {
            mc_stack_append (store->spl.stack, playlist);
        }

    } END_COMMAND;


    UNLOCK_UPDATE_MTX (store);
}
///////////////////

static GList * mc_stprv_spl_get_loaded_list (mc_StoreDB * self)
{
    GList * table_name_list = NULL;

    /* Select all table names from db that start with spl_ */
    while (sqlite3_step (self->spl.select_tables_stmt) == SQLITE_ROW) {
        table_name_list = g_list_prepend (table_name_list, g_strdup ( (char *)
                                          sqlite3_column_text (self->spl.select_tables_stmt, 0) ) );
    }

    /* Be nice, and always check for errors */
    int error = sqlite3_errcode (self->handle);
    if (error != SQLITE_DONE && error != SQLITE_OK) {
        REPORT_SQL_ERROR (self, "Some error while selecting all playlists");
    } else {
        CLEAR_BINDS (self->spl.select_tables_stmt);
    }

    return table_name_list;
}

/////////////////// PUBLIC AREA ///////////////////

void mc_stprv_spl_update (mc_StoreDB * self)
{
    g_assert (self);

    LOCK_UPDATE_MTX (self);

    mc_stprv_spl_listplaylists (self);

    GList * table_name_list = mc_stprv_spl_get_loaded_list (self);

    /* Filter all Playlist Tables that do not exist anymore in the current playlist stack.
     * I.e. those that were deleted, or accidentally created. */
    for (GList * iter = table_name_list; iter;) {
        char * drop_table_name = iter->data;

        if (drop_table_name != NULL) {
            bool is_valid = false;

            for (unsigned i = 0; i < mc_stack_length (self->spl.stack); ++i) {
                struct mpd_playlist * playlist = mc_stack_at (self->spl.stack, i);

                char * valid_table_name = mc_stprv_spl_construct_table_name (playlist);

                is_valid = (g_strcmp0 (valid_table_name, drop_table_name) == 0);
                g_free (valid_table_name);

                if (is_valid) {
                    break;
                }
            }

            if (is_valid == false) {
                /* drop invalid table */
                mc_shelper_report_progress (self->client, true, "database: Dropping orphaned playlist-table ,,%s''", drop_table_name);
                mc_stprv_spl_drop_table (self, drop_table_name);

                iter = iter->next;
                table_name_list = g_list_delete_link (table_name_list, iter);
                continue;
            }
        }

        iter = iter->next;
    }

    /* Iterate over all loaded mpd_playlists,
     * and find the corresponding table_name.
     *
     * If desired, also call load() on it.  */
    for (unsigned i = 0; i < mc_stack_length (self->spl.stack); ++i) {
        struct mpd_playlist * playlist = mc_stack_at (self->spl.stack, i);

        if (playlist != NULL) {

            /* Find matching playlists by name.
             * If the playlist is too old:
             *    Re-Load it.
             * If the playlist does not exist in the stack anymore:
             *    Drop it.
             */
            for (GList * iter = table_name_list; iter; iter = iter->next) {
                char * old_table_name = iter->data;
                time_t old_pl_time = 0;
                char * old_pl_name = mc_stprv_spl_parse_table_name (old_table_name, &old_pl_time);

                if (g_strcmp0 (old_pl_name, mpd_playlist_get_path (playlist) ) == 0) {
                    time_t new_pl_time = mpd_playlist_get_last_modified (playlist);

                    if (new_pl_time == old_pl_time) {
                        /* nothing has changed */
                    } else if (new_pl_time > old_pl_time) {
                        /* reload old_pl? */
                        mc_stprv_spl_load (self, playlist);
                    } else {
                        /* This should not happen */
                        g_assert_not_reached ();
                    }

                    g_free (old_pl_name);
                    break;
                }

                g_free (old_pl_name);
            }
        }
    }

    mc_shelper_report_operation_finished (self->client, MC_OP_SPL_LIST_UPDATED);

    /* table names were dyn. allocated. */
    g_list_free_full (table_name_list, g_free);

    UNLOCK_UPDATE_MTX (self);
}

///////////////////

void mc_stprv_spl_load (mc_StoreDB * store, struct mpd_playlist * playlist)
{
    g_assert (store);
    g_assert (store->client);
    g_assert (playlist);

    LOCK_UPDATE_MTX (store);

    mc_Client * self = store->client;
    sqlite3_stmt * insert_stmt = NULL;
    char * table_name = mc_stprv_spl_construct_table_name (playlist);
    if (table_name != NULL) {

        mc_stprv_begin (store);

        mc_stprv_spl_delete_content (store, table_name);

        char * sql = sqlite3_mprintf (spl_sql_stmts[STMT_INSERT_SONG_IDX], table_name);
        if (sql != NULL) {
            if (sqlite3_prepare_v2 (store->handle, sql, -1, &insert_stmt, NULL) != SQLITE_OK) {
                REPORT_SQL_ERROR (store, "Cannot prepare INSERT stmt!");
                UNLOCK_UPDATE_MTX (store);
                return;
            }

            mc_shelper_report_progress (self, true, "database: Loading stored playlist ,,%s'' from MPD", table_name);
            sqlite3_free (sql);

            BEGIN_COMMAND {

                if (mpd_send_list_playlist (conn, mpd_playlist_get_path (playlist) ) ) {
                    struct mpd_pair * file_pair = NULL;

                    while ( (file_pair = mpd_recv_pair_named (conn, "file") ) != NULL) {

                        if (sqlite3_bind_text (insert_stmt, 1, file_pair->value, -1, NULL) != SQLITE_OK)
                            REPORT_SQL_ERROR (store, "Cannot bind filename to SPL-Insert");

                        if (sqlite3_step (insert_stmt) != SQLITE_DONE) {
                            REPORT_SQL_ERROR (store, "Cannot insert song index to stored playlist.");
                        }

                        CLEAR_BINDS (insert_stmt);
                    }

                    if (mpd_response_finish (conn) == FALSE) {
                        mc_shelper_report_error (self, conn);
                    } else {
                        mc_shelper_report_operation_finished (self, MC_OP_SPL_UPDATED);
                    }
                }

            } END_COMMAND;

            if (sqlite3_finalize (insert_stmt) != SQLITE_OK)
                REPORT_SQL_ERROR (store, "Cannot finalize INSERT stmt");
        }
        mc_stprv_commit (store);
        g_free (table_name);
    }
    UNLOCK_UPDATE_MTX (store);
}

///////////////////

void mc_stprv_spl_load_by_playlist_name (mc_StoreDB * store, const char * playlist_name)
{
    g_assert (store);
    g_assert (playlist_name);

    struct mpd_playlist * playlist = mc_stprv_spl_name_to_playlist (store, playlist_name);
    if (playlist != NULL) {
        mc_stprv_spl_load (store, playlist);
    } else {
        mc_shelper_report_error_printf (store->client, "Could not find stored playlist ,,%s''", playlist_name);
    }
}

///////////////////

int mc_stprv_spl_select_playlist (mc_StoreDB * store, mc_Stack * out_stack, const char * playlist_name, const char * match_clause)
{
    g_assert (store);
    g_assert (playlist_name);
    g_assert (out_stack);

    int rc = 0;

    if (match_clause && *match_clause == 0)
        match_clause = NULL;

    struct mpd_playlist * playlist = mc_stprv_spl_name_to_playlist (store, playlist_name);
    if (playlist != NULL) {
        char * table_name = mc_stprv_spl_construct_table_name (playlist);
        if (table_name != NULL) {

            char * sql = NULL;
            if (match_clause == NULL) {
                sql = sqlite3_mprintf (spl_sql_stmts[STMT_PLAYLIST_DUMPIT], table_name);
            } else {
                sql = sqlite3_mprintf (spl_sql_stmts[STMT_PLAYLIST_SEARCH], table_name, match_clause);
            }

            if (sql != NULL) {

                sqlite3_stmt * pl_search_stmt = NULL;
                if (sqlite3_prepare_v2 (store->handle, sql, -1, &pl_search_stmt, NULL) != SQLITE_OK) {
                    REPORT_SQL_ERROR (store, "Cannot prepare playlist search statement");
                } else {

                    while (sqlite3_step (pl_search_stmt) == SQLITE_ROW) {
                        int rowid = sqlite3_column_int (pl_search_stmt, 0);
                        struct mpd_song * song = mc_stack_at (store->stack, rowid);

                        if (song != NULL) {
                            mc_stack_append (out_stack, song);
                            ++rc;
                        }
                    }

                    if (sqlite3_errcode (store->handle) != SQLITE_DONE) {
                        REPORT_SQL_ERROR (store, "Error while matching stuff in playlist");
                    }

                    if (sqlite3_finalize (pl_search_stmt) != SQLITE_OK) {
                        REPORT_SQL_ERROR (store, "Error while finalizing match statement");
                    }
                }

                sqlite3_free (sql);
            }
            g_free (table_name);
        }
    }

    return rc;
}

///////////////////

int mc_stprv_spl_get_loaded_playlists (mc_StoreDB * store, mc_Stack * stack)
{
    g_assert (store);
    g_assert (stack);

    int rc = 0;
    GList * table_name_list = mc_stprv_spl_get_loaded_list (store);

    if (table_name_list != NULL) {
        for (GList * iter = table_name_list; iter; iter = iter->next) {
            char * table_name = iter->data;
            if (table_name != NULL) {
                char * playlist_name = mc_stprv_spl_parse_table_name (table_name, NULL);
                if (playlist_name != NULL) {
                    struct mpd_playlist * playlist = mc_stprv_spl_name_to_playlist (store, playlist_name);
                    if (playlist != NULL) {
                        mc_stack_append (stack, playlist);

                        ++rc;
                    }
                    g_free (playlist_name);
                }
                g_free (table_name);
            }
        }
    }

    return rc;
}