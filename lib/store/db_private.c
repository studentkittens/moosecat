#include "db_private.h"

/* strlen() */
#include <string.h>

/* gmtime(), strftime() */
#define _POSIX_SOURCE
#include <time.h>

/*
 * Note:
 * =====
 *
 * Database code is always a bit messy apparently.
 * I tried my best to keep this readable.
 *
 * Please excuse any eye cancer while reading this. Thank you!
 */

///////////////////

/* this enum mirros the order in the CREATE statement */
enum
{
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
    SQL_COL_COMPOSER,
    SQL_COL_PERFORMER,
    SQL_COL_COMMENT,
    SQL_COL_DISC,
    SQL_COL_MUSICBRAINZ_ARTIST_ID,
    SQL_COL_MUSICBRAINZ_ALBUM_ID,
    SQL_COL_MUSICBRAINZ_ALBUMARTIST_ID,
    SQL_COL_MUSICBRAINZ_TRACK_ID
};

///////////////////

static const char * _sql_stmts[] =
{
    [_MC_SQL_CREATE] =
    "-- Note: Type information is not parsed at all, and rather meant as hint for the developer.    \n"
    "CREATE VIRTUAL TABLE IF NOT EXISTS songs USING Fts4(                                           \n"
    "uri            TEXT UNIQUE NOT NULL,     -- Path to file, or URL to webradio etc.              \n"
    "start          INTEGER NOT NULL,         -- Start of the virtual song within the physical file \n"
    "end            INTEGER NOT NULL,         -- End of the virtual song within the physical file   \n"
    "duration       INTEGER NOT NULL,         -- Songudration in Seconds. (0 means unknown)         \n"
    "last_modified  INTEGER NOT NULL,         -- Last modified as Posix UTC Timestamp               \n"
    "-- Tags:                                                                                       \n"
    "artist         TEXT,                                                                           \n"
    "album          TEXT,                                                                           \n"
    "title          TEXT,                                                                           \n"
    "album_artist   TEXT,                                                                           \n"
    "track          TEXT,                                                                           \n"
    "name           TEXT,                                                                           \n"
    "genre          TEXT,                                                                           \n"
    "composer       TEXT,                                                                           \n"
    "performer      TEXT,                                                                           \n"
    "comment        TEXT,                                                                           \n"
    "disc           TEXT,                                                                           \n"
    "-- MB-Tags:                                                                                    \n"
    "musicbrainz_artist_id       TEXT,                                                              \n"
    "musicbrainz_album_id        TEXT,                                                              \n"
    "musicbrainz_albumartist_id  TEXT,                                                              \n"
    "musicbrainz_track           TEXT,                                                              \n"
    "-- FTS options:                                                                                \n"
    "tokenize=porter -- todo: make this interchangeable.                                            \n"
    ");                                                                                             \n",
    [_MC_SQL_META] =
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
    "INSERT INTO meta VALUES(%d, %d, %d, %d, '%s');                                  \n"
    "COMMIT;                                                                         \n",
    /* binding column names does not seem to work well.. */
    [_MC_SQL_SELECT_META_DB_VERSION] =
    "SELECT db_version FROM meta;",
    [_MC_SQL_SELECT_META_PL_VERSION] =
    "SELECT pl_version FROM meta;",
    [_MC_SQL_SELECT_META_SC_VERSION] =
    "SELECT sc_version FROM meta;",
    [_MC_SQL_SELECT_META_MPD_PORT] =
    "SELECT mpd_port FROM meta;",
    [_MC_SQL_SELECT_META_MPD_HOST] =
    "SELECT mpd_host FROM meta;",
    [_MC_SQL_COUNT] =
    "SELECT count(*) FROM songs;",
    [_MC_SQL_INSERT] =
    "INSERT INTO songs VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
    [_MC_SQL_SELECT_MATCHED] =
    "SELECT rowid FROM songs WHERE artist MATCH ?;",
    [_MC_SQL_SELECT_MATCHED_ALL] =
    "SELECT rowid FROM songs;",
    [_MC_SQL_SELECT_ALL] =
    "SELECT * FROM songs;",
    [_MC_SQL_BEGIN] =
    "BEGIN IMMEDIATE;",
    [_MC_SQL_COMMIT] =
    "COMMIT;",
    [_MC_SQL_SOURCE_COUNT] =
    ""
};

///////////////////

#define SQL_CODE(NAME) \
    _sql_stmts[_MC_SQL_##NAME]

#define SQL_STMT(STORE, NAME) \
    STORE->sql_prep_stmts[_MC_SQL_##NAME]

////////////////////////////////

void mc_stprv_insert_meta_attributes (mc_StoreDB * self)
{
    char * dyn_insert_meta = g_strdup_printf (SQL_CODE (META),
                             mpd_stats_get_db_update_time (self->client->stats),
                             mpd_status_get_queue_version,
                             MC_DB_SCHEMA_VERSION,
                             self->client->_port,
                             self->client->_host
                                             );

    if (sqlite3_exec (self->handle, dyn_insert_meta, NULL, NULL, NULL) != SQLITE_OK)
        g_print ("Cannot insert meta attributes.\n");

    g_free (dyn_insert_meta);
}

////////////////////////////////

void mc_stprv_prepare_all_statements (mc_StoreDB * self)
{
    int prep_error = 0;

    if (sqlite3_exec (self->handle, _sql_stmts[_MC_SQL_CREATE], NULL, NULL, NULL) != SQLITE_OK)
    {
        g_print ("Cannot CREATE table structure. This is pretty fatal, you know?\n");
    }

    mc_stprv_insert_meta_attributes (self);

    for (int i = _MC_SQL_NEED_TO_PREPARE_COUNT; i < _MC_SQL_SOURCE_COUNT; i++)
    {
        prep_error = sqlite3_prepare_v2 (
                         self->handle,
                         _sql_stmts[i],
                         strlen (_sql_stmts[i]) + 1,
                         &self->sql_prep_stmts[i],
                         NULL);

        /* Uh-Oh. Typo? */
        if (prep_error != SQLITE_OK)
            g_print ("WARNING: Cannot prepare statement #%d: %s\n",
                     i, sqlite3_errmsg (self->handle) );
    }
}

////////////////////////////////

void mc_stprv_finalize_all_statements (mc_StoreDB * self)
{
    for (int i = _MC_SQL_NEED_TO_PREPARE_COUNT; i < _MC_SQL_SOURCE_COUNT; i++)
    {
        if (sqlite3_finalize (self->sql_prep_stmts[i]) != SQLITE_OK)
            g_print ("WARNING: Cannot finalize statement #%d: %s\n",
                     i, sqlite3_errmsg (self->handle) );
    }
}

////////////////////////////////

bool mc_strprv_open_memdb (mc_StoreDB * self)
{
    if (sqlite3_open (":memory:", &self->handle) != SQLITE_OK)
    {
        g_print ("Cannot open db: %s\n", sqlite3_errmsg (self->handle) );
        self->handle = NULL;
        return false;
    }
    return true;
}

////////////////////////////////

void mc_stprv_begin (mc_StoreDB * self)
{
    if ( sqlite3_step (SQL_STMT (self, BEGIN) ) != SQLITE_DONE)
        g_print ("Unable to execute BEGIN\n");
}

////////////////////////////////

void mc_stprv_commit (mc_StoreDB * self)
{
    if ( sqlite3_step (SQL_STMT (self, COMMIT) ) != SQLITE_DONE)
        g_print ("Unable to execute COMMIT\n");
}

////////////////////////////////

/* Make binding values to prepared statements less painful */
#define bind_int(db, type, pos_idx, value, error_id) \
    error_id |= sqlite3_bind_int (SQL_STMT(db, type), (pos_idx)++, value);

#define bind_txt(db, type, pos_idx, value, error_id) \
    error_id |= sqlite3_bind_text (SQL_STMT(db, type), (pos_idx)++, value, -1, NULL);

#define bind_tag(db, type, pos_idx, song, tag_id, error_id) \
    bind_txt (db, type, pos_idx, mpd_song_get_tag (song, tag_id, 0), error_id)

bool mc_stprv_insert_song (mc_StoreDB * db, mpd_song * song)
{
    int error_id = SQLITE_OK;
    int pos_idx = 1;
    bool rc = true;

    /* bind basic attributes */
    bind_txt (db, INSERT, pos_idx, mpd_song_get_uri (song), error_id);
    bind_int (db, INSERT, pos_idx, mpd_song_get_start (song), error_id);
    bind_int (db, INSERT, pos_idx, mpd_song_get_end (song), error_id);
    bind_int (db, INSERT, pos_idx, mpd_song_get_duration (song), error_id);
    bind_int (db, INSERT, pos_idx, mpd_song_get_last_modified (song), error_id);

    /* bind tags */
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_ARTIST, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_ALBUM, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_TITLE, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_ALBUM_ARTIST, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_TRACK, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_NAME, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_GENRE, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_COMPOSER, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_PERFORMER, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_COMMENT, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_DISC, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_MUSICBRAINZ_ARTISTID, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_MUSICBRAINZ_ALBUMID, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_MUSICBRAINZ_ALBUMARTISTID, error_id);
    bind_tag (db, INSERT, pos_idx, song, MPD_TAG_MUSICBRAINZ_TRACKID, error_id);

    /* this is one error check for all the blocks above */
    if (error_id != SQLITE_OK)
        g_print ("INSERT: Error while binding: #%d\n", error_id);

    /* evaluate prepared statement, only check for errors */
    while ( (error_id = sqlite3_step (SQL_STMT (db, INSERT) ) ) == SQLITE_BUSY)
        /* Nothing */;

    /* Make sure bindings are ready for the next insert */
    sqlite3_reset (SQL_STMT (db, INSERT) );
    sqlite3_clear_bindings (SQL_STMT (db, INSERT) );

    if ( (rc = (error_id != SQLITE_DONE) ) == true)
        g_print ("Cannot INSERT into :memory: : %s\n", sqlite3_errmsg (db->handle) );

    return rc;
}


////////////////////////////////

int mc_stprv_select_out (mc_StoreDB * self, const char * match_clause, mpd_song ** song_buffer, int buffer_len)
{
    int error_id = SQLITE_OK,
        pos_id = 1,
        buf_pos = 0;

    sqlite3_stmt * select_stmt = NULL;

    if (match_clause == NULL || *match_clause == 0)
    {
        select_stmt = SQL_STMT (self, SELECT_MATCHED_ALL);
    }
    else
    {
        select_stmt = SQL_STMT (self, SELECT_MATCHED);
        bind_txt (self, SELECT_MATCHED, pos_id, match_clause, error_id);
        if (error_id != SQLITE_OK)
            g_print ("Select: Error while binding: #%d\n", error_id);
    }

    while ( (error_id = sqlite3_step (select_stmt) ) == SQLITE_ROW &&
            buf_pos < buffer_len)
    {
        int song_idx = sqlite3_column_int (select_stmt, 0);
        song_buffer[buf_pos++] = mc_store_stack_at (self->stack, song_idx - 1);
    }

    if (error_id != SQLITE_DONE && error_id != SQLITE_ROW)
        g_print ("Cannot SELECT: %s\n", sqlite3_errmsg (self->handle) );

    sqlite3_reset (select_stmt);
    sqlite3_clear_bindings (select_stmt);

    return buf_pos;
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
int mc_stprv_load_or_save (mc_StoreDB * self, bool is_save)
{
    int rc;                   /* Function return code */
    sqlite3 *pFile;           /* Database connection opened on zFilename */
    sqlite3_backup *pBackup;  /* Backup object used to copy data */
    sqlite3 *pTo;             /* Database to copy to (pFile or pInMemory) */
    sqlite3 *pFrom;           /* Database to copy from (pFile or pInMemory) */

    /* Open the database file identified by zFilename. Exit early if this fails
     ** for any reason. */
    rc = sqlite3_open (self->db_path, &pFile);
    if (rc == SQLITE_OK)
    {

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
        pBackup = sqlite3_backup_init (pTo, "main", pFrom, "main");
        if ( pBackup )
        {
            sqlite3_backup_step (pBackup, -1);
            sqlite3_backup_finish (pBackup);
        }
        rc = sqlite3_errcode (pTo);
    }

    /* Close the database connection opened on database file zFilename
     ** and return the result of this function. */
    sqlite3_close (pFile);
    return rc;
}

////////////////////////////////

#define feed_tag(tag_enum, sql_col_pos, stmt, song, pair)         \
    pair.value = (char *)sqlite3_column_text(stmt, sql_col_pos);  \
    if (pair.value) {                                             \
        pair.name  = mpd_tag_name(tag_enum);                      \
        mpd_song_feed(song, &pair);                               \
    }                                                             \
     
void mc_stprv_deserialize_songs (mc_StoreDB * self)
{
    /* just assume we're not failing */
    int error_id = SQLITE_OK;

    /* used to convert integer and dates to string */
    char val_buf[64] = {0};
    const int val_buf_size = sizeof (val_buf);

    /* the stmt we will use here, shorter to write.. */
    sqlite3_stmt * stmt = SQL_STMT (self, SELECT_ALL);

    /* a pair (tuple of name/value) that is fed to a mpd_song */
    struct mpd_pair pair;

    /* for date feeding */
    time_t last_modified = 0;
    struct tm * lm_tm = NULL;

    /* range parsing */
    int start = 0, end = 0;

    /* loop over all rows in the songs table */
    while ( (error_id = sqlite3_step (stmt) ) == SQLITE_ROW)
    {
        pair.name = "file";
        pair.value = (char *) sqlite3_column_text (stmt, SQL_COL_URI);

        /* Following fields are understood by libmpdclient:
         * Time (= duration)
         * Range (= start-end)
         * Last-Modified
         * Pos (= here, always 0)
         * Id (= here, always 0)
         *
         * It's also case-sensitive.
         */
        mpd_song * song = mpd_song_begin (&pair);
        if (song == NULL)
            g_print ("Warning: unable to begin parsing of song.\n");

        g_assert (song != NULL);

        /* Parse start/end - this is at least here almost always 0 */
        pair.name = "Range";
        start = sqlite3_column_int (stmt, SQL_COL_START);
        end = sqlite3_column_int (stmt, SQL_COL_END);

        if (!start && !end)
            g_strlcpy (val_buf, "0-0", val_buf_size);
        else
            g_snprintf (val_buf, val_buf_size, "%d-%d", start, end);

        pair.value = val_buf;
        mpd_song_feed (song, &pair);

        /* Since SQLite is completely typeless we can just retrieve the column as string */
        pair.name = "Time";
        pair.value = (char *) sqlite3_column_text (stmt, SQL_COL_DURATION);
        mpd_song_feed (song, &pair);

        /* We parse the date ourself, since we can use a nice static buffer here */
        pair.name = "Last-Modified";
        last_modified = sqlite3_column_int (stmt, SQL_COL_LAST_MODIFIED);
        lm_tm = gmtime (&last_modified);
        strftime (val_buf, val_buf_size, "%Y-%m-%dT%H:%M:%SZ", lm_tm);
        pair.value = val_buf;
        mpd_song_feed (song, &pair);

        /* Now feed the tags */
        feed_tag (MPD_TAG_ARTIST, SQL_COL_ARTIST, stmt, song, pair);
        feed_tag (MPD_TAG_ALBUM, SQL_COL_ALBUM, stmt, song, pair);
        feed_tag (MPD_TAG_TITLE, SQL_COL_TITLE, stmt, song, pair);
        feed_tag (MPD_TAG_ALBUM_ARTIST, SQL_COL_ALBUM_ARTIST, stmt, song, pair);
        feed_tag (MPD_TAG_TRACK, SQL_COL_TRACK, stmt, song, pair);
        feed_tag (MPD_TAG_NAME, SQL_COL_NAME, stmt, song, pair);
        feed_tag (MPD_TAG_GENRE, SQL_COL_GENRE, stmt, song, pair);
        feed_tag (MPD_TAG_COMPOSER, SQL_COL_COMPOSER, stmt, song, pair);
        feed_tag (MPD_TAG_PERFORMER, SQL_COL_PERFORMER, stmt, song, pair);
        feed_tag (MPD_TAG_COMMENT, SQL_COL_COMMENT, stmt, song, pair);
        feed_tag (MPD_TAG_DISC, SQL_COL_DISC, stmt, song, pair);
        feed_tag (MPD_TAG_MUSICBRAINZ_ARTISTID, SQL_COL_MUSICBRAINZ_ARTIST_ID, stmt, song, pair);
        feed_tag (MPD_TAG_MUSICBRAINZ_ALBUMID, SQL_COL_MUSICBRAINZ_ALBUM_ID, stmt, song, pair);
        feed_tag (MPD_TAG_MUSICBRAINZ_ALBUMARTISTID, SQL_COL_MUSICBRAINZ_ALBUMARTIST_ID, stmt, song, pair);
        feed_tag (MPD_TAG_MUSICBRAINZ_TRACKID, SQL_COL_MUSICBRAINZ_TRACK_ID, stmt, song, pair);

        mc_store_stack_append (self->stack, song);
    }

    if (error_id != SQLITE_DONE)
    {
        g_print ("Cannot load songs from the database: %s\n", sqlite3_errmsg (self->handle) );
    }
}

////////////////////////////////

#define select_meta_attribute(self, meta_enum, column_func, out_var, copy_func, cast_type)       \
    {                                                                                                \
        int error_id = SQLITE_OK;                                                                    \
        if ( (error_id = sqlite3_step(SQL_STMT(self, meta_enum))) == SQLITE_ROW)                     \
            out_var = (cast_type)column_func(SQL_STMT(self, meta_enum), 0);                         \
        if (error_id != SQLITE_DONE && error_id != SQLITE_ROW)                                       \
            g_print ("Cannot SELECT META: (Error #%d) %s\n", error_id, sqlite3_errmsg (self->handle) );  \
        out_var = (cast_type)copy_func((cast_type)out_var);                                          \
        sqlite3_reset (SQL_STMT (self, meta_enum));                                                  \
    }                                                                                                \
     

////////////////////////////////

int mc_stprv_get_song_count (mc_StoreDB * self)
{
    int song_count = -1;
    select_meta_attribute (self, COUNT, sqlite3_column_int, song_count, ABS, int);
    return song_count;
}

////////////////////////////////

int mc_stprv_get_db_version (mc_StoreDB * self)
{
    int db_version = 0;
    select_meta_attribute (self, SELECT_META_DB_VERSION, sqlite3_column_int, db_version, ABS, int);
    return db_version;
}

////////////////////////////////

int mc_stprv_get_pl_version (mc_StoreDB * self)
{
    int pl_version = 0;
    select_meta_attribute (self, SELECT_META_PL_VERSION, sqlite3_column_int, pl_version, ABS, int);
    return pl_version;
}

////////////////////////////////

int mc_stprv_get_sc_version (mc_StoreDB * self)
{
    int sc_version = 0;
    select_meta_attribute (self, SELECT_META_SC_VERSION, sqlite3_column_int, sc_version, ABS, int);
    return sc_version;
}

////////////////////////////////

int mc_stprv_get_mpd_port (mc_StoreDB * self)
{
    int mpd_port = 0;
    select_meta_attribute (self, SELECT_META_MPD_PORT, sqlite3_column_int, mpd_port, ABS, int);
    return mpd_port;
}

////////////////////////////////

char * mc_stprv_get_mpd_host (mc_StoreDB * self)
{
    char * mpd_host = NULL;
    select_meta_attribute (self, SELECT_META_MPD_HOST, sqlite3_column_text, mpd_host, g_strdup, char *);
    return mpd_host;
}
