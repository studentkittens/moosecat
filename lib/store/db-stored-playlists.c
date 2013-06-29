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

#include "../mpd/client.h"
#include "../mpd/signal-helper.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h> /* bsearch */

enum {
    STMT_SELECT_TABLES = 0,
    STMT_DROP_SPLTABLE,
    STMT_CREATE_SPLTABLE,
    STMT_INSERT_SONG_IDX,
    STMT_PLAYLIST_GETIDS,
    STMT_PLAYLIST_SCOUNT
};

const char *spl_sql_stmts[] = {
    [STMT_SELECT_TABLES] = "SELECT name FROM sqlite_master WHERE type = 'table' AND name LIKE 'spl_%' ORDER BY name;",
    [STMT_DROP_SPLTABLE] = "DROP TABLE IF EXISTS %q;",
    [STMT_CREATE_SPLTABLE] = "CREATE TABLE IF NOT EXISTS %q (song_idx INTEGER);",
    [STMT_INSERT_SONG_IDX] = "INSERT INTO %q VALUES((SELECT rowid FROM songs_content WHERE c0uri = ?));",
    [STMT_PLAYLIST_GETIDS] = "SELECT song_idx FROM %q;",
    [STMT_PLAYLIST_SCOUNT] = "SELECT count(*) FROM %q;"
};

void mc_stprv_spl_init(mc_Store *self)
{
    g_assert(self);

    self->spl.select_tables_stmt = NULL;
    if (sqlite3_prepare_v2(self->handle, spl_sql_stmts[STMT_SELECT_TABLES], -1, &self->spl.select_tables_stmt, NULL) != SQLITE_OK)
        REPORT_SQL_ERROR(self, "Cannot initialize Stored Playlist Support.");

    self->spl.stack = NULL;
}

///////////////////

void mc_stprv_spl_destroy(mc_Store *self)
{
    g_assert(self);

    if (sqlite3_finalize(self->spl.select_tables_stmt) != SQLITE_OK)
        REPORT_SQL_ERROR(self, "Cannot finalize SELECT ALL TABLES statement.");

    if (self->spl.stack != NULL)
        mc_stack_free(self->spl.stack);
}

///////////////////

static char *mc_stprv_spl_construct_table_name(struct mpd_playlist *playlist)
{
    g_assert(playlist);
    return g_strdup_printf("spl_%x_%s",
                           (unsigned) mpd_playlist_get_last_modified(playlist),
                           mpd_playlist_get_path(playlist));
}

///////////////////

static char *mc_stprv_spl_parse_table_name(const char *table_name, /* OUT */ time_t *last_modified)
{
    g_assert(table_name);

    for (size_t i = 0; table_name[i] != 0; ++i) {
        if (table_name [i] == '_') {
            if (last_modified != NULL) {
                *last_modified = g_ascii_strtoll(&table_name[i + 1], NULL, 16);
            }

            char *name_begin = strchr(&table_name[i + 1], '_');

            if (name_begin != NULL)
                return g_strdup(name_begin + 1);
            else
                return NULL;
        }
    }

    return NULL;
}

///////////////////

static void mc_stprv_spl_drop_table(mc_Store   *self, const char *table_name)
{
    g_assert(table_name);
    g_assert(self);
    char *sql = sqlite3_mprintf(spl_sql_stmts [STMT_DROP_SPLTABLE], table_name);

    if (sql != NULL) {
        if (sqlite3_exec(self->handle, sql, 0, 0, 0) != SQLITE_OK) {
            REPORT_SQL_ERROR(self, "Cannot DROP some stored playlist table...");
        }

        sqlite3_free(sql);
    }
}

///////////////////

static void mc_stprv_spl_create_table(mc_Store *self, const char *table_name)
{
    g_assert(self);
    g_assert(table_name);
    char *sql = sqlite3_mprintf(spl_sql_stmts [STMT_CREATE_SPLTABLE], table_name);

    if (sql != NULL) {
        if (sqlite3_exec(self->handle, sql, 0, 0, 0) != SQLITE_OK) {
            REPORT_SQL_ERROR(self, "Cannot CREATE some stored playlist table...");
        }

        sqlite3_free(sql);
    }
}

///////////////////

static void mc_stprv_spl_delete_content(mc_Store *self, const char *table_name)
{
    g_assert(self);
    g_assert(table_name);
    mc_stprv_spl_drop_table(self, table_name);
    mc_stprv_spl_create_table(self, table_name);
}

///////////////////

static struct mpd_playlist *mc_stprv_spl_name_to_playlist(mc_Store *store, const char *playlist_name) {
    g_assert(store);

    struct mpd_playlist * playlist_result = NULL;
    unsigned length = mc_stack_length(store->spl.stack);

    for (unsigned i = 0; i < length; ++i) {
        struct mpd_playlist *playlist = mc_stack_at(store->spl.stack, i);

        if (playlist && g_strcmp0(playlist_name, mpd_playlist_get_path(playlist)) == 0) {
            playlist_result = playlist;
        }
    }

    return playlist_result;
}

///////////////////

static void mc_stprv_spl_listplaylists(mc_Store *store)
{
    g_assert(store);
    g_assert(store->client);
    mc_Client *self = store->client;

    if (store->spl.stack != NULL)
        mc_stack_free(store->spl.stack);

    store->spl.stack = mc_stack_create(10, (GDestroyNotify) mpd_playlist_free);


    struct mpd_connection *conn = mc_proto_get(self);
    if(conn != NULL) {
        struct mpd_playlist *playlist = NULL;
        mpd_send_list_playlists(conn);

        while ((playlist = mpd_recv_playlist(conn)) != NULL) {
            mc_stack_append(store->spl.stack, playlist);
        }

        if (mpd_response_finish(conn) == false) {       
            mc_shelper_report_error(self, conn);        
        }                                               
    } else {
        mc_shelper_report_error_printf(store->client,
                "Cannot get connection to get listplaylists (not connected?)"
        );
    }
    mc_proto_put(self);
}
///////////////////

static GList *mc_stprv_spl_get_loaded_list(mc_Store *self)
{
    GList *table_name_list = NULL;

    /* Select all table names from db that start with spl_ */
    while (sqlite3_step(self->spl.select_tables_stmt) == SQLITE_ROW) {
        table_name_list = g_list_prepend(table_name_list, g_strdup((char *)
                                         sqlite3_column_text(self->spl.select_tables_stmt, 0)));
    }

    /* Be nice, and always check for errors */
    int error = sqlite3_errcode(self->handle);

    if (error != SQLITE_DONE && error != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Some error while selecting all playlists");
    } else {
        CLEAR_BINDS(self->spl.select_tables_stmt);
    }

    return table_name_list;
}

///////////////////

static int mc_stprv_spl_get_song_count(mc_Store *self, struct mpd_playlist *playlist)
{
    g_assert(self);

    int count = -1;
    char *table_name = mc_stprv_spl_construct_table_name(playlist);

    if(table_name != NULL) {
        char *dyn_sql = sqlite3_mprintf(spl_sql_stmts[STMT_PLAYLIST_SCOUNT], table_name);

        if(dyn_sql != NULL) {
            sqlite3_stmt *count_stmt = NULL;

            if (sqlite3_prepare_v2(self->handle, dyn_sql, -1, &count_stmt, NULL) != SQLITE_OK) {
                REPORT_SQL_ERROR(self, "Cannot prepare COUNT stmt!");
            } else {
                if(sqlite3_step(count_stmt) == SQLITE_ROW) {
                    count = sqlite3_column_int(count_stmt, 0);
                }

                if (sqlite3_step(count_stmt) != SQLITE_DONE) {
                    REPORT_SQL_ERROR(self, "Cannot count songs in stored playlist table.");
                }
            }

            sqlite3_free(dyn_sql);
        }

        g_free(table_name);
    }
    return count;
}

/////////////////// PUBLIC AREA ///////////////////

void mc_stprv_spl_update(mc_Store *self)
{
    g_assert(self);

    mc_stprv_spl_listplaylists(self);
    GList *table_name_list = mc_stprv_spl_get_loaded_list(self);

    /* Filter all Playlist Tables that do not exist anymore in the current playlist stack.
     * i.e. those that were deleted, or accidentally created. */
    for (GList *iter = table_name_list; iter;) {
        char *drop_table_name = iter->data;

        if (drop_table_name != NULL) {
            bool is_valid = false;

            for (unsigned i = 0; i < mc_stack_length(self->spl.stack); ++i) {
                struct mpd_playlist *playlist = mc_stack_at(self->spl.stack, i);
                char *valid_table_name = mc_stprv_spl_construct_table_name(playlist);
                is_valid = (g_strcmp0(valid_table_name, drop_table_name) == 0);
                g_free(valid_table_name);

                if (is_valid) {
                    break;
                }
            }

            if (is_valid == false) {
                /* drop invalid table */
                mc_shelper_report_progress(self->client, true,
                                           "database: Dropping orphaned playlist-table ,,%s''",
                                           drop_table_name);
                mc_stprv_spl_drop_table(self, drop_table_name);
                iter = iter->next;
                table_name_list = g_list_delete_link(table_name_list, iter);
                continue;
            }
        }

        iter = iter->next;
    }

    /* Iterate over all loaded mpd_playlists,
     * and find the corresponding table_name.
     *
     * If desired, also call load() on it.  */
    for (unsigned i = 0; i < mc_stack_length(self->spl.stack); ++i) {
        struct mpd_playlist *playlist = mc_stack_at(self->spl.stack, i);

        if (playlist != NULL) {
            /* Find matching playlists by name.
             * If the playlist is too old:
             *    Re-Load it.
             * If the playlist does not exist in the stack anymore:
             *    Drop it.
             */
            for (GList *iter = table_name_list; iter; iter = iter->next) {
                char *old_table_name = iter->data;
                time_t old_pl_time = 0;
                char *old_pl_name = mc_stprv_spl_parse_table_name(old_table_name, &old_pl_time);

                if (g_strcmp0(old_pl_name, mpd_playlist_get_path(playlist)) == 0) {
                    time_t new_pl_time = mpd_playlist_get_last_modified(playlist);

                    if (new_pl_time == old_pl_time) {
                        /* nothing has changed */
                    } else if (new_pl_time > old_pl_time) {
                        /* reload old_pl? */
                        mc_stprv_spl_load(self, playlist);
                    } else {
                        /* This should not happen */
                        g_assert_not_reached();
                    }

                    g_free(old_pl_name);
                    break;
                }

                g_free(old_pl_name);
            }
        }
    }

    /* table names were dyn. allocated. */
    g_list_free_full(table_name_list, g_free);

    mc_shelper_report_operation_finished(self->client, MC_OP_SPL_LIST_UPDATED);
}

///////////////////

bool mc_stprv_spl_is_loaded(mc_Store *store, struct mpd_playlist *playlist)
{
    bool result = false;

    /* Construct the table name for comparasion */
    char *table_name = mc_stprv_spl_construct_table_name(playlist);
    GList *table_names = mc_stprv_spl_get_loaded_list(store);

    for(GList *iter = table_names; iter != NULL; iter = iter->next) {
        char *comp_table_name = iter->data;
        if(g_ascii_strcasecmp(comp_table_name, table_name) == 0) {

            /* Get the actual playlist name from the table name and the last_mod date */
            time_t comp_last_mod = 0;
            g_free(mc_stprv_spl_parse_table_name(
                    comp_table_name, &comp_last_mod
            ));
            
            if(comp_last_mod == mpd_playlist_get_last_modified(playlist)) {
                result = true;
                break;
            }
        }
    }

    g_free(table_name);
    g_list_free_full(table_names, g_free);

    return result;
}

///////////////////

void mc_stprv_spl_load(mc_Store *store, struct mpd_playlist *playlist)
{
    g_assert(store);
    g_assert(store->client);
    g_assert(playlist);
    
    mc_Client *self = store->client;

    if(mc_stprv_spl_is_loaded(store, playlist)) {
        mc_shelper_report_progress(
                self, true,
                "database: Stored playlist already loaded - skipping."
        );
        return;
    }

    sqlite3_stmt *insert_stmt = NULL;
    char *table_name = mc_stprv_spl_construct_table_name(playlist);

    if (table_name != NULL) {
        mc_stprv_begin(store);
        mc_stprv_spl_delete_content(store, table_name);
        char *sql = sqlite3_mprintf(spl_sql_stmts[STMT_INSERT_SONG_IDX], table_name);

        if (sql != NULL) {
            if (sqlite3_prepare_v2(store->handle, sql, -1, &insert_stmt, NULL) != SQLITE_OK) {
                REPORT_SQL_ERROR(store, "Cannot prepare INSERT stmt!");
                return;
            }

            mc_shelper_report_progress(self, true, "database: Loading stored playlist ,,%s'' from MPD", table_name);
            sqlite3_free(sql);

            /* Acquire the connection (this locks the connection for others) */
            struct mpd_connection *conn = mc_proto_get(self);
            if(conn != NULL) {

                if (mpd_send_list_playlist(conn, mpd_playlist_get_path(playlist))) {
                    struct mpd_pair *file_pair = NULL;

                    while ((file_pair = mpd_recv_pair_named(conn, "file")) != NULL) {
                        if (sqlite3_bind_text(insert_stmt, 1, file_pair->value, -1, NULL) != SQLITE_OK)
                            REPORT_SQL_ERROR(store, "Cannot bind filename to SPL-Insert");

                        if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
                            REPORT_SQL_ERROR(store, "Cannot insert song index to stored playlist.");
                        }

                        CLEAR_BINDS(insert_stmt);
                    }

                    if (mpd_response_finish(conn) == FALSE) {
                        mc_shelper_report_error(self, conn);
                    } else {
                        mc_shelper_report_operation_finished(self, MC_OP_SPL_UPDATED);
                    }
                }
            }

            /* Release the connection mutex */
            mc_proto_put(self);

            if (sqlite3_finalize(insert_stmt) != SQLITE_OK)
                REPORT_SQL_ERROR(store, "Cannot finalize INSERT stmt");
        }

        mc_stprv_commit(store);
        g_free(table_name);
    }

}

///////////////////

void mc_stprv_spl_load_by_playlist_name(mc_Store *store, const char *playlist_name)
{
    g_assert(store);
    g_assert(playlist_name);
    struct mpd_playlist *playlist = mc_stprv_spl_name_to_playlist(store, playlist_name);

    if (playlist != NULL) {
        mc_stprv_spl_load(store, playlist);
    } else {
        mc_shelper_report_error_printf(store->client, "Could not find stored playlist ,,%s''", playlist_name);
    }
}

///////////////////

static inline int mc_stprv_spl_hash_compare(gconstpointer a, gconstpointer b)
{
    return *((gconstpointer **)a) - *((gconstpointer **)b);
}

///////////////////

static int mc_stprv_spl_bsearch_cmp(const void *key, const void *array)
{
    return key - *(void **)array;
}

///////////////////

static int mc_stprv_spl_filter_id_list(mc_Store *store, GPtrArray *song_ptr_array, const char *match_clause, mc_Stack *out_stack)
{
    /* Roughly estimate the number of song pointer to expect */
    int preallocations = mc_stack_length(store->stack) /
                        (((match_clause) ? MAX(strlen(match_clause), 1) : 1) * 2);

    /* Temp. container to hold the query on the database */
    mc_Stack *db_songs = mc_stack_create(preallocations, NULL);

    if(mc_stprv_select_to_stack(store, match_clause, false, db_songs, -1) > 0) {
        for(size_t i = 0; i < mc_stack_length(db_songs); ++i) {
            struct mpd_song *song = mc_stack_at(db_songs, i);
            if(bsearch(
                        song, 
                        song_ptr_array->pdata, 
                        song_ptr_array->len,
                        sizeof(gpointer),
                        mc_stprv_spl_bsearch_cmp
                    )
            )
            {
                mc_stack_append(out_stack, song);
            }
        }
    }

    /* Set them free */
    mc_stack_free(db_songs);

    return mc_stack_length(out_stack);
}

///////////////////

int mc_stprv_spl_select_playlist(mc_Store *store, mc_Stack *out_stack, const char *playlist_name, const char *match_clause)
{
    g_assert(store);
    g_assert(playlist_name);
    g_assert(out_stack);

    /*
     * Algorithm:
     *
     * ids = sort('select id from spl_xzy_1234')
     * songs = queue_search(match_clause)
     * for song in songs:
     *     if bsearch(songs, song.id) == FOUND:
     *          results.append(song)
     *          
     * return len(songs)
     *
     *
     * (JOIN is much more expensive, that's why.)
     */

    int rc = 0;
    char *table_name, *dyn_sql;
    GPtrArray *song_ptr_array = NULL;
    sqlite3_stmt *pl_id_stmt = NULL;
    struct mpd_playlist *playlist;
    
    if (match_clause && *match_clause == 0) {
        match_clause = NULL;
    }

    /* Try to find the playlist structure in the playlist_struct list by name */
    if ((playlist = mc_stprv_spl_name_to_playlist(store, playlist_name)) == NULL) {
        return -1;
    }

    /* Use the acquired playlist to find out the correct playlist name */ 
    if ((table_name = mc_stprv_spl_construct_table_name(playlist)) == NULL) {
        return -2;
    }

    /* Use the table name to construct a select on this table */
    if ((dyn_sql = sqlite3_mprintf(spl_sql_stmts[STMT_PLAYLIST_GETIDS], table_name)) == NULL) {
        sqlite3_free(table_name);
        return -3;
    }

    if (sqlite3_prepare_v2(store->handle, dyn_sql, -1, &pl_id_stmt, NULL) != SQLITE_OK) {
        REPORT_SQL_ERROR(store, "Cannot prepare playlist search statement");
    } else {
        int song_count = mc_stprv_spl_get_song_count(store, playlist);
        song_ptr_array = g_ptr_array_sized_new(MAX(song_count, 1));

        while (sqlite3_step(pl_id_stmt) == SQLITE_ROW) {
            /* Get the actual song */
            int song_idx = sqlite3_column_int(pl_id_stmt, 0);
            struct mpd_song *song = mc_stack_at(store->stack, song_idx - 1);

            /* Remember the pointer */
            g_ptr_array_add(song_ptr_array, song);
        }

        if (sqlite3_errcode(store->handle) != SQLITE_DONE) {
            REPORT_SQL_ERROR(store, "Error while matching stuff in playlist");
        } else {
            if (sqlite3_finalize(pl_id_stmt) != SQLITE_OK) {
                REPORT_SQL_ERROR(store, "Error while finalizing getid statement");
            } else {
                g_ptr_array_sort(song_ptr_array, mc_stprv_spl_hash_compare);
                rc = mc_stprv_spl_filter_id_list(store, song_ptr_array, match_clause, out_stack);
            }
        }

        g_ptr_array_free(song_ptr_array, true);
    }

    g_free(table_name);
    sqlite3_free(dyn_sql);
    
    
    return rc;
}

///////////////////

int mc_stprv_spl_get_loaded_playlists(mc_Store *store, mc_Stack *stack)
{
    g_assert(store);
    g_assert(stack);
    int rc = 0;
    GList *table_name_list = mc_stprv_spl_get_loaded_list(store);

    if (table_name_list != NULL) {
        for (GList *iter = table_name_list; iter; iter = iter->next) {
            char *table_name = iter->data;

            if (table_name != NULL) {
                char *playlist_name = mc_stprv_spl_parse_table_name(table_name, NULL);

                if (playlist_name != NULL) {
                    struct mpd_playlist *playlist = mc_stprv_spl_name_to_playlist(store, playlist_name);

                    if (playlist != NULL) {
                        mc_stack_append(stack, playlist);
                        ++rc;
                    }

                    g_free(playlist_name);
                }

                g_free(table_name);
            }
        }
    }

    return rc;
}
