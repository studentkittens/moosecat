#include "moose-store-private.h"
#include "moose-store-macros.h"
#include "moose-store-query-parser.h"

#include "../mpd/moose-mpd-signal-helper.h"
#include "../mpd/moose-mpd-update.h"
#include "../mpd/moose-song-private.h"

/* strlen() */
#include <string.h>

/* strtol() */
#include <stdlib.h>

/* gmtime(), strftime() */
#ifndef _POSIX_SOURCE
    #define _POSIX_SOURCE
#endif
#include <time.h>

#define SQL_CODE(NAME) \
    _sql_stmts[STMT_SQL_ ## NAME]

#define SQL_STMT(STORE, NAME) \
    STORE->sql_prep_stmts[STMT_SQL_ ## NAME]

/*
 * Note:
 * =====
 *
 * Database code is always a bit messy apparently.
 * I tried my best to keep this readable.
 *
 * Please excuse any eye cancer while reading this. Thank you!
 */

enum {
    /* creation of the basic tables */
    STMT_SQL_CREATE = 0,
    /* create the dummy meta table
     * (just to let the statements compile) */
    STMT_SQL_META_DUMMY,
    /* update info about the db */
    STMT_SQL_META,
    /* === border between non-prepared/prepared statements === */
    STMT_SQL_NEED_TO_PREPARE_COUNT,
    /* ======================================================= */
    /* update queue_pos / queue_idx */
    STMT_SQL_QUEUE_INSERT_ROW,
    /* clear the pos/id fields */
    STMT_SQL_QUEUE_CLEAR,
    /* select using match */
    STMT_SQL_SELECT_MATCHED,
    STMT_SQL_SELECT_MATCHED_ALL,
    /* select all */
    STMT_SQL_SELECT_ALL,
    /* select all queue songs */
    STMT_SQL_SELECT_ALL_QUEUE,
    /* delete all content from 'songs' */
    STMT_SQL_DELETE_ALL,
    /* select meta attributes */
    STMT_SQL_SELECT_META_DB_VERSION,
    STMT_SQL_SELECT_META_PL_VERSION,
    STMT_SQL_SELECT_META_SC_VERSION,
    STMT_SQL_SELECT_META_MPD_PORT,
    STMT_SQL_SELECT_META_MPD_HOST,
    /* count songs in db */
    STMT_SQL_COUNT,
    /* insert one song */
    STMT_SQL_INSERT,
    /* begin statement */
    STMT_SQL_BEGIN,
    /* commit statement */
    STMT_SQL_COMMIT,
    /* === total number of defined sources === */
    STMT_SQL_SOURCE_COUNT
    /* ======================================= */
};

///////////////////

/* this enum mirros the order in the CREATE statement */
enum {
    SQL_COL_URI = 0,
    SQL_COL_START,
    SQL_COL_END,
    SQL_COL_DURATION,
    SQL_COL_LAST_MODIFIED,
    SQL_COL_ARTIST,
    SQL_COL_ALBUM,
    SQL_COL_TITLE,
    SQL_COL_ALBUM_ARTIST,
    SQL_COL_TRACK,
    SQL_COL_NAME,
    SQL_COL_GENRE,
    SQL_COL_DATE,
    SQL_COL_COMPOSER,
    SQL_COL_PERFORMER,
    SQL_COL_COMMENT,
    SQL_COL_DISC,
    SQL_COL_MUSICBRAINZ_ARTIST_ID,
    SQL_COL_MUSICBRAINZ_ALBUM_ID,
    SQL_COL_MUSICBRAINZ_ALBUMARTIST_ID,
    SQL_COL_MUSICBRAINZ_TRACK_ID,
    SQL_COL_ALWAYS_DUMMY
};

///////////////////
static const char * _sql_stmts[] = {
    [STMT_SQL_CREATE] =
        "PRAGMA foreign_keys = ON;                                                                          \n"
        "-- Note: Type information is not parsed at all, and rather meant as hint for the developer.        \n"
        "CREATE VIRTUAL TABLE IF NOT EXISTS songs USING fts4(                                               \n"
        "    uri            TEXT UNIQUE NOT NULL,     -- Path to file, or URL to webradio etc.              \n"
        "    start          INTEGER NOT NULL,         -- Start of the virtual song within the physical file \n"
        "    end            INTEGER NOT NULL,         -- End of the virtual song within the physical file   \n"
        "    duration       INTEGER NOT NULL,         -- Songudration in Seconds. (0 means unknown)         \n"
        "    last_modified  INTEGER NOT NULL,         -- Last modified as Posix UTC Timestamp               \n"
        "    -- Tags (all tags are saved, regardless if used or not):                                       \n"
        "    artist         TEXT,                                                                           \n"
        "    album          TEXT,                                                                           \n"
        "    title          TEXT,                                                                           \n"
        "    album_artist   TEXT,                                                                           \n"
        "    track          TEXT,                                                                           \n"
        "    name           TEXT,                                                                           \n"
        "    genre          TEXT,                                                                           \n"
        "    date           TEXT,                                                                           \n"
        "    composer       TEXT,                                                                           \n"
        "    performer      TEXT,                                                                           \n"
        "    comment        TEXT,                                                                           \n"
        "    disc           TEXT,                                                                           \n"
        "    -- MB-Tags:                                                                                    \n"
        "    musicbrainz_artist_id       TEXT,                                                              \n"
        "    musicbrainz_album_id        TEXT,                                                              \n"
        "    musicbrainz_albumartist_id  TEXT,                                                              \n"
        "    musicbrainz_track           TEXT,                                                              \n"
        "    -- Constant Value. Useful for a MATCH clause that selects everything.                          \n"
        "    always_dummy INTEGER,                                                                          \n"
        "    -- Depth of uri:                                                                               \n"
        "    uri_depth INTEGER NOT NULL,                                                                    \n"
        "    -- FTS options:                                                                                \n"
        "    matchinfo=fts3, tokenize=%s                                                                    \n"
        ");                                                                                                 \n"
        "-- This is a bit of a hack, but necessary, without UPDATE is awfully slow.                         \n"
        "CREATE UNIQUE INDEX IF NOT EXISTS queue_uri_index ON songs_content(c0uri);                         \n"
        "                                                                                                   \n"
        "-- A list of Queue contents (similar to a stored playlist, but not dynamic)                        \n"
        "CREATE TABLE IF NOT EXISTS queue(song_idx INTEGER, pos INTEGER UNIQUE, idx INTEGER UNIQUE);        \n"
        "CREATE INDEX IF NOT EXISTS queue_pos_index ON queue(pos);                                          \n"
        "                                                                                                   \n"
        "CREATE VIRTUAL TABLE IF NOT EXISTS dirs USING fts4(path TEXT NOT NULL, depth INTEGER NOT NULL);    \n",
    [STMT_SQL_META_DUMMY] =
        "CREATE TABLE IF NOT EXISTS meta(db_version, pl_version, sc_version, mpd_port, mpd_host); \n",
    [STMT_SQL_META] =
        "-- Meta Table, containing information about the metadata itself.                \n"
        "BEGIN IMMEDIATE;                                                                \n"
        "DROP TABLE IF EXISTS meta;                                                      \n"
        "CREATE TABLE meta(                                                              \n"
        "      db_version    INTEGER,  -- last db update from statistics                 \n"
        "      pl_version    INTEGER,  -- queue version from status                      \n"
        "      sc_version    INTEGER,  -- schema version                                 \n"
        "      mpd_port      INTEGER,  -- analogous, the port.                           \n"
        "      mpd_host      TEXT      -- the hostname that this table was created from. \n"
        ");                                                                              \n"
        "INSERT INTO meta VALUES(%d, %d, %d, %d, '%q');                                  \n"
        "COMMIT;                                                                         \n",
    /* binding column names does not seem to work well.. */
    [STMT_SQL_SELECT_META_DB_VERSION] =
        "SELECT db_version FROM meta;",
    [STMT_SQL_SELECT_META_PL_VERSION] =
        "SELECT pl_version FROM meta;",
    [STMT_SQL_SELECT_META_SC_VERSION] =
        "SELECT sc_version FROM meta;",
    [STMT_SQL_SELECT_META_MPD_PORT] =
        "SELECT mpd_port FROM meta;",
    [STMT_SQL_SELECT_META_MPD_HOST] =
        "SELECT mpd_host FROM meta;",
    [STMT_SQL_COUNT] =
        "SELECT count(*) FROM songs;",
    [STMT_SQL_INSERT] =
        "INSERT INTO songs VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
    [STMT_SQL_QUEUE_INSERT_ROW] =
        "INSERT INTO queue(song_idx, pos, idx) VALUES((SELECT rowid FROM songs_content WHERE c0uri = ?), ?, ?);",
    [STMT_SQL_QUEUE_CLEAR] =
        "DELETE FROM queue WHERE pos > ?;",
    [STMT_SQL_SELECT_MATCHED] =
        "SELECT rowid FROM songs WHERE artist MATCH ? LIMIT ?;",
    [STMT_SQL_SELECT_MATCHED_ALL] =
        "SELECT rowid FROM songs;",
    [STMT_SQL_SELECT_ALL] =
        "SELECT * FROM songs;",
    [STMT_SQL_SELECT_ALL_QUEUE] =
        "SELECT song_idx, pos, idx FROM queue;",
    [STMT_SQL_DELETE_ALL] =
        "DELETE FROM songs;",
    [STMT_SQL_BEGIN] =
        "BEGIN IMMEDIATE;",
    [STMT_SQL_COMMIT] =
        "COMMIT;",
    [STMT_SQL_SOURCE_COUNT] =
        ""
};

////////////////////////////////
//     ATTRIBUTE LOCKING      //
////////////////////////////////

void moose_stprv_lock_attributes(MooseStore * self)
{
    g_assert(self);

    g_mutex_lock(&self->attr_set_mtx);
}

////////////////////////////////

void moose_stprv_unlock_attributes(MooseStore * self)
{
    g_assert(self);

    g_mutex_unlock(&self->attr_set_mtx);
}

////////////////////////////////

/*
 * Fill information into the meta-table.
 * Before inserting the table is dropped. Yes, really.
 * But it's re-created immediately, and then inserted.
 *
 * This way we will always have only one single row there.
 *
 * Also note, that this cannot be done using prepared statements,
 * since they rely on already created tables. That's why we
 * have to insert the values manually.
 */
void moose_stprv_insert_meta_attributes(MooseStore * self)
{

    char * insert_meta_sql = NULL;
    int queue_version = -1;
    MooseStatus * status = moose_ref_status(self->client);

    if (status != NULL) {
        queue_version = moose_status_get_queue_version(status);
        insert_meta_sql = sqlite3_mprintf(
            SQL_CODE(META),
            moose_status_stats_get_db_update_time(status),
            queue_version,
            MOOSE_DB_SCHEMA_VERSION,
            moose_get_port(self->client),
            moose_get_host(self->client)
            );
    }
    moose_status_unref(status);

    if (insert_meta_sql != NULL) {
        if (sqlite3_exec(self->handle, insert_meta_sql, NULL, NULL, NULL) != SQLITE_OK)
            REPORT_SQL_ERROR(self, "Cannot INSERT META Atrributes.");

        sqlite3_free(insert_meta_sql);
    }
}

////////////////////////////////

bool moose_stprv_create_song_table(MooseStore * self)
{
    g_assert(self);

    if (self->settings->tokenizer == NULL) {
        self->settings->tokenizer = "porter";
    } else { /* validate tokenizer string */
        bool found = false;
        const char * allowed[] = {"simple", "porter", "unicode61", "icu", NULL};

        for (int i = 0; allowed[i]; ++i) {
            if (g_strcmp0(allowed[i], self->settings->tokenizer) == 0) {
                found = true;
                break;
            }
        }

        if (found == false) {
            moose_shelper_report_error_printf(
                self->client,
                "Tokenizer ,,%s'' is unknown. Defaulting to 'porter'.",
                self->settings->tokenizer
                );

            self->settings->tokenizer = "porter";
        }
    }

    char * sql_create = g_strdup_printf(
        SQL_CODE(CREATE),
        self->settings->tokenizer
        );

    if (sqlite3_exec(self->handle, sql_create, NULL, NULL, NULL) != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Cannot CREATE TABLE Structure. This is pretty deadly to moosecat's core, you know?\n");
        return false;
    }

    g_free(sql_create);
    return true;
}


////////////////////////////////

void moose_stprv_prepare_all_statements(MooseStore * self)
{
    g_assert(self);

    /*
     * This is a bit hacky fix to let the 'select ... from meta' stmts compile.
     * Just add an empy table, so the selects compile.
     * When they actually get executed, the table will be filled correctly.
     *
     * Even if it is; not much would happen. */
    if (sqlite3_exec(self->handle, SQL_CODE(META_DUMMY), NULL, NULL, NULL) != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Cannot create dummy meta table. Expect some warnings.");
    }

    self->sql_prep_stmts = moose_stprv_prepare_all_statements_listed(
        self,
        _sql_stmts,
        STMT_SQL_NEED_TO_PREPARE_COUNT + 1,
        STMT_SQL_SOURCE_COUNT
        );
}

////////////////////////////////

sqlite3_stmt * * moose_stprv_prepare_all_statements_listed(MooseStore * self, const char * * sql_stmts, int offset, int n_stmts)
{
    g_assert(self);
    sqlite3_stmt * * stmt_list = NULL;
    stmt_list = g_malloc0(sizeof(sqlite3_stmt *) * (n_stmts + 1));
    int prep_error = 0;

    for (int i = offset; i < n_stmts; i++) {
        const char * sql = sql_stmts[i];

        if (sql != NULL) {
            prep_error = sqlite3_prepare_v2(
                self->handle, sql,
                strlen(sql) + 1, &stmt_list[i], NULL
                );

            /* Uh-Oh. Typo in the SQL statements perhaps? */
            if (prep_error != SQLITE_OK)
                REPORT_SQL_ERROR(self, "WARNING: cannot prepare statement");

            /* Left for Debugging */
            /* g_print("Faulty statement: %s\n", sql); */
        }
    }

    return stmt_list;
}

////////////////////////////////

/*
 * Open a sqlite3 handle to the memory.
 *
 * Returns: true on success.
 */
bool moose_strprv_open_memdb(MooseStore * self)
{
    g_assert(self);
    const char * db_name = (self->settings->use_memory_db) ? ":memory:" : MOOSE_STORE_TMP_DB_PATH;

    if (sqlite3_open(db_name, &self->handle) != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "ERROR: cannot open :memory: database. Dude, that's weird...");
        self->handle = NULL;
        return false;
    }

    return moose_stprv_create_song_table(self);
}

////////////////////////////////

void moose_stprv_finalize_statements(MooseStore * self, sqlite3_stmt * * stmts, int offset, int n_stmts)
{
    g_assert(self);
    g_assert(stmts);

    for (int i = offset; i < n_stmts; i++) {
        if (stmts[i] != NULL) {
            if (sqlite3_finalize(stmts[i]) != SQLITE_OK) {
                REPORT_SQL_ERROR(self, "WARNING: Cannot finalize statement");
            }

            stmts[i] = NULL;
        }
    }

    g_free(stmts);
}

////////////////////////////////

void moose_stprv_close_handle(MooseStore * self, bool free_statements)
{
    if (free_statements) {
        moose_stprv_finalize_statements(self, self->sql_prep_stmts, STMT_SQL_NEED_TO_PREPARE_COUNT + 1, STMT_SQL_SOURCE_COUNT);
        self->sql_prep_stmts = NULL;
    }

    if (sqlite3_close(self->handle) != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Warning: Unable to close db connection");
        self->handle = NULL;
    }
}

////////////////////////////////

void moose_stprv_begin(MooseStore * self)
{
    if (sqlite3_step(SQL_STMT(self, BEGIN)) != SQLITE_DONE)
        REPORT_SQL_ERROR(self, "WARNING: Unable to execute BEGIN");
    sqlite3_reset(SQL_STMT(self, BEGIN));
}

////////////////////////////////

void moose_stprv_commit(MooseStore * self)
{
    if (sqlite3_step(SQL_STMT(self, COMMIT)) != SQLITE_DONE)
        REPORT_SQL_ERROR(self, "WARNING: Unable to execute COMMIT");
    sqlite3_reset(SQL_STMT(self, COMMIT));
}

////////////////////////////////

void moose_stprv_delete_songs_table(MooseStore * self)
{
    if (sqlite3_step(SQL_STMT(self, DELETE_ALL)) != SQLITE_DONE)
        REPORT_SQL_ERROR(self, "WARNING: Cannot delete table contentes of 'songs'");
    sqlite3_reset(SQL_STMT(self, DELETE_ALL));
}

////////////////////////////////

/*
 * Insert a single song into the 'songs' table.
 * This inserts all attributes of the song, even if they are not set,
 * or not used in moosecat itself
 *
 * You should call moose_stprv_begin and moose_stprv_commit before/after
 * inserting, especially if inserting many songs.
 *
 * A prepared statement is used for simplicity & speed reasons.
 */
bool moose_stprv_insert_song(MooseStore * db, MooseSong * song)
{
    int error_id = SQLITE_OK;
    int pos_idx = 1;
    bool rc = true;

    /* bind basic attributes */
    // TODO TODO TODO: Start/End is empty.
    bind_txt(db, INSERT, pos_idx, moose_song_get_uri(song), error_id);
    bind_int(db, INSERT, pos_idx, 0, error_id);
    bind_int(db, INSERT, pos_idx, 1, error_id);
    bind_int(db, INSERT, pos_idx, moose_song_get_duration(song), error_id);
    bind_int(db, INSERT, pos_idx, moose_song_get_last_modified(song), error_id);

    /* bind tags */
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_ARTIST, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_ALBUM, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_TITLE, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_ALBUM_ARTIST, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_TRACK, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_NAME, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_GENRE, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_DATE, error_id);
    if (error_id != SQLITE_OK) {
        REPORT_SQL_ERROR(db, "WARNING: Error while binding");
    }

    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_COMPOSER, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_PERFORMER, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_COMMENT, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_DISC, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_MUSICBRAINZ_ARTISTID, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_MUSICBRAINZ_ALBUMID, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_MUSICBRAINZ_ALBUMARTISTID, error_id);
    bind_tag(db, INSERT, pos_idx, song, MPD_TAG_MUSICBRAINZ_TRACKID, error_id);

    /* Constant Value. See Create statement. */
    bind_int(db, INSERT, pos_idx,  0, error_id);

    bind_int(db, INSERT, pos_idx, moose_stprv_path_get_depth(moose_song_get_uri(song)), error_id);

    /* this is one error check for all the blocks above */
    if (error_id != SQLITE_OK)
        REPORT_SQL_ERROR(db, "WARNING: Error while binding");

    /* evaluate prepared statement, only check for errors */
    while ((error_id = sqlite3_step(SQL_STMT(db, INSERT))) == SQLITE_BUSY)
        /* Nothing */;

    /* Make sure bindings are ready for the next insert */
    CLEAR_BINDS_BY_NAME(db, INSERT);
    sqlite3_reset(SQL_STMT(db, INSERT));

    if ((rc = (error_id != SQLITE_DONE)) == true)
        REPORT_SQL_ERROR(db, "WARNING: cannot insert into :memory: - that's pretty serious.");

    return rc;
}


////////////////////////////////

static gint moose_stprv_select_impl_sort_func_by_pos(gconstpointer a, gconstpointer b)
{
    if (a && b) {
        int pos_a = moose_song_get_pos(*((MooseSong * *)a));
        int pos_b = moose_song_get_pos(*((MooseSong * *)b));

        if (pos_a == pos_b) return +0;

        if (pos_a <  pos_b) return -1;

        /* pos_a > pos_b */
        return +1;
    } else return +1; /* to sort NULL at the end */
}

static gint moose_stprv_select_impl_sort_func_by_stack_ptr(gconstpointer a, gconstpointer b)
{
    if (a && b) {
        MooseSong * sa = (*((MooseSong * *)a));
        MooseSong * sb = (*((MooseSong * *)b));

        if (sa == sb) return +0;

        if (sa <  sb) return -1;

        return +1;
    } else return +1; /* to sort NULL at the end */
}

////////////////////////////////

/* Quadratic binary search algorithm adapted from:
 *
 *   http://research.ijcaonline.org/volume65/number14/pxc3886165.pdf
 *
 * This is more of a fun excercise, than a real speedup.
 */
static MooseSong * moose_stprv_find_idx(MoosePlaylist * stack, MooseSong * key) {
    int l = 0;
    int r = moose_playlist_length(stack) - 1;
    int qr1 = 0, qr3 = 0, mid = 0;

    while (l <= r) {
        mid = (l + r) / 2;
        qr1 = l + (r - l) / 4;
        qr3 = l + (r - l) * 3 / 4;

        MooseSong * at_mid = moose_playlist_at(stack, mid);
        MooseSong * at_qr1 = moose_playlist_at(stack, qr1);
        MooseSong * at_qr3 = moose_playlist_at(stack, qr3);

        if (at_mid == key || key == at_qr1 || key == at_qr3) {
            int result_idx = 0;
            if (at_mid == key) {
                result_idx = mid;
            } else if (at_qr1 == key) {
                result_idx = qr1;
            } else {
                result_idx = qr3;
            }
            return moose_playlist_at(stack, result_idx);
        } else if (key < at_mid && key < at_qr1) {
            r = qr1 - 1;
        } else if (key < at_mid && key > at_qr1) {
            l = qr1 + 1;
            r = mid - 1;
        } else if (key > at_mid && key > at_qr3) {
            l = qr3 + 1;
        } else if (key > at_mid && key < at_qr3) {
            l = mid + 1;
            r = qr3 - 1;
        }
    }
    return NULL;
}

////////////////////////////////

static MoosePlaylist * moose_stprv_build_queue_content(MooseStore * self, MoosePlaylist * to_filter)
{
    g_assert(self);

    int error_id = SQLITE_OK;
    sqlite3_stmt * select_stmt = SQL_STMT(self, SELECT_ALL_QUEUE);
    MoosePlaylist * queue_songs = moose_playlist_new();

    while ((error_id = sqlite3_step(select_stmt)) == SQLITE_ROW) {
        int stack_idx = sqlite3_column_int(select_stmt, 0);
        if (stack_idx <= 0) {
            continue;
        }

        MooseSong * queue_song = moose_stprv_find_idx(
            to_filter, moose_playlist_at(self->stack, stack_idx - 1)
            );
        if (queue_song != NULL) {
            moose_playlist_append(queue_songs, queue_song);
        }
    }

    if (sqlite3_errcode(self->handle) != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "Error while building queue contents");
    }

    if (sqlite3_reset(select_stmt) != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Error while resetting select queue statement");
    }

    return queue_songs;
}

/*
 * Search stuff in the 'songs' table using a SELECT clause (also using MATCH).
 * Instead of selecting the actual songs, only the docid is selected, and used as
 * an index to the song-stack, which is quite a bit faster/memory efficient for
 * large returns.
 *
 * It uses the buffer passed to the function to store the pointer to mpd songs it found.
 * Do not free these, the memory of these are manged internally!
 *
 * This function will select at max. buffer_len songs.
 *
 * Returns: number of actually found songs, or -1 on error.
 */
int moose_stprv_select_to_stack(MooseStore * self, const char * match_clause, bool queue_only, MoosePlaylist * stack, int limit_len)
{
    int error_id = SQLITE_OK, pos_id = 1;
    limit_len = (limit_len < 0) ? INT_MAX : limit_len;

    const char * warning = NULL;
    int warning_pos = -1;
    gchar * match_clause_dup = moose_store_qp_parse(match_clause, &warning, &warning_pos);

    if (warning != NULL) {
        moose_shelper_report_error_printf(
            self->client,
            "database: Query Parse Error: %d:%s", warning_pos, warning
            );
    }

    /* emits a warning if NULL is passed *sigh* */
    if (match_clause_dup != NULL)
        match_clause_dup = g_strstrip(match_clause_dup);

    sqlite3_stmt * select_stmt = NULL;

    /* If the query is empty anyway, we just select everything */
    if (match_clause_dup == NULL || *match_clause_dup == 0) {
        select_stmt = SQL_STMT(self, SELECT_MATCHED_ALL);
    } else {
        select_stmt = SQL_STMT(self, SELECT_MATCHED);
        bind_txt(self, SELECT_MATCHED, pos_id, match_clause_dup, error_id);
        bind_int(self, SELECT_MATCHED, pos_id, limit_len, error_id);
    }

    if (error_id != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "WARNING: Error while binding");
        return -1;
    }

    while ((error_id = sqlite3_step(select_stmt)) == SQLITE_ROW) {
        int song_idx = sqlite3_column_int(select_stmt, 0);
        MooseSong * selected = moose_playlist_at(self->stack, song_idx - 1);

        /* Even if we set queue_only == true, all rows are searched using MATCH.
         * This is because of MATCH does not like additianal constraints.
         * Therefore we filter here the queue songs ourselves.
         *
         * We can do that a lot faster anyways;
         * */
        if (queue_only == false || ((int)moose_song_get_pos(selected) > -1)) {
            moose_playlist_append(stack, selected);
        }
    }

    if (error_id != SQLITE_DONE && error_id != SQLITE_ROW)
        REPORT_SQL_ERROR(self, "WARNING: Cannot SELECT");

    CLEAR_BINDS(select_stmt);
    sqlite3_reset(select_stmt);

    g_free(match_clause_dup);

    /* sort by position in queue if queue_only is passed. */
    if (queue_only) {
        moose_playlist_sort(stack, moose_stprv_select_impl_sort_func_by_stack_ptr);
        MoosePlaylist * queue = moose_stprv_build_queue_content(self, stack);
        moose_playlist_clear(stack);
        for (unsigned i = 0; i < moose_playlist_length(queue); ++i) {
            moose_playlist_append(stack, moose_playlist_at(queue, i));
        }
        stack = queue;
        moose_playlist_sort(stack, moose_stprv_select_impl_sort_func_by_pos);
    }

    return moose_playlist_length(stack);
}

////////////////////////////////

/*
 * Code taken from:
 * http://www.sqlite.org/backup.html
 *
 ** This function is used to load the contents of a database file on disk
 ** into the "main" database of open database connection pInMemory, or
 ** to save the current contents of the database opened by pInMemory into
 ** a database file on disk. pInMemory is probably an in-memory database,
 ** but this function will also work fine if it is not.
 **
 ** Parameter zFilename points to a nul-terminated string containing the
 ** name of the database file on disk to load from or save to. If parameter
 ** isSave is non-zero, then the contents of the file zFilename are
 ** overwritten with the contents of the database opened by pInMemory. If
 ** parameter isSave is zero, then the contents of the database opened by
 ** pInMemory are replaced by data loaded from the file zFilename.
 **
 ** If the operation is successful, SQLITE_OK is returned. Otherwise, if
 ** an error occurs, an SQLite error code is returned.
 */
void moose_stprv_load_or_save(MooseStore * self, bool is_save, const char * db_path)
{
    sqlite3 * pFile;           /* Database connection opened on zFilename */
    sqlite3_backup * pBackup;  /* Backup object used to copy data */
    sqlite3 * pTo;             /* Database to copy to (pFile or pInMemory) */
    sqlite3 * pFrom;           /* Database to copy from (pFile or pInMemory) */

    /* Open the database file identified by zFilename. Exit early if this fails
    ** for any reason. */
    if (sqlite3_open(db_path, &pFile) == SQLITE_OK) {
        /* If this is a 'load' operation (isSave==0), then data is copied
        ** from the database file just opened to database pInMemory.
        ** Otherwise, if this is a 'save' operation (isSave==1), then data
        ** is copied from pInMemory to pFile.  Set the variables pFrom and
        ** pTo accordingly. */
        pFrom = (is_save ? self->handle : pFile);
        pTo   = (is_save ? pFile : self->handle);
        /* Set up the backup procedure to copy from the "main" database of
        ** connection pFile to the main database of connection pInMemory.
        ** If something goes wrong, pBackup will be set to NULL and an error
        ** code and  message left in connection pTo.
        **
        ** If the backup object is successfully created, call backup_step()
        ** to copy data from pFile to pInMemory. Then call backup_finish()
        ** to release resources associated with the pBackup object.  If an
        ** error occurred, then  an error code and message will be left in
        ** connection pTo. If no error occurred, then the error code belonging
        ** to pTo is set to SQLITE_OK.
        */
        pBackup = sqlite3_backup_init(pTo, "main", pFrom, "main");

        if (pBackup) {
            sqlite3_backup_step(pBackup, -1);
            sqlite3_backup_finish(pBackup);
        }

        if (sqlite3_errcode(pTo) != SQLITE_OK)
            REPORT_SQL_ERROR(self, "Something went wrong during backup");
    } else {
        REPORT_SQL_ERROR(self, "Cannot open disk-db");
    }

    /* Close the database connection opened on database file zFilename
    ** and return the result of this function. */
    if (sqlite3_close(pFile) != SQLITE_OK)
        REPORT_SQL_ERROR(self, "Cannot close db in load-or-save");
}

////////////////////////////////

#define feed_tag(tag_enum, sql_col_pos, stmt, song) {              \
        char * value = (char *)sqlite3_column_text(stmt, sql_col_pos);  \
        if (value) {                                                   \
            moose_song_set_tag(song, tag_enum, value);                 \
        }                                                              \
}                                                               \

void moose_stprv_deserialize_songs(MooseStore * self)
{
    /* just assume we're not failing */
    int error_id = SQLITE_OK;

    /* the stmt we will use here, shorter to write.. */
    sqlite3_stmt * stmt = SQL_STMT(self, SELECT_ALL);

    /* progress */
    int progress_counter = 0;
    GTimer * timer = g_timer_new();

    /* loop over all rows in the songs table */
    while ((error_id = sqlite3_step(stmt)) == SQLITE_ROW) {
        /* Following fields are understood by libmpdclient:
         * file (= uri)
         * Time (= duration)
         * Range (= start-end)
         * Last-Modified
         * Pos (= here, always 0)
         * Id (= here, always 0)
         *
         * It's also case-sensitive.
         */
        MooseSong * song = moose_song_new();
        moose_song_set_uri(
            song, (char *)sqlite3_column_text(stmt, SQL_COL_URI)
            );

        /* Since SQLite is completely typeless we can just retrieve the column as string */
        moose_song_set_duration(
            song, sqlite3_column_int(stmt, SQL_COL_DURATION)
            );

        /* We parse the date ourself, since we can use a nice static buffer here */
        moose_song_set_last_modified(
            song, sqlite3_column_int(stmt, SQL_COL_LAST_MODIFIED)
            );

        /* Now feed the tags */
        feed_tag(MOOSE_TAG_ARTIST, SQL_COL_ARTIST, stmt, song);
        feed_tag(MOOSE_TAG_ALBUM, SQL_COL_ALBUM, stmt, song);
        feed_tag(MOOSE_TAG_TITLE, SQL_COL_TITLE, stmt, song);
        feed_tag(MOOSE_TAG_ALBUM_ARTIST, SQL_COL_ALBUM_ARTIST, stmt, song);
        feed_tag(MOOSE_TAG_TRACK, SQL_COL_TRACK, stmt, song);
        feed_tag(MOOSE_TAG_NAME, SQL_COL_NAME, stmt, song);
        feed_tag(MOOSE_TAG_GENRE, SQL_COL_GENRE, stmt, song);
        feed_tag(MOOSE_TAG_DATE, SQL_COL_DATE, stmt, song);
        feed_tag(MOOSE_TAG_COMPOSER, SQL_COL_COMPOSER, stmt, song);
        feed_tag(MOOSE_TAG_PERFORMER, SQL_COL_PERFORMER, stmt, song);
        feed_tag(MOOSE_TAG_COMMENT, SQL_COL_COMMENT, stmt, song);
        feed_tag(MOOSE_TAG_DISC, SQL_COL_DISC, stmt, song);
        feed_tag(MOOSE_TAG_MUSICBRAINZ_ARTISTID, SQL_COL_MUSICBRAINZ_ARTIST_ID, stmt, song);
        feed_tag(MOOSE_TAG_MUSICBRAINZ_ALBUMID, SQL_COL_MUSICBRAINZ_ALBUM_ID, stmt, song);
        feed_tag(MOOSE_TAG_MUSICBRAINZ_ALBUMARTISTID, SQL_COL_MUSICBRAINZ_ALBUMARTIST_ID, stmt, song);
        feed_tag(MOOSE_TAG_MUSICBRAINZ_TRACKID, SQL_COL_MUSICBRAINZ_TRACK_ID, stmt, song);

        /* Remember it */
        moose_playlist_append(self->stack, song);

        ++progress_counter;
    }

    /* Tell the user we're so happy, we shat a database */
    moose_shelper_report_progress(
        self->client, true,
        "database: deserialized %d songs from local db. (took %2.3f)",
        progress_counter, g_timer_elapsed(timer, NULL)
        );
    moose_shelper_report_operation_finished(self->client, MOOSE_OP_DB_UPDATED);
    moose_shelper_report_operation_finished(self->client, MOOSE_OP_QUEUE_UPDATED);
    g_timer_destroy(timer);

    if (error_id != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "ERROR: cannot load songs from database");
    }

    sqlite3_reset(stmt);
}

/////////////////// META TABLE STUFF ////////////////////

#define select_meta_attribute(self, meta_enum, column_func, out_var, copy_func, cast_type)  \
    {                                                                                       \
        int error_id = SQLITE_OK;                                                           \
        if ((error_id = sqlite3_step(SQL_STMT(self, meta_enum))) == SQLITE_ROW)            \
            out_var = (cast_type)column_func(SQL_STMT(self, meta_enum), 0);                 \
        if (error_id != SQLITE_DONE && error_id != SQLITE_ROW)                              \
            REPORT_SQL_ERROR(self, "WARNING: Cannot SELECT META");                         \
        out_var = (cast_type)copy_func((cast_type)out_var);                                 \
        sqlite3_reset(SQL_STMT(self, meta_enum));                                         \
    }                                                                                       \


////////////////////////////////

int moose_stprv_get_song_count(MooseStore * self)
{
    int song_count = -1;
    select_meta_attribute(self, COUNT, sqlite3_column_int, song_count, ABS, int);
    return song_count;
}

////////////////////////////////

int moose_stprv_get_db_version(MooseStore * self)
{
    int db_version = 0;
    select_meta_attribute(self, SELECT_META_DB_VERSION, sqlite3_column_int, db_version, ABS, int);
    return db_version;
}

////////////////////////////////

int moose_stprv_get_pl_version(MooseStore * self)
{
    int pl_version = 0;
    select_meta_attribute(self, SELECT_META_PL_VERSION, sqlite3_column_int, pl_version, ABS, int);
    return pl_version;
}

////////////////////////////////

int moose_stprv_get_sc_version(MooseStore * self)
{
    int sc_version = 0;
    select_meta_attribute(self, SELECT_META_SC_VERSION, sqlite3_column_int, sc_version, ABS, int);
    return sc_version;
}

////////////////////////////////

int moose_stprv_get_mpd_port(MooseStore * self)
{
    int mpd_port = 0;
    select_meta_attribute(self, SELECT_META_MPD_PORT, sqlite3_column_int, mpd_port, ABS, int);
    return mpd_port;
}

////////////////////////////////

char * moose_stprv_get_mpd_host(MooseStore * self)
{
    char * mpd_host = NULL;
    select_meta_attribute(self, SELECT_META_MPD_HOST, sqlite3_column_text, mpd_host, g_strdup, char *);
    return mpd_host;
}

/////////////////// QUEUE STUFF ////////////////////

int moose_stprv_queue_clip(MooseStore * self, int since_pos)
{
    g_assert(self);

    int pos_idx = 1;
    int error_id = SQLITE_OK;
    sqlite3_stmt * clear_stmt = SQL_STMT(self, QUEUE_CLEAR);

    bind_int(self, QUEUE_CLEAR, pos_idx, MAX(-1, since_pos - 1), error_id);

    if (error_id != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Cannot bind stuff to clip statement");
        return -1;
    }

    if (sqlite3_step(clear_stmt) != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "Cannot clip Queue contents");
    }

    /* Make sure bindings are ready for the next insert */
    CLEAR_BINDS(clear_stmt);
    sqlite3_reset(clear_stmt);
    return sqlite3_changes(self->handle);
}

///////////////////

void moose_stprv_queue_update_stack_posid(MooseStore * self)
{
    g_assert(self);
    int error_id = SQLITE_OK;
    sqlite3_stmt * select_stmt = SQL_STMT(self, SELECT_ALL_QUEUE);

    /* Set the ID by parsing the ID */
    GHashTable * already_seen_ids = g_hash_table_new(NULL, NULL);

    while ((error_id = sqlite3_step(select_stmt)) == SQLITE_ROW) {
        int row_idx  = sqlite3_column_int(select_stmt, 0);

        if (row_idx > 0) {
            MooseSong * song = moose_playlist_at(self->stack, row_idx - 1);

            if (song != NULL) {
                unsigned queue_pos = sqlite3_column_int(select_stmt, 1);
                g_hash_table_insert(
                    already_seen_ids,
                    song,
                    GINT_TO_POINTER(TRUE)
                    );

                /* Luckily we have a setter here, otherwise I would feel a bit strange. */
                moose_song_set_pos(song, queue_pos);
                moose_song_set_id(song, sqlite3_column_int(select_stmt, 2));
            }
        }
    }

    sqlite3_reset(select_stmt);

    /* Reset all songs of the database to -1/-1 that were not in the queue */
    for (unsigned i = 0; i < moose_playlist_length(self->stack); ++i) {
        MooseSong * song = moose_playlist_at(self->stack, i);
        if (song != NULL) {
            if (!g_hash_table_lookup(already_seen_ids, song)) {
                /* Warning: -1 is casted to unsigned int and late back */
                moose_song_set_pos(song, -1);
                moose_song_set_id(song, -1);
            }
        }
    }
    g_hash_table_destroy(already_seen_ids);
}

///////////////////

void moose_stprv_queue_insert_posid(MooseStore * self, int pos, int idx, const char * file)
{
    int pos_idx = 1, error_id = SQLITE_OK;
    bind_txt(self, QUEUE_INSERT_ROW, pos_idx, file, error_id);
    bind_int(self, QUEUE_INSERT_ROW, pos_idx, pos, error_id);
    bind_int(self, QUEUE_INSERT_ROW, pos_idx, idx, error_id);
    pos_idx = 1;

    if (error_id != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Cannot bind stuff to INSERT statement");
        return;
    }

    if (sqlite3_step(SQL_STMT(self, QUEUE_INSERT_ROW)) != SQLITE_DONE)
        REPORT_SQL_ERROR(self, "Unable to INSERT song into queue.");

    CLEAR_BINDS_BY_NAME(self, QUEUE_INSERT_ROW);
    sqlite3_reset(SQL_STMT(self, QUEUE_INSERT_ROW));
}

///////////////////

int moose_stprv_path_get_depth(const char * dir_path)
{
    int dir_depth = 0;
    char * cursor = (char *)dir_path;

    if (dir_path == NULL)
        return -1;

    for (;;) {
        gunichar curr = g_utf8_get_char_validated(cursor, -1);
        cursor = g_utf8_next_char(cursor);

        if (*cursor != (char)0) {
            if (curr == (gunichar)'/')
                ++dir_depth;
        } else {
            break;
        }
    }

    return dir_depth;
}
