#include "moose-store-private.h"
#include "moose-store-dirs.h"
#include "moose-store-macros.h"

#include "../mpd/signal-helper.h"
#include "../util/paths.h"

#include <glib.h>
#include <string.h>

#define SQL_CODE(NAME) \
    dirs_sql_stmts[SQL_##NAME]

#define SQL_STMT(STORE, NAME) \
    STORE->sql_dir_stmts[SQL_##NAME]

enum {
    SQL_DIR_INSERT = 0,
    SQL_DIR_SEARCH_PATH,
    SQL_DIR_SEARCH_DEPTH,
    SQL_DIR_SEARCH_PATH_AND_DEPTH,
    SQL_DIR_DELETE_ALL,
    SQL_DIR_STMT_COUNT
};

const char *dirs_sql_stmts[] = {
    [SQL_DIR_INSERT] = "INSERT INTO dirs VALUES(?, ?);",
    [SQL_DIR_DELETE_ALL] = "DELETE FROM dirs;",
    [SQL_DIR_SEARCH_PATH] =
    "SELECT -1, path FROM dirs WHERE path MATCH ? UNION "
    "SELECT rowid, uri FROM songs WHERE uri MATCH ? "
    "ORDER BY songs.rowid, songs.uri, dirs.path;",
    [SQL_DIR_SEARCH_DEPTH] =
    "SELECT -1, path FROM dirs WHERE depth = ? UNION "
    "SELECT rowid, uri FROM songs WHERE uri_depth = ? "
    "ORDER BY songs.rowid, songs.uri, dirs.path;",
    [SQL_DIR_SEARCH_PATH_AND_DEPTH] =
    "SELECT -1, path FROM dirs WHERE depth = ? AND path MATCH ? UNION "
    "SELECT rowid, uri FROM songs WHERE uri_depth = ? AND uri MATCH ? "
    "ORDER BY songs.rowid, songs.uri, dirs.path;"
};


///////////////////

void moose_stprv_dir_prepare_statemtents(MooseStore *self)
{
    g_assert(self);

    self->sql_dir_stmts = moose_stprv_prepare_all_statements_listed(
            self, dirs_sql_stmts,
            0, SQL_DIR_STMT_COUNT
    );
}

///////////////////

void moose_stprv_dir_finalize_statements(MooseStore *self)
{
    moose_stprv_finalize_statements(self, self->sql_dir_stmts, 0, SQL_DIR_STMT_COUNT);
    self->sql_dir_stmts = NULL;
}

///////////////////

void moose_stprv_dir_insert(MooseStore *self, const char *path)
{
    g_assert(self);
    g_assert(path);
    int pos_idx = 1;
    int error_id = SQLITE_OK;
    bind_txt(self, DIR_INSERT, pos_idx, path, error_id);
    bind_int(self, DIR_INSERT, pos_idx, moose_path_get_depth(path), error_id);

    if (error_id != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Cannot bind stuff to INSERT statement");
        return;
    }

    if (sqlite3_step(SQL_STMT(self, DIR_INSERT)) != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "cannot INSERT directory");
    }

    /* Make sure bindings are ready for the next insert */
    CLEAR_BINDS_BY_NAME(self, DIR_INSERT);
    sqlite3_reset(SQL_STMT(self, DIR_INSERT));
}

//////////////////

void moose_stprv_dir_delete(MooseStore *self)
{
    g_assert(self);

    if (sqlite3_step(SQL_STMT(self, DIR_DELETE_ALL)) != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "cannot DELETE * from dirs");
    }
    sqlite3_reset(SQL_STMT(self, DIR_DELETE_ALL));
}

//////////////////

int moose_stprv_dir_select_to_stack(MooseStore *self, MoosePlaylist *stack, const char *directory, int depth)
{
    g_assert(self);
    g_assert(stack);
    int returned = 0;
    int pos_idx = 1;
    int error_id = SQLITE_OK;

    if (directory && *directory == '/')
        directory = NULL;

    sqlite3_stmt *select_stmt = NULL;

    if (directory != NULL && depth < 0) {
        select_stmt = SQL_STMT(self, DIR_SEARCH_PATH);
        bind_txt(self, DIR_SEARCH_PATH, pos_idx, directory, error_id);
        bind_txt(self, DIR_SEARCH_PATH, pos_idx, directory, error_id);
    } else if (directory == NULL && depth >= 0) {
        select_stmt = SQL_STMT(self, DIR_SEARCH_DEPTH);
        bind_int(self, DIR_SEARCH_DEPTH, pos_idx, depth, error_id);
        bind_int(self, DIR_SEARCH_DEPTH, pos_idx, depth, error_id);
    } else if (directory != NULL && depth >= 0) {
        select_stmt = SQL_STMT(self, DIR_SEARCH_PATH_AND_DEPTH);
        bind_int(self, DIR_SEARCH_PATH_AND_DEPTH, pos_idx, depth, error_id);
        bind_txt(self, DIR_SEARCH_PATH_AND_DEPTH, pos_idx, directory, error_id);
        bind_int(self, DIR_SEARCH_PATH_AND_DEPTH, pos_idx, depth, error_id);
        bind_txt(self, DIR_SEARCH_PATH_AND_DEPTH, pos_idx, directory, error_id);
    } else {
        return -1;
    }

    if (error_id != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Cannot bind stuff to directory select");
        return -2;
    } else {
        while (sqlite3_step(select_stmt) == SQLITE_ROW) {
            const char *rowid = (const char *) sqlite3_column_text(select_stmt, 0);
            const char *path = (const char *) sqlite3_column_text(select_stmt, 1);
            moose_stack_append(stack, g_strjoin(":", rowid, path, NULL));
            ++returned;
        }

        if (sqlite3_errcode(self->handle) != SQLITE_DONE) {
            REPORT_SQL_ERROR(self, "Some error occured during selecting directories");
            return -3;
        }

        CLEAR_BINDS(select_stmt);
        sqlite3_reset(select_stmt);
    }

    return returned;
}
