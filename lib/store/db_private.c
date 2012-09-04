#include "db_private.h"

/* strlen() */
#include <string.h>

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

static const char * _sql_stmts[] =
{
    [_MC_SQL_CREATE] =
        "-- Note: Type information is not parsed at all, and rather meant as hint for the developer.    \n"
        "CREATE VIRTUAL TABLE IF NOT EXISTS songs using fts3(                                           \n"
        "uri            TEXT UNIQUE NOT NULL,     -- Path to file, or URL to webradio etc.              \n"
        "id             INTEGER NOT NULL,         -- Song ID in the Database                            \n"
        "pos            INTEGER NOT NULL,         -- Position in the Queue                              \n"
        "start          INTEGER NOT NULL,         -- Start of the virtual song within the physical file \n"
        "end            INTEGER NOT NULL,         -- End of the virtual song within the physical file   \n"
        "duration       INTEGER NOT NULL,         -- Songudration in Seconds. (0 means unknown)         \n"
        "last_modified  INTEGER NOT NULL,         -- Last modified as Posix UTC Timestamp               \n"
        ///////////////////////////////////////////////////////////////////////////////////////////////////
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
        ///////////////////////////////////////////////////////////////////////////////////////////////////
        "-- FTS options:                                                                                \n"
        "tokenize=porter -- todo: make this interchangeable.                                            \n"                     
        ");                                                                                             \n"
        ///////////////////////////////////////////////////////////////////////////////////////////////////
        "-- Meta Table, containing information about the metadata itself.                               \n"
        "CREATE TABLE IF NOT EXISTS meta(                                                               \n"
        "      db_version    INTEGER,  -- db version from statistics                                    \n"
        "      pl_version    INTEGER,  -- pl version from status                                        \n"
        "      sc_version    INTEGER   -- schema version                                                \n"
        ");                                                                                             \n",
    [_MC_SQL_INSERT] =
        "INSERT INTO songs VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
    [_MC_SQL_SELECT] =
        "SELECT rowid FROM songs WHERE artist MATCH ?;",
    [_MC_SQL_BEGIN] =
        "BEGIN IMMEDIATE;",
    [_MC_SQL_COMMIT] = 
        "COMMIT;"
};


///////////////////

#define SQL_STMT(STORE, NAME) \
    STORE->sql_prep_stmts[_MC_SQL_##NAME]

////////////////////////////////

void mc_stprv_prepare_all_statements (mc_StoreDB * self)
{
    int prep_error = 0;

    // TODO
    sqlite3_exec(self->handle, _sql_stmts[_MC_SQL_CREATE], NULL, NULL, NULL);
        _sql_stmts[_MC_SQL_CREATE] = NULL;

    for (int i = 1; i < _MC_SQL_SOURCE_COUNT; i++)
    {
        prep_error = sqlite3_prepare_v2 (
                self->handle,
                _sql_stmts[i],
                -1,
                &self->sql_prep_stmts[i],
                NULL);

        /* Uh-Oh. Typo? */
        if (prep_error != SQLITE_OK)
            g_print ("WARNING: Cannot prepare statement #%d: %s\n",
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
    if ( sqlite3_step (SQL_STMT(self, BEGIN)) != SQLITE_DONE)
        g_print ("Unable to execute BEGIN\n");
}


////////////////////////////////

void mc_stprv_commit (mc_StoreDB * self)
{
    if ( sqlite3_step (SQL_STMT(self, COMMIT)) != SQLITE_DONE)
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
    bind_int (db, INSERT, pos_idx, mpd_song_get_id(song), error_id);
    bind_int (db, INSERT, pos_idx, mpd_song_get_pos(song), error_id);
    bind_int (db, INSERT, pos_idx, mpd_song_get_start(song), error_id);
    bind_int (db, INSERT, pos_idx, mpd_song_get_end(song), error_id);
    bind_int (db, INSERT, pos_idx, mpd_song_get_duration(song), error_id);
    bind_int (db, INSERT, pos_idx, mpd_song_get_last_modified(song), error_id);

    /* bind tags */
    bind_tag (db, INSERT,pos_idx, song, MPD_TAG_ARTIST, error_id);
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
        g_print ("Insert: Error while binding: #%d\n", error_id);

    /* evaluate prepared statement, only check for errors */
    while ( (error_id = sqlite3_step (SQL_STMT(db, INSERT)) ) == SQLITE_BUSY)
        /* Nothing */;

    /* Make sure bindings are ready for the next insert */
    sqlite3_reset (SQL_STMT (db, INSERT));
    sqlite3_clear_bindings (SQL_STMT (db, INSERT));

    if((rc = (error_id != SQLITE_DONE)) == true)
        g_print ("Cannot INSERT into :memory: : %s\n", sqlite3_errmsg (db->handle) );

    return rc;
}

////////////////////////////////

int mc_stprv_select_out (mc_StoreDB * self, const char * match_clause, mpd_song ** song_buffer, int buffer_len)
{
    int error_id = SQLITE_OK,
        pos_id = 1,
        buf_pos = 0;

    bind_txt (self, SELECT, pos_id, match_clause, error_id);

    if (error_id != SQLITE_OK)
        g_print ("Select: Error while binding: #%d\n", error_id);

    while ( (error_id = sqlite3_step (SQL_STMT(self, SELECT)) ) == SQLITE_ROW &&
            buf_pos < buffer_len)
    {
        int song_idx = sqlite3_column_int(SQL_STMT(self, SELECT), 0);
        song_buffer[buf_pos++] = mc_store_stack_at (self->stack, song_idx - 1);
    }

    if (error_id != SQLITE_DONE && error_id != SQLITE_ROW)
        g_print ("Cannot SELECT: %s\n", sqlite3_errmsg (self->handle) );

    sqlite3_reset (SQL_STMT (self, SELECT));
    sqlite3_clear_bindings (SQL_STMT (self, SELECT));

    return buf_pos;
}

////////////////////////////////

mpd_song ** mc_stprv_select (mc_StoreDB * self, const char * match_clause)
{
    // TODO
    (void ) self;
    (void ) match_clause;
    return NULL;   
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
