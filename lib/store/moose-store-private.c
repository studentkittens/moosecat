#include "../moose-config.h"
#include "../mpd/moose-song-private.h"
#include "moose-store-query-parser.h"

/**
 * @brief Open a :memory: db
 *
 * Returns true on success.
 */
bool moose_strprv_open_memdb(MooseStorePrivate * self);

/**
 * @brief Insert a single song to the db.
 *
 * You should call moose_stprv_begin/commit before and after.
 */
bool moose_stprv_insert_song(MooseStorePrivate * db, MooseSong * song);

/**
 * @brief Update the db's meta table.
 */
void moose_stprv_insert_meta_attributes(MooseStorePrivate * self);

/**
 * @brief Compile all sqlite3_stmt objects.
 *
 * Needed before any calls after open_memdb
 */
void moose_stprv_prepare_all_statements(MooseStorePrivate * self);

/**
 * @brief Initialize the statement buffer of MooseStorePrivate
 */
sqlite3_stmt ** moose_stprv_prepare_all_statements_listed(MooseStorePrivate * self, const char ** sql_stmts, int offset, int n_stmts);

/**
 * @brief Deinitialize the statement buffer of MooseStorePrivate
 */
void moose_stprv_finalize_statements(MooseStorePrivate * self, sqlite3_stmt ** stmts, int offset, int n_stmts);

/**
 * @brief Creates the (empty) song table.
 */
bool moose_stprv_create_song_table(MooseStorePrivate * self);

/**
 * @brief Execute COMMIT;
 */
void moose_stprv_commit(MooseStorePrivate * self);

/**
 * @brief Execute BEGIN IMMEDIATE;
 */
void moose_stprv_begin(MooseStorePrivate * self);

/**
 * @brief DELETE FROM SONGS;
 */
void moose_stprv_delete_songs_table(MooseStorePrivate * self);

/**
 * @brief load or save a memory db to/from disk.
 *
 * @param is_save true if db should be saved.
 *
 * @returns 0 on success, an SQLITE errcode otherwise.
 */
void moose_stprv_load_or_save(MooseStorePrivate * self, bool is_save, const char * db_path);

/**
 * @brief Load songs from previously saved database into stack.
 */
void moose_stprv_deserialize_songs(MooseStorePrivate * self);

/**
 * @brief Same as moose_stprv_select_to_buf, but use stack instead of buf.
 *
 * @param self store to operate on
 * @param match_clause FTS match clause
 * @param queue_only limit search to queue?
 * @param stack an MoosePlaylist, ideally preallocated to the expected size.
 * @param limit_len Max songs to select, or -1 for no limit
 *
 * @return the number of selected songs.
 */
int moose_stprv_select_to_stack(
    MooseStorePrivate * self,
    const char * match_clause,
    bool queue_only,
    MoosePlaylist * stack,
    int limit_len);

/**
 * @brief get server db version
 */
int moose_stprv_get_db_version(MooseStorePrivate * self);

/**
 * @brief get playlist version
 */
int moose_stprv_get_pl_version(MooseStorePrivate * self);

/**
 * @brief get schema version (only to check if db is old)
 */
int moose_stprv_get_sc_version(MooseStorePrivate * self);

/**
 * @brief get mpd port to the db where this belongs to.
 */
int moose_stprv_get_mpd_port(MooseStorePrivate * self);

/**
 * @brief select count(*) from songs;
 */
int moose_stprv_get_song_count(MooseStorePrivate * self);

/**
 * @brief get mpd host in the meta table.
 *
 * Free return value if no longer used.
 */
char * moose_stprv_get_mpd_host(MooseStorePrivate * self);

/**
 * @brief Clear pos/id in the songs table (to -1)
 */
int moose_stprv_queue_clip(MooseStorePrivate * self, int since_pos);

/**
 * @brief Update a song, identified by file's pos/id to pos/idx
 */
void moose_stprv_queue_insert_posid(MooseStorePrivate * self, int pos, int idx, const char * file);

/**
 * @brief Update the song's stack songs pos/id according to the songs table.
 */
void moose_stprv_queue_update_stack_posid(MooseStorePrivate * self);

/**
 * @brief Close the sqlite3 handle
 */
void moose_stprv_close_handle(MooseStorePrivate * self, bool free_statements);

/**
 * @brief Lock self->stack and self->spl.stack for change
 */
void moose_stprv_lock_attributes(MooseStorePrivate * self);

/**
 * @brief Unlock previous lock by moose_stprv_lock_attributes()
 */
void moose_stprv_unlock_attributes(MooseStorePrivate * self);

/* @brief Count the 'depth' of a path.
 * @param dir_path the path to count
 *
 * Examples:
 *
 *  Path:     Depth:
 *  /         0
 *  a/        0
 *  a         0
 *  a/b       1
 *  a/b/      1
 *  a/b/c     2
 *
 *  @return the depth.
 */
int moose_stprv_path_get_depth(const char * dir_path);

/**
 * @brief Insert a new directory to the database
 *
 * @param self Store to insert it inot
 * @param path what path you want to insert
 */
void moose_stprv_dir_insert(MooseStorePrivate * self, const char * path);

/**
 * @brief Delete all contents from the dir table.
 *
 * @param self the store to operate on
 */
void moose_stprv_dir_delete(MooseStorePrivate * self);

/**
 * @brief Query the database
 *
 * @param self the store to search.
 * @param stack stack to write the results too
 * @param directory Search query or NULL
 * @param depth depth or -1
 *
 * @return number of matches
 */
int moose_stprv_dir_select_to_stack(MooseStorePrivate * self, MoosePlaylist * stack, const char * directory, int depth);

/**
 * @brief Compile all SQL statements
 *
 * @param self Store to operate on
 */
void moose_stprv_dir_prepare_statemtents(MooseStorePrivate * self);

/**
 * @brief Free all prepared statements
 *
 * @param self Store to operate on
 */
void moose_stprv_dir_finalize_statements(MooseStorePrivate * self);

/**
 * @brief List all Songs from the Database and write them to the Database.
 *
 * The function prototype is compatible to MooseJobManager.
 *
 * @param store The Store passed.
 * @param cancel A flag indicating the desired cancellation of a Job.
 */
void moose_stprv_oper_listallinfo(MooseStorePrivate * store, volatile gboolean * cancel);

/**
 * @brief List all changes of the Queue since the last time.
 *
 * The function prototype is compatible to MooseJobManager.
 *
 * @param store The Store passed.
 * @param cancel A flag indicating the desired cancellation of a Job.
 */
void moose_stprv_oper_plchanges(MooseStorePrivate * store, volatile gboolean * cancel);

/**
 * @brief Init self->spl.*
 *
 * @param self the store to initialzie stored playlist support
 */
void moose_stprv_spl_init(MooseStorePrivate * self);

/**
 * @brief Destroy all ressources allocated by moose_stprv_spl_init()
 *
 * @param self the store you called moose_stprv_spl_init on
 */
void moose_stprv_spl_destroy(MooseStorePrivate * self);

/**
 * @brief Update stored playlists from MPD
 *
 * - This will only fetch metadata about the playlist.
 *   use moose_stprv_spl_load to actually load them
 * - This will drop orphanded playlists.
 *
 * @param self the store to update.
 */
void moose_stprv_spl_update(MooseStorePrivate * self);

/**
 * @brief Load a certain playlist.
 *
 * This actually fetches the
 *
 * @param store the store to load them to.
 * @param playlist a mpd_playlist struct
 */
bool moose_stprv_spl_load(MooseStorePrivate * store, struct mpd_playlist * playlist);

/**
 * @brief Load a playlist by it's name.
 *
 * @param store the store to load them to
 * @param playlist_name  a playlist name
 */
bool moose_stprv_spl_load_by_playlist_name(MooseStorePrivate * store, const char * playlist_name);

/**
 * @brief Search in a Playlist
 *
 * @param store the store that knows about the Playlist
 * @param out_stack stack to write the results to
 * @param playlist_name name of the playlist
 * @param match_clause a match clause as understood by FTS
 *
 * @return Number of selected songs.
 */
int moose_stprv_spl_select_playlist(MooseStorePrivate * store, MoosePlaylist * out_stack, const char * playlist_name, const char * match_clause);

/**
 * @brief Select all loaded mpd_playlists object to a stack.
 *
 * @param store the store that knows about the playlists
 * @param stack the stack to write the mpd_playlist objects to
 *
 * @return Number of laoded playlists
 */
int moose_stprv_spl_get_loaded_playlists(MooseStorePrivate * store, MoosePlaylist * stack);

/**
 * @brief Return a list of a all known mpd_playlists structs
 *
 * @param store the store which knows about them
 * @param stack the stack where to write them to
 *
 * @return number of appended playlisyts
 */
int moose_stprv_spl_get_known_playlists(MooseStorePrivate * store, GPtrArray * stack);

/* strlen() */
#include <string.h>

/* strtol() */
#include <stdlib.h>

#define BIND_INT(db, type, pos_idx, value, error_id) \
    error_id |= sqlite3_bind_int(SQL_STMT(db, type), (pos_idx)++, value);

#define BIND_TXT(db, type, pos_idx, value, error_id) \
    error_id |= sqlite3_bind_text(SQL_STMT(db, type), (pos_idx)++, value, -1, NULL);

#define BIND_TAG(db, type, pos_idx, song, tag_id, error_id) \
    BIND_TXT(db, type, pos_idx, moose_song_get_tag(song, tag_id), error_id)

/*
 * DB Layout version.
 * Older tables will not be loaded.
 * */
#define MOOSE_DB_SCHEMA_VERSION 2

#define MOOSE_STORE_TMP_DB_PATH "/tmp/.moosecat.tmp.db"

#define REPORT_SQL_ERROR(store, message)                              \
    moose_critical(                                                   \
        "[%s:%d] %s -> %s (#%d)",                                     \
        __FILE__, __LINE__, message,                                  \
        sqlite3_errmsg(store->handle), sqlite3_errcode(store->handle) \
        );                                                                \
 
#define CLEAR_BINDS(stmt)         \
    sqlite3_reset(stmt);          \
    sqlite3_clear_bindings(stmt); \
 
#define CLEAR_BINDS_BY_NAME(store, type) \
    CLEAR_BINDS(SQL_STMT(store, type))

#define SQL_CODE(NAME) \
    _sql_stmts[STMT_SQL_ ## NAME]

#define SQL_STMT(STORE, NAME) \
    STORE->sql_prep_stmts[STMT_SQL_ ## NAME]

/**
 * Enumeration of all operations understood by moose_store_job_execute_callback()
 *
 * A name and priority can be given to the enum in MooseJobNames and MooseJobPrios
 * in db.c (somewhere at the top)
 */
typedef enum {
    MOOSE_OPER_UNDEFINED       = 0,       /* Actually, not used.                                 */
    MOOSE_OPER_DESERIALIZE     = 1 << 0,  /* Load all songs from the database                    */
    MOOSE_OPER_LISTALLINFO     = 1 << 1, /* Get the database from mpd                            */
    MOOSE_OPER_PLCHANGES       = 1 << 2, /* Get the Queue from mpd                               */
    MOOSE_OPER_DB_SEARCH       = 1 << 3, /* Search Data in the Queue or whole DB                 */
    MOOSE_OPER_DIR_SEARCH      = 1 << 4, /* List a directory of the music directory              */
    MOOSE_OPER_SPL_LOAD        = 1 << 5, /* Load a stored playlist                               */
    MOOSE_OPER_SPL_UPDATE      = 1 << 6, /* Update loaded playlists                              */
    MOOSE_OPER_SPL_LIST        = 1 << 7, /* List the names of all loaded playlists               */
    MOOSE_OPER_SPL_QUERY       = 1 << 8, /* Query a stored playlist                              */
    MOOSE_OPER_SPL_LIST_ALL    = 1 << 9, /* List the mpd_playlist objects of all known playlists */
    MOOSE_OPER_UPDATE_META     = 1 << 10, /* Update meta information about the table             */
    MOOSE_OPER_WRITE_DATABASE  = 1 << 11, /* Write Database to disk, making a backup             */
    MOOSE_OPER_FIND_SONG_BY_ID = 1 << 12, /* Find a song by it's SongID                          */
    MOOSE_OPER_ENUM_MAX        = 1 << 13, /* Highest Value in this Enum                          */
} MooseStoreOperation;

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
    STMT_SQL_DIR_INSERT,
    STMT_SQL_DIR_SEARCH_PATH,
    STMT_SQL_DIR_SEARCH_DEPTH,
    STMT_SQL_DIR_SEARCH_PATH_AND_DEPTH,
    STMT_SQL_DIR_DELETE_ALL,
    STMT_SQL_DIR_STMT_COUNT,
    STMT_SQL_SPL_SELECT_TABLES,
    /* === total number of defined sources === */
    STMT_SQL_SOURCE_COUNT
    /* ======================================= */
};

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
    [STMT_SQL_DIR_INSERT] =
    "INSERT INTO dirs VALUES(?, ?);",
    [STMT_SQL_DIR_DELETE_ALL] =
    "DELETE FROM dirs;",
    [STMT_SQL_DIR_SEARCH_PATH] =
    "SELECT -1, path FROM dirs WHERE path MATCH ? UNION "
    "SELECT rowid, uri FROM songs WHERE uri MATCH ? "
    "ORDER BY songs.rowid, songs.uri, dirs.path;",
    [STMT_SQL_DIR_SEARCH_DEPTH] =
    "SELECT -1, path FROM dirs WHERE depth = ? UNION "
    "SELECT rowid, uri FROM songs WHERE uri_depth = ? "
    "ORDER BY songs.rowid, songs.uri, dirs.path;",
    [STMT_SQL_DIR_SEARCH_PATH_AND_DEPTH] =
    "SELECT -1, path FROM dirs WHERE depth = ? AND path MATCH ? UNION "
    "SELECT rowid, uri FROM songs WHERE uri_depth = ? AND uri MATCH ? "
    "ORDER BY songs.rowid, songs.uri, dirs.path;",
    [STMT_SQL_SPL_SELECT_TABLES] =
    "SELECT name FROM sqlite_master WHERE type = 'table' AND name LIKE 'spl_%' ORDER BY name;",
    [STMT_SQL_SOURCE_COUNT] =
    ""
};

void moose_stprv_lock_attributes(MooseStorePrivate * self) {
    g_assert(self);

    g_mutex_lock(&self->attr_set_mtx);
}

void moose_stprv_unlock_attributes(MooseStorePrivate * self) {
    g_assert(self);

    g_mutex_unlock(&self->attr_set_mtx);
}

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
void moose_stprv_insert_meta_attributes(MooseStorePrivate * self) {

    char * insert_meta_sql = NULL;
    int queue_version = -1;
    MooseStatus * status = moose_client_ref_status(self->client);

    if (status != NULL) {
        queue_version = moose_status_get_queue_version(status);
        insert_meta_sql = sqlite3_mprintf(
                              SQL_CODE(META),
                              moose_status_stats_get_db_update_time(status),
                              queue_version,
                              MOOSE_DB_SCHEMA_VERSION,
                              moose_client_get_port(self->client),
                              moose_client_get_host(self->client)
                          );
    }
    moose_status_unref(status);

    if (insert_meta_sql != NULL) {
        if (sqlite3_exec(self->handle, insert_meta_sql, NULL, NULL, NULL) != SQLITE_OK) {
            REPORT_SQL_ERROR(self, "Cannot INSERT META Atrributes.");
        }

        sqlite3_free(insert_meta_sql);
    }
}

bool moose_stprv_create_song_table(MooseStorePrivate * self) {
    g_assert(self);

    if (self->settings.tokenizer == NULL) {
        strcpy(self->settings.tokenizer, "porter");
    } else { /* validate tokenizer string */
        bool found = false;
        const char * allowed[] = {"simple", "porter", "unicode61", "icu", NULL};

        for (int i = 0; allowed[i]; ++i) {
            if (g_strcmp0(allowed[i], self->settings.tokenizer) == 0) {
                found = true;
                break;
            }
        }

        if (found == false) {
            moose_critical(
                "Tokenizer ,,%s'' is unknown. Defaulting to 'porter'.",
                self->settings.tokenizer
            );

            strcpy(self->settings.tokenizer, "porter");
        }
    }

    char * sql_create = g_strdup_printf(
                            SQL_CODE(CREATE),
                            self->settings.tokenizer
                        );

    if (sqlite3_exec(self->handle, sql_create, NULL, NULL, NULL) != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Cannot CREATE TABLE Structure. This is pretty deadly to moosecat's core, you know?\n");
        return false;
    }

    g_free(sql_create);
    return true;
}

void moose_stprv_prepare_all_statements(MooseStorePrivate * self) {
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

sqlite3_stmt ** moose_stprv_prepare_all_statements_listed(MooseStorePrivate * self, const char ** sql_stmts, int offset, int n_stmts) {
    g_assert(self);
    sqlite3_stmt ** stmt_list = NULL;
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
            if (prep_error != SQLITE_OK) {
                REPORT_SQL_ERROR(self, "WARNING: cannot prepare statement");
            }

            /* Left for Debugging */
            /* g_print("Faulty statement: %s\n", sql); */
        }
    }

    return stmt_list;
}

/*
 * Open a sqlite3 handle to the memory.
 *
 * Returns: true on success.
 */
bool moose_strprv_open_memdb(MooseStorePrivate * self) {
    g_assert(self);
    const char * db_name = (self->settings.use_memory_db) ? ":memory:" : MOOSE_STORE_TMP_DB_PATH;

    if (sqlite3_open(db_name, &self->handle) != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "ERROR: cannot open :memory: database. Dude, that's weird...");
        self->handle = NULL;
        return false;
    }

    return moose_stprv_create_song_table(self);
}

void moose_stprv_finalize_statements(MooseStorePrivate * self, sqlite3_stmt ** stmts, int offset, int n_stmts) {
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

void moose_stprv_close_handle(MooseStorePrivate * self, bool free_statements) {
    if (free_statements) {
        moose_stprv_finalize_statements(self, self->sql_prep_stmts, STMT_SQL_NEED_TO_PREPARE_COUNT + 1, STMT_SQL_SOURCE_COUNT);
        self->sql_prep_stmts = NULL;
    }

    if (sqlite3_close(self->handle) != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Warning: Unable to close db connection");
        self->handle = NULL;
    }
    if (self->spl_stack != NULL) {
        g_ptr_array_free(self->spl_stack, true);
        self->spl_stack = NULL;
    }
}

void moose_stprv_begin(MooseStorePrivate * self) {
    if (sqlite3_step(SQL_STMT(self, BEGIN)) != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "WARNING: Unable to execute BEGIN");
    }
    sqlite3_reset(SQL_STMT(self, BEGIN));
}

void moose_stprv_commit(MooseStorePrivate * self) {
    if (sqlite3_step(SQL_STMT(self, COMMIT)) != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "WARNING: Unable to execute COMMIT");
    }
    sqlite3_reset(SQL_STMT(self, COMMIT));
}

void moose_stprv_delete_songs_table(MooseStorePrivate * self) {
    if (sqlite3_step(SQL_STMT(self, DELETE_ALL)) != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "WARNING: Cannot delete table contentes of 'songs'");
    }
    sqlite3_reset(SQL_STMT(self, DELETE_ALL));
}

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
bool moose_stprv_insert_song(MooseStorePrivate * db, MooseSong * song) {
    int error_id = SQLITE_OK;
    int pos_idx = 1;
    bool rc = true;

    /* bind basic attributes */
    // TODO TODO TODO: Start/End is empty.
    BIND_TXT(db, INSERT, pos_idx, moose_song_get_uri(song), error_id);
    BIND_INT(db, INSERT, pos_idx, 0, error_id);
    BIND_INT(db, INSERT, pos_idx, 1, error_id);
    BIND_INT(db, INSERT, pos_idx, moose_song_get_duration(song), error_id);
    BIND_INT(db, INSERT, pos_idx, moose_song_get_last_modified(song), error_id);

    /* bind tags */
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_ARTIST, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_ALBUM, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_TITLE, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_ALBUM_ARTIST, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_TRACK, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_NAME, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_GENRE, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_DATE, error_id);
    if (error_id != SQLITE_OK) {
        REPORT_SQL_ERROR(db, "WARNING: Error while binding");
    }

    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_COMPOSER, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_PERFORMER, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_COMMENT, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_DISC, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_MUSICBRAINZ_ARTISTID, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_MUSICBRAINZ_ALBUMID, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_MUSICBRAINZ_ALBUMARTISTID, error_id);
    BIND_TAG(db, INSERT, pos_idx, song, MOOSE_TAG_MUSICBRAINZ_TRACKID, error_id);

    /* Constant Value. See Create statement. */
    BIND_INT(db, INSERT, pos_idx,  0, error_id);

    BIND_INT(db, INSERT, pos_idx, moose_stprv_path_get_depth(moose_song_get_uri(song)), error_id);

    /* this is one error check for all the blocks above */
    if (error_id != SQLITE_OK) {
        REPORT_SQL_ERROR(db, "WARNING: Error while binding");
    }

    /* evaluate prepared statement, only check for errors */
    while ((error_id = sqlite3_step(SQL_STMT(db, INSERT))) == SQLITE_BUSY)
        /* Nothing */;

    /* Make sure bindings are ready for the next insert */
    CLEAR_BINDS_BY_NAME(db, INSERT);
    sqlite3_reset(SQL_STMT(db, INSERT));

    if ((rc = (error_id != SQLITE_DONE)) == true) {
        REPORT_SQL_ERROR(db, "WARNING: cannot insert into :memory: - that's pretty serious.");
    }

    return rc;
}

static gint moose_stprv_select_impl_sort_func_by_pos(gconstpointer a, gconstpointer b) {
    if (a && b) {
        int pos_a = moose_song_get_pos(*((MooseSong **)a));
        int pos_b = moose_song_get_pos(*((MooseSong **)b));

        if (pos_a == pos_b) {
            return +0;
        }

        if (pos_a <  pos_b) {
            return -1;
        }

        /* pos_a > pos_b */
        return +1;
    } else {
        return +1;    /* to sort NULL at the end */
    }
}

static gint moose_stprv_select_impl_sort_func_by_stack_ptr(gconstpointer a, gconstpointer b) {
    if (a && b) {
        MooseSong * sa = (*((MooseSong **)a));
        MooseSong * sb = (*((MooseSong **)b));

        if (sa == sb) {
            return +0;
        }

        if (sa <  sb) {
            return -1;
        }

        return +1;
    } else {
        return +1;    /* to sort NULL at the end */
    }
}

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

static MoosePlaylist * moose_stprv_build_queue_content(MooseStorePrivate * self, MoosePlaylist * to_filter) {
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
int moose_stprv_select_to_stack(MooseStorePrivate * self, const char * match_clause, bool queue_only, MoosePlaylist * stack, int limit_len) {
    int error_id = SQLITE_OK, pos_id = 1;
    limit_len = (limit_len < 0) ? INT_MAX : limit_len;

    const char * warning = NULL;
    int warning_pos = -1;
    gchar * match_clause_dup = moose_store_qp_parse(match_clause, &warning, &warning_pos);

    if (warning != NULL) {
        moose_critical("database: Query Parse Error: %d:%s", warning_pos, warning);
    }

    /* emits a warning if NULL is passed *sigh* */
    if (match_clause_dup != NULL) {
        match_clause_dup = g_strstrip(match_clause_dup);
    }

    sqlite3_stmt * select_stmt = NULL;

    /* If the query is empty anyway, we just select everything */
    if (match_clause_dup == NULL || *match_clause_dup == 0) {
        select_stmt = SQL_STMT(self, SELECT_MATCHED_ALL);
    } else {
        select_stmt = SQL_STMT(self, SELECT_MATCHED);
        BIND_TXT(self, SELECT_MATCHED, pos_id, match_clause_dup, error_id);
        BIND_INT(self, SELECT_MATCHED, pos_id, limit_len, error_id);
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

    if (error_id != SQLITE_DONE && error_id != SQLITE_ROW) {
        REPORT_SQL_ERROR(self, "WARNING: Cannot SELECT");
    }

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
void moose_stprv_load_or_save(MooseStorePrivate * self, bool is_save, const char * db_path) {
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

        if (sqlite3_errcode(pTo) != SQLITE_OK) {
            REPORT_SQL_ERROR(self, "Something went wrong during backup");
        }
    } else {
        REPORT_SQL_ERROR(self, "Cannot open disk-db");
    }

    /* Close the database connection opened on database file zFilename
    ** and return the result of this function. */
    if (sqlite3_close(pFile) != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Cannot close db in load-or-save");
    }
}

#define feed_tag(tag_enum, sql_col_pos, stmt, song) {              \
        char * value = (char *)sqlite3_column_text(stmt, sql_col_pos);  \
        if (value) {                                                   \
            moose_song_set_tag(song, tag_enum, value);                 \
        }                                                              \
}                                                               \
 
void moose_stprv_deserialize_songs(MooseStorePrivate * self) {
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
    moose_debug(
        "database: deserialized %d songs from local db. (took %2.3f)",
        progress_counter, g_timer_elapsed(timer, NULL)
    );
    moose_debug("Finished: Database Update.");
    moose_debug("Finished: Queue Update.");

    if (error_id != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "ERROR: cannot load songs from database");
    }

    sqlite3_reset(stmt);
    g_timer_destroy(timer);
}

#define SELECT_META_ATTRIBUTES(self, meta_enum, column_func, out_var, copy_func, cast_type) \
    {                                                                                       \
        int error_id = SQLITE_OK;                                                           \
        if ((error_id = sqlite3_step(SQL_STMT(self, meta_enum))) == SQLITE_ROW)             \
            out_var = (cast_type)column_func(SQL_STMT(self, meta_enum), 0);                 \
        if (error_id != SQLITE_DONE && error_id != SQLITE_ROW)                              \
            REPORT_SQL_ERROR(self, "WARNING: Cannot SELECT META");                          \
        out_var = (cast_type)copy_func((cast_type)out_var);                                 \
        sqlite3_reset(SQL_STMT(self, meta_enum));                                           \
    }                                                                                       \
 

int moose_stprv_get_song_count(MooseStorePrivate * self) {
    int song_count = -1;
    SELECT_META_ATTRIBUTES(self, COUNT, sqlite3_column_int, song_count, ABS, int);
    return song_count;
}

int moose_stprv_get_db_version(MooseStorePrivate * self) {
    int db_version = 0;
    SELECT_META_ATTRIBUTES(self, SELECT_META_DB_VERSION, sqlite3_column_int, db_version, ABS, int);
    return db_version;
}

int moose_stprv_get_pl_version(MooseStorePrivate * self) {
    int pl_version = 0;
    SELECT_META_ATTRIBUTES(self, SELECT_META_PL_VERSION, sqlite3_column_int, pl_version, ABS, int);
    return pl_version;
}

int moose_stprv_get_sc_version(MooseStorePrivate * self) {
    int sc_version = 0;
    SELECT_META_ATTRIBUTES(self, SELECT_META_SC_VERSION, sqlite3_column_int, sc_version, ABS, int);
    return sc_version;
}

int moose_stprv_get_mpd_port(MooseStorePrivate * self) {
    int mpd_port = 0;
    SELECT_META_ATTRIBUTES(self, SELECT_META_MPD_PORT, sqlite3_column_int, mpd_port, ABS, int);
    return mpd_port;
}

char * moose_stprv_get_mpd_host(MooseStorePrivate * self) {
    char * mpd_host = NULL;
    SELECT_META_ATTRIBUTES(self, SELECT_META_MPD_HOST, sqlite3_column_text, mpd_host, g_strdup, char *);
    return mpd_host;
}

int moose_stprv_queue_clip(MooseStorePrivate * self, int since_pos) {
    g_assert(self);

    int pos_idx = 1;
    int error_id = SQLITE_OK;
    sqlite3_stmt * clear_stmt = SQL_STMT(self, QUEUE_CLEAR);

    BIND_INT(self, QUEUE_CLEAR, pos_idx, MAX(-1, since_pos - 1), error_id);

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

void moose_stprv_queue_update_stack_posid(MooseStorePrivate * self) {
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

void moose_stprv_queue_insert_posid(MooseStorePrivate * self, int pos, int idx, const char * file) {
    int pos_idx = 1, error_id = SQLITE_OK;
    BIND_TXT(self, QUEUE_INSERT_ROW, pos_idx, file, error_id);
    BIND_INT(self, QUEUE_INSERT_ROW, pos_idx, pos, error_id);
    BIND_INT(self, QUEUE_INSERT_ROW, pos_idx, idx, error_id);
    pos_idx = 1;

    if (error_id != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Cannot bind stuff to INSERT statement");
        return;
    }

    if (sqlite3_step(SQL_STMT(self, QUEUE_INSERT_ROW)) != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "Unable to INSERT song into queue.");
    }

    CLEAR_BINDS_BY_NAME(self, QUEUE_INSERT_ROW);
    sqlite3_reset(SQL_STMT(self, QUEUE_INSERT_ROW));
}

int moose_stprv_path_get_depth(const char * dir_path) {
    int dir_depth = 0;
    char * cursor = (char *)dir_path;

    if (dir_path == NULL) {
        return -1;
    }

    for (;;) {
        gunichar curr = g_utf8_get_char_validated(cursor, -1);
        cursor = g_utf8_next_char(cursor);

        if (*cursor != (char)0) {
            if (curr == (gunichar)'/') {
                ++dir_depth;
            }
        } else {
            break;
        }
    }

    return dir_depth;
}

void moose_stprv_dir_insert(MooseStorePrivate * self, const char * path) {
    g_assert(self);
    g_assert(path);

    int pos_idx = 1;
    int error_id = SQLITE_OK;
    BIND_TXT(self, DIR_INSERT, pos_idx, path, error_id);
    BIND_INT(self, DIR_INSERT, pos_idx, moose_stprv_path_get_depth(path), error_id);

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

void moose_stprv_dir_delete(MooseStorePrivate * self) {
    g_assert(self);

    if (sqlite3_step(SQL_STMT(self, DIR_DELETE_ALL)) != SQLITE_DONE) {
        REPORT_SQL_ERROR(self, "cannot DELETE * from dirs");
    }
    sqlite3_reset(SQL_STMT(self, DIR_DELETE_ALL));
}

int moose_stprv_dir_select_to_stack(MooseStorePrivate * self, MoosePlaylist * stack, const char * directory, int depth) {
    g_assert(self);
    g_assert(stack);
    int returned = 0;
    int pos_idx = 1;
    int error_id = SQLITE_OK;

    if (directory && *directory == '/') {
        directory = NULL;
    }

    sqlite3_stmt * select_stmt = NULL;

    if (directory != NULL && depth < 0) {
        select_stmt = SQL_STMT(self, DIR_SEARCH_PATH);
        BIND_TXT(self, DIR_SEARCH_PATH, pos_idx, directory, error_id);
        BIND_TXT(self, DIR_SEARCH_PATH, pos_idx, directory, error_id);
    } else if (directory == NULL && depth >= 0) {
        select_stmt = SQL_STMT(self, DIR_SEARCH_DEPTH);
        BIND_INT(self, DIR_SEARCH_DEPTH, pos_idx, depth, error_id);
        BIND_INT(self, DIR_SEARCH_DEPTH, pos_idx, depth, error_id);
    } else if (directory != NULL && depth >= 0) {
        select_stmt = SQL_STMT(self, DIR_SEARCH_PATH_AND_DEPTH);
        BIND_INT(self, DIR_SEARCH_PATH_AND_DEPTH, pos_idx, depth, error_id);
        BIND_TXT(self, DIR_SEARCH_PATH_AND_DEPTH, pos_idx, directory, error_id);
        BIND_INT(self, DIR_SEARCH_PATH_AND_DEPTH, pos_idx, depth, error_id);
        BIND_TXT(self, DIR_SEARCH_PATH_AND_DEPTH, pos_idx, directory, error_id);
    } else {
        return -1;
    }

    if (error_id != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Cannot bind stuff to directory select");
        return -2;
    } else {
        while (sqlite3_step(select_stmt) == SQLITE_ROW) {
            const char * rowid = (const char *)sqlite3_column_text(select_stmt, 0);
            const char * path = (const char *)sqlite3_column_text(select_stmt, 1);
            moose_playlist_append(stack, g_strjoin(":", rowid, path, NULL));
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

typedef struct {
    MooseStorePrivate * store;
    GAsyncQueue * queue;
} MooseStoreQueueTag;

/* Popping this from a GAsyncQueue means
 * the queue is empty and we night to do some actions
 */
#define EMPTY_QUEUE_INDICATOR 0x1

static gpointer moose_stprv_do_list_all_info_sql_thread(gpointer user_data) {
    MooseStoreQueueTag * tag = user_data;
    MooseStorePrivate * self = tag->store;
    GAsyncQueue * queue = tag->queue;

    struct mpd_entity * ent = NULL;

    /* Begin a new transaction */
    moose_stprv_begin(self);

    while ((gpointer)(ent = g_async_queue_pop(queue)) != queue) {
        switch (mpd_entity_get_type(ent)) {
        case MPD_ENTITY_TYPE_SONG: {

            MooseSong * song = moose_song_new_from_struct(
                                   (struct mpd_song *)mpd_entity_get_song(ent)
                               );

            moose_playlist_append(self->stack, song);
            moose_stprv_insert_song(self, song);
            mpd_entity_free(ent);

            break;
        }

        case MPD_ENTITY_TYPE_DIRECTORY: {
            const struct mpd_directory * dir = mpd_entity_get_directory(ent);

            if (dir != NULL) {
                moose_stprv_dir_insert(self, mpd_directory_get_path(dir));
                mpd_entity_free(ent);
            }

            break;
        }

        case MPD_ENTITY_TYPE_PLAYLIST:
        default: {
            mpd_entity_free(ent);
            ent = NULL;
        }
        }
    }

    /* Commit changes */
    moose_stprv_commit(self);

    return NULL;
}

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
 */
void moose_stprv_oper_listallinfo(MooseStorePrivate * store, volatile gboolean * cancel) {
    g_assert(store);
    g_assert(store->client);

    int progress_counter = 0;
    size_t db_version = 0;
    MooseStatus * status = moose_client_ref_status(store->client);

    int number_of_songs = moose_status_stats_get_number_of_songs(status);

    db_version = moose_stprv_get_db_version(store);

    if (store->force_update_listallinfo == false) {
        size_t db_update_time = moose_status_stats_get_db_update_time(status);

        if (db_update_time  == db_version) {
            moose_message("database: Will not update database, timestamp didn't change.");
            return;
        } else {
            moose_message(
                "database: Will update database (%u != %u)",
                (unsigned)db_update_time, (unsigned)db_version
            );
        }
    } else {
        moose_message("database: Doing forced listallinfo");
    }

    moose_status_unref(status);

    GTimer * timer = NULL;
    GAsyncQueue * queue = g_async_queue_new();
    GThread * sql_thread = NULL;

    MooseStoreQueueTag tag;
    tag.queue = queue;
    tag.store = store;

    /* We're building the whole table new,
     * so throw away old data */
    moose_stprv_delete_songs_table(store);
    moose_stprv_dir_delete(store);

    /* Also throw away current stack, and reallocate it
     * to the fitting size.
     *
     * Apparently GPtrArray does not support reallocating,
     * without adding NULL-cells to the array? */
    if (store->stack != NULL) {
        g_object_unref(store->stack);
    }

    store->stack = moose_playlist_new_full(
                       number_of_songs + 1,
                       (GDestroyNotify)moose_song_unref
                   );

    /* Profiling */
    timer = g_timer_new();

    sql_thread = g_thread_new(
                     "sql-thread",
                     moose_stprv_do_list_all_info_sql_thread,
                     &tag
                 );

    struct mpd_connection * conn = moose_client_get(store->client);
    if (conn != NULL) {
        struct mpd_entity * ent = NULL;

        g_timer_start(timer);

        /* Order the real big list */
        mpd_send_list_all_meta(conn, "/");

        while ((ent = mpd_recv_entity(conn)) != NULL) {
            if (moose_job_manager_check_cancel(store->jm, cancel)) {
                moose_warning("database: listallinfo cancelled!");
                break;
            }

            ++progress_counter;

            g_async_queue_push(queue, ent);
        }

        /* This should only happen if the operation was cancelled */
        if (mpd_response_finish(conn) == false) {
            moose_client_check_error(store->client, conn);
        }
    }
    moose_client_put(store->client);

    /* tell SQL thread kindly to die, but wait for him to bleed */
    g_async_queue_push(queue, queue);
    g_thread_join(sql_thread);

    moose_message(
        "database: retrieved %d songs from mpd (took %2.3fs)",
        number_of_songs, g_timer_elapsed(timer, NULL)
    );

    moose_debug("Finished: Database update.");

    g_async_queue_unref(queue);
    g_timer_destroy(timer);
}

static gpointer moose_stprv_do_plchanges_sql_thread(gpointer user_data) {
    MooseStoreQueueTag * tag = user_data;
    MooseStorePrivate * self = tag->store;
    GAsyncQueue * queue = tag->queue;

    int clipped = 0;
    bool first_song_passed = false;
    MooseSong * song = NULL;
    GTimer * timer = g_timer_new();
    gdouble clip_time = 0.0, posid_time = 0.0, stack_time = 0.0;

    /* start a transaction */
    moose_stprv_begin(self);

    while ((gpointer)(song = g_async_queue_pop(queue)) != queue) {
        if (first_song_passed == false || song == (gpointer)EMPTY_QUEUE_INDICATOR) {
            g_timer_start(timer);

            int start_position = -1;
            if (song != (gpointer)EMPTY_QUEUE_INDICATOR) {
                start_position = moose_song_get_pos(song);
            }
            clipped = moose_stprv_queue_clip(self, start_position);
            clip_time = g_timer_elapsed(timer, NULL);
            g_timer_start(timer);

            first_song_passed = true;
        }

        if (song != (gpointer)EMPTY_QUEUE_INDICATOR) {
            moose_stprv_queue_insert_posid(
                self,
                moose_song_get_pos(song),
                moose_song_get_id(song),
                moose_song_get_uri(song)
            );
            moose_song_unref(song);
        }
    }

    posid_time = g_timer_elapsed(timer, NULL);

    if (clipped > 0) {
        moose_debug("database: Clipped %d songs.", clipped);
    }

    /* Commit all those update statements */
    moose_stprv_commit(self);

    /* Update the pos/id data in the song stack (=> sync with db) */
    g_timer_start(timer);
    moose_stprv_queue_update_stack_posid(self);
    stack_time = g_timer_elapsed(timer, NULL);
    g_timer_destroy(timer);

    moose_debug(
        "database: QueueSQL Timing: %2.3fs Clip | %2.3fs Posid | %2.3fs Stack",
        clip_time, posid_time, stack_time
    );

    return NULL;
}

void moose_stprv_oper_plchanges(MooseStorePrivate * store, volatile gboolean * cancel) {
    g_assert(store);

    int progress_counter = 0;
    size_t last_pl_version = 0;

    /* get last version of the queue (which we're having already.
     * (in case full queue is wanted take first version) */
    if (store->force_update_plchanges == false) {
        last_pl_version = moose_stprv_get_pl_version(store);
    }

    if (store->force_update_plchanges == false) {
        size_t current_pl_version = -1;
        MooseStatus * status = moose_client_ref_status(store->client);
        if (status != NULL) {
            current_pl_version = moose_status_get_queue_version(status);
        } else {
            current_pl_version = last_pl_version;
        }
        moose_status_unref(status);

        if (last_pl_version == current_pl_version) {
            moose_message(
                "database: Will not update queue, version didn't change (%d == %d)",
                (int)last_pl_version, (int)current_pl_version
            );
            return;
        }
    }

    GAsyncQueue * queue = g_async_queue_new();
    GThread * sql_thread = NULL;
    GTimer * timer = NULL;

    MooseStoreQueueTag tag;
    tag.queue = queue;
    tag.store = store;

    /* needs to be started after inserting meta attributes, since it calls 'begin;' */
    sql_thread = g_thread_new("queue-update", moose_stprv_do_plchanges_sql_thread, &tag);

    /* timing */
    timer = g_timer_new();

    /* Now do the actual hard work, send the actual plchanges command,
     * loop over the retrieved contents, and push them to the SQL Thread */
    struct mpd_connection * conn = moose_client_get(store->client);
    if (conn != NULL) {
        g_timer_start(timer);

        moose_message(
            "database: Queue was updated. Will do ,,plchanges %d''",
            (int)last_pl_version
        );

        if (mpd_send_queue_changes_meta(conn, last_pl_version)) {
            struct mpd_song * song_struct = NULL;

            while ((song_struct = mpd_recv_song(conn)) != NULL) {
                MooseSong * song = moose_song_new_from_struct(song_struct);
                mpd_song_free(song_struct);

                if (moose_job_manager_check_cancel(store->jm, cancel)) {
                    moose_warning("database: plchanges canceled!");
                    break;
                }

                ++progress_counter;

                g_async_queue_push(queue, song);
            }

            /* Empty queue - we need to notift he other thread
             * */
            if (progress_counter == 0) {
                g_async_queue_push(queue, (gpointer)EMPTY_QUEUE_INDICATOR);
            }
        }

        if (mpd_response_finish(conn) == false) {
            moose_client_check_error(store->client, conn);
        }
    }
    moose_client_put(store->client);

    /* Killing the SQL thread softly. */
    g_async_queue_push(queue, queue);
    g_thread_join(sql_thread);

    /* a bit of timing report */
    moose_debug(
        "database: updated %d song's pos/id (took %2.3fs)",
        progress_counter, g_timer_elapsed(timer, NULL)
    );

    moose_debug("Finished: Queue updated.");

    g_async_queue_unref(queue);
    g_timer_destroy(timer);
}

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
 * On startup OR on MOOSE_IDLE_STORED_PLAYLIST:
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

static char * moose_stprv_spl_construct_table_name(struct mpd_playlist * playlist) {
    g_assert(playlist);
    return g_strdup_printf("spl_%x_%s",
                           (unsigned)mpd_playlist_get_last_modified(playlist),
                           mpd_playlist_get_path(playlist));
}

static char * moose_stprv_spl_parse_table_name(const char * table_name, /* OUT */ time_t * last_modified) {
    g_assert(table_name);

    for (size_t i = 0; table_name[i] != 0; ++i) {
        if (table_name[i] == '_') {
            if (last_modified != NULL) {
                *last_modified = g_ascii_strtoll(&table_name[i + 1], NULL, 16);
            }

            char * name_begin = strchr(&table_name[i + 1], '_');

            if (name_begin != NULL) {
                return g_strdup(name_begin + 1);
            } else {
                return NULL;
            }
        }
    }

    return NULL;
}

static void moose_stprv_spl_drop_table(MooseStorePrivate   * self, const char * table_name) {
    g_assert(table_name);
    g_assert(self);
    char * sql = sqlite3_mprintf("DROP TABLE IF EXISTS %q;", table_name);
    if (sql != NULL) {
        if (sqlite3_exec(self->handle, sql, 0, 0, 0) != SQLITE_OK) {
            REPORT_SQL_ERROR(self, "Cannot DROP some stored playlist table...");
        }

        sqlite3_free(sql);
    }
}

static void moose_stprv_spl_create_table(MooseStorePrivate * self, const char * table_name) {
    g_assert(self);
    g_assert(table_name);
    char * sql = sqlite3_mprintf(
                     "CREATE TABLE IF NOT EXISTS %q (song_idx INTEGER);", table_name
                 );

    if (sql != NULL) {
        if (sqlite3_exec(self->handle, sql, 0, 0, 0) != SQLITE_OK) {
            REPORT_SQL_ERROR(self, "Cannot CREATE some stored playlist table...");
        }

        sqlite3_free(sql);
    }
}

static void moose_stprv_spl_delete_content(MooseStorePrivate * self, const char * table_name) {
    g_assert(self);
    g_assert(table_name);
    moose_stprv_spl_drop_table(self, table_name);
    moose_stprv_spl_create_table(self, table_name);
}

static struct mpd_playlist * moose_stprv_spl_name_to_playlist(MooseStorePrivate * store, const char * playlist_name) {
    g_assert(store);

    struct mpd_playlist * playlist_result = NULL;
    unsigned length = store->spl_stack->len;

    for (unsigned i = 0; i < length; ++i) {
        struct mpd_playlist * playlist = g_ptr_array_index(store->spl_stack, i);

        if (playlist && g_strcmp0(playlist_name, mpd_playlist_get_path(playlist)) == 0) {
            playlist_result = playlist;
        }
    }

    return playlist_result;
}

static void moose_stprv_spl_listplaylists(MooseStorePrivate * store) {
    g_assert(store);
    g_assert(store->client);
    MooseClient * self = store->client;

    if (store->spl_stack != NULL) {
        g_ptr_array_free(store->spl_stack, true);
        store->spl_stack = NULL;
    }

    store->spl_stack = g_ptr_array_new_with_free_func((GDestroyNotify)mpd_playlist_free);

    struct mpd_connection * conn = moose_client_get(self);
    if (conn != NULL) {
        struct mpd_playlist * playlist = NULL;
        mpd_send_list_playlists(conn);

        while ((playlist = mpd_recv_playlist(conn)) != NULL) {
            g_ptr_array_add(store->spl_stack, playlist);
        }

        if (mpd_response_finish(conn) == false) {
            moose_client_check_error(self, conn);
        }
    } else {
        moose_critical("Cannot get connection to get listplaylists (not connected?)");
    }
    moose_client_put(self);
}

static GList * moose_stprv_spl_get_loaded_list(MooseStorePrivate * self) {
    GList * table_name_list = NULL;

    /* Select all table names from db that start with spl_ */
    while (sqlite3_step(SQL_STMT(self, SPL_SELECT_TABLES)) == SQLITE_ROW) {
        table_name_list = g_list_prepend(
                              table_name_list,
                              g_strdup(
                                  (char *)sqlite3_column_text(SQL_STMT(self, SPL_SELECT_TABLES), 0)
                              )
                          );
    }

    /* Be nice, and always check for errors */
    int error = sqlite3_errcode(self->handle);

    if (error != SQLITE_DONE && error != SQLITE_OK) {
        REPORT_SQL_ERROR(self, "Some error while selecting all playlists");
    } else {
        CLEAR_BINDS_BY_NAME(self, SPL_SELECT_TABLES);
    }

    return table_name_list;
}

static int moose_stprv_spl_get_song_count(MooseStorePrivate * self, struct mpd_playlist * playlist) {
    g_assert(self);

    int count = -1;
    char * table_name = moose_stprv_spl_construct_table_name(playlist);

    if (table_name != NULL) {
        char * dyn_sql = sqlite3_mprintf("SELECT count(*) FROM %q;", table_name);

        if (dyn_sql != NULL) {
            sqlite3_stmt * count_stmt = NULL;

            if (sqlite3_prepare_v2(self->handle, dyn_sql, -1, &count_stmt, NULL) != SQLITE_OK) {
                REPORT_SQL_ERROR(self, "Cannot prepare COUNT stmt!");
            } else {
                if (sqlite3_step(count_stmt) == SQLITE_ROW) {
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

void moose_stprv_spl_update(MooseStorePrivate * self) {
    g_assert(self);

    /* Get a new list of playlists from mpd */
    moose_stprv_spl_listplaylists(self);

    GList * table_name_list = moose_stprv_spl_get_loaded_list(self);

    moose_message("database: Updating stored playlists...");

    /* Filter all Playlist Tables that do not exist anymore in the current playlist stack.
     * i.e. those that were deleted, or accidentally created. */
    for (GList * iter = table_name_list; iter;) {
        char * drop_table_name = iter->data;

        if (drop_table_name != NULL) {
            bool is_valid = false;

            for (unsigned i = 0; i < self->spl_stack->len; ++i) {
                struct mpd_playlist * playlist = g_ptr_array_index(self->spl_stack, i);
                char * valid_table_name = moose_stprv_spl_construct_table_name(playlist);
                is_valid = (g_strcmp0(valid_table_name, drop_table_name) == 0);
                g_free(valid_table_name);

                if (is_valid) {
                    break;
                }
            }

            if (is_valid == false) {
                /* drop invalid table */
                moose_message(
                    "database: Dropping orphaned playlist-table ,,%s''",
                    drop_table_name
                );
                moose_stprv_spl_drop_table(self, drop_table_name);
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
    for (unsigned i = 0; i < self->spl_stack->len; ++i) {
        struct mpd_playlist * playlist = g_ptr_array_index(self->spl_stack, i);

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
                char * old_pl_name = moose_stprv_spl_parse_table_name(old_table_name, &old_pl_time);

                if (g_strcmp0(old_pl_name, mpd_playlist_get_path(playlist)) == 0) {
                    time_t new_pl_time = mpd_playlist_get_last_modified(playlist);

                    if (new_pl_time == old_pl_time) {
                        /* nothing has changed */
                    } else if (new_pl_time > old_pl_time) {
                        /* reload old_pl? */
                        moose_stprv_spl_load(self, playlist);
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

    moose_debug("Finished: Stored Playlist update.");
}

bool moose_stprv_spl_is_loaded(MooseStorePrivate * store, struct mpd_playlist * playlist) {
    bool result = false;

    /* Construct the table name for comparasion */
    char * table_name = moose_stprv_spl_construct_table_name(playlist);
    GList * table_names = moose_stprv_spl_get_loaded_list(store);

    for (GList * iter = table_names; iter != NULL; iter = iter->next) {
        char * comp_table_name = iter->data;
        if (g_ascii_strcasecmp(comp_table_name, table_name) == 0) {

            /* Get the actual playlist name from the table name and the last_mod date */
            time_t comp_last_mod = 0;
            g_free(moose_stprv_spl_parse_table_name(
                       comp_table_name, &comp_last_mod
                   ));

            if (comp_last_mod == mpd_playlist_get_last_modified(playlist)) {
                result = true;
                break;
            }
        }
    }

    g_free(table_name);
    g_list_free_full(table_names, g_free);

    return result;
}

bool moose_stprv_spl_load(MooseStorePrivate * store, struct mpd_playlist * playlist) {
    g_assert(store);
    g_assert(store->client);
    g_assert(playlist);

    bool successfully_loaded = false;
    MooseClient * self = store->client;

    if (moose_stprv_spl_is_loaded(store, playlist)) {
        moose_message(
            "database: Stored playlist '%s' already loaded - skipping.",
            mpd_playlist_get_path(playlist)
        );
        return true;
    } else {
        moose_message(
            "database: Loading stored playlist '%s'...",
            mpd_playlist_get_path(playlist)
        );
    }

    sqlite3_stmt * insert_stmt = NULL;
    char * table_name = moose_stprv_spl_construct_table_name(playlist);

    if (table_name != NULL) {
        moose_stprv_begin(store);
        moose_stprv_spl_delete_content(store, table_name);
        char * sql = sqlite3_mprintf(
                         "INSERT INTO %q VALUES((SELECT rowid FROM songs_content WHERE c0uri  = ?));",
                         table_name
                     );

        if (sql != NULL) {
            if (sqlite3_prepare_v2(store->handle, sql, -1, &insert_stmt, NULL) != SQLITE_OK) {
                REPORT_SQL_ERROR(store, "Cannot prepare INSERT stmt!");
                return false;
            }

            moose_message("database: Loading stored playlist ,,%s'' from MPD", table_name);
            sqlite3_free(sql);

            /* Acquire the connection (this locks the connection for others) */
            struct mpd_connection * conn = moose_client_get(self);
            if (conn != NULL) {

                if (mpd_send_list_playlist(conn, mpd_playlist_get_path(playlist))) {
                    struct mpd_pair * file_pair = NULL;

                    while ((file_pair = mpd_recv_pair_named(conn, "file")) != NULL) {
                        if (sqlite3_bind_text(insert_stmt, 1, file_pair->value, -1, NULL) != SQLITE_OK) {
                            REPORT_SQL_ERROR(store, "Cannot bind filename to SPL-Insert");
                        }

                        if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
                            REPORT_SQL_ERROR(store, "Cannot insert song index to stored playlist.");
                        }

                        CLEAR_BINDS(insert_stmt);
                        mpd_return_pair(conn, file_pair);
                    }

                    if (mpd_response_finish(conn) == FALSE) {
                        moose_client_check_error(self, conn);
                    } else {
                        moose_debug("Stored Playlist Sync");
                        successfully_loaded = true;
                    }
                }
            }

            /* Release the connection mutex */
            moose_client_put(self);

            if (sqlite3_finalize(insert_stmt) != SQLITE_OK) {
                REPORT_SQL_ERROR(store, "Cannot finalize INSERT stmt");
            }
        }

        moose_stprv_commit(store);
        g_free(table_name);
    }

    return successfully_loaded;
}

bool moose_stprv_spl_load_by_playlist_name(MooseStorePrivate * store, const char * playlist_name) {
    g_assert(store);
    g_assert(playlist_name);
    struct mpd_playlist * playlist = moose_stprv_spl_name_to_playlist(store, playlist_name);

    if (playlist != NULL) {
        moose_stprv_spl_load(store, playlist);
        return true;
    } else {
        moose_critical("Could not find stored playlist ,,%s''", playlist_name);
        return false;
    }
}

static inline int moose_stprv_spl_hash_compare(gconstpointer a, gconstpointer b) {
    return *((gconstpointer **)a) - *((gconstpointer **)b);
}

static int moose_stprv_spl_bsearch_cmp(const void * key, const void * array) {
    /* We only compare the pointers. The type is not important here. */
    return (MooseSong *)key - *(MooseSong **)array;
}

static int moose_stprv_spl_filter_id_list(MooseStorePrivate * store, GPtrArray * song_ptr_array, const char * match_clause, MoosePlaylist * out_stack) {
    /* Roughly estimate the number of song pointer to expect */
    int preallocations = moose_playlist_length(store->stack) /
                         (((match_clause) ? MAX(strlen(match_clause), 1) : 1) * 2);

    /* Temp. container to hold the query on the database */
    MoosePlaylist * db_songs = moose_playlist_new_full(preallocations, NULL);

    if (moose_stprv_select_to_stack(store, match_clause, false, db_songs, -1) > 0) {
        for (size_t i = 0; i < moose_playlist_length(db_songs); ++i) {
            MooseSong * song = moose_playlist_at(db_songs, i);
            if (bsearch(
                        song,
                        song_ptr_array->pdata,
                        song_ptr_array->len,
                        sizeof(gpointer),
                        moose_stprv_spl_bsearch_cmp
                    )
               ) {
                moose_playlist_append(out_stack, song);
            }
        }
    }

    /* Set them free */
    g_object_unref(db_songs);

    return moose_playlist_length(out_stack);
}

int moose_stprv_spl_select_playlist(MooseStorePrivate * store, MoosePlaylist * out_stack, const char * playlist_name, const char * match_clause) {
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
    char * table_name, * dyn_sql;
    GPtrArray * song_ptr_array = NULL;
    sqlite3_stmt * pl_id_stmt = NULL;
    struct mpd_playlist * playlist;

    if (match_clause && *match_clause == 0) {
        match_clause = NULL;
    }

    /* Try to find the playlist structure in the playlist_struct list by name */
    if ((playlist = moose_stprv_spl_name_to_playlist(store, playlist_name)) == NULL) {
        return -1;
    }

    /* Use the acquired playlist to find out the correct playlist name */
    if ((table_name = moose_stprv_spl_construct_table_name(playlist)) == NULL) {
        return -2;
    }

    /* Use the table name to construct a select on this table */
    if ((dyn_sql = sqlite3_mprintf("SELECT song_idx FROM %q;", table_name)) == NULL) {
        sqlite3_free(table_name);
        return -3;
    }

    if (sqlite3_prepare_v2(store->handle, dyn_sql, -1, &pl_id_stmt, NULL) != SQLITE_OK) {
        REPORT_SQL_ERROR(store, "Cannot prepare playlist search statement");
    } else {
        int song_count = moose_stprv_spl_get_song_count(store, playlist);
        song_ptr_array = g_ptr_array_sized_new(MAX(song_count, 1));

        while (sqlite3_step(pl_id_stmt) == SQLITE_ROW) {
            /* Get the actual song */
            int song_idx = sqlite3_column_int(pl_id_stmt, 0);
            if (0 < song_idx && song_idx <= (int)moose_playlist_length(store->stack)) {
                MooseSong * song = moose_playlist_at(store->stack, song_idx - 1);

                /* Remember the pointer */
                g_ptr_array_add(song_ptr_array, song);
            }
        }

        if (sqlite3_errcode(store->handle) != SQLITE_DONE) {
            REPORT_SQL_ERROR(store, "Error while matching stuff in playlist");
        } else {
            if (sqlite3_finalize(pl_id_stmt) != SQLITE_OK) {
                REPORT_SQL_ERROR(store, "Error while finalizing getid statement");
            } else {
                g_ptr_array_sort(song_ptr_array, moose_stprv_spl_hash_compare);
                rc = moose_stprv_spl_filter_id_list(store, song_ptr_array, match_clause, out_stack);
            }
        }

        g_ptr_array_free(song_ptr_array, true);
    }

    g_free(table_name);
    sqlite3_free(dyn_sql);

    return rc;
}

int moose_stprv_spl_get_loaded_playlists(MooseStorePrivate * store, MoosePlaylist * stack) {
    g_assert(store);
    g_assert(stack);

    int rc = 0;
    GList * table_name_list = moose_stprv_spl_get_loaded_list(store);

    if (table_name_list != NULL) {
        for (GList * iter = table_name_list; iter; iter = iter->next) {
            char * table_name = iter->data;

            if (table_name != NULL) {
                char * playlist_name = moose_stprv_spl_parse_table_name(table_name, NULL);

                if (playlist_name != NULL) {
                    struct mpd_playlist * playlist = moose_stprv_spl_name_to_playlist(store, playlist_name);

                    if (playlist != NULL) {
                        moose_playlist_append(stack, playlist);
                        ++rc;
                    }

                    g_free(playlist_name);
                }

                g_free(table_name);
            }
        }

        g_list_free(table_name_list);
    }

    return rc;
}

int moose_stprv_spl_get_known_playlists(MooseStorePrivate * store, GPtrArray * stack) {
    for (unsigned i = 0; i < store->spl_stack->len; ++i) {
        g_ptr_array_add(
            stack, g_ptr_array_index(store->spl_stack, i)
        );
    }
    return store->spl_stack->len;
}
