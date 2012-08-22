#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <mpd/client.h>

////////////////////////////////

/*
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
int loadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave){
  int rc;                   /* Function return code */
  sqlite3 *pFile;           /* Database connection opened on zFilename */
  sqlite3_backup *pBackup;  /* Backup object used to copy data */
  sqlite3 *pTo;             /* Database to copy to (pFile or pInMemory) */
  sqlite3 *pFrom;           /* Database to copy from (pFile or pInMemory) */

  /* Open the database file identified by zFilename. Exit early if this fails
  ** for any reason. */
  rc = sqlite3_open(zFilename, &pFile);
  if( rc==SQLITE_OK ){

    /* If this is a 'load' operation (isSave==0), then data is copied
    ** from the database file just opened to database pInMemory. 
    ** Otherwise, if this is a 'save' operation (isSave==1), then data
    ** is copied from pInMemory to pFile.  Set the variables pFrom and
    ** pTo accordingly. */
    pFrom = (isSave ? pInMemory : pFile);
    pTo   = (isSave ? pFile     : pInMemory);

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
    if( pBackup ){
      sqlite3_backup_step(pBackup, -1);
      sqlite3_backup_finish(pBackup);
    }
    rc = sqlite3_errcode(pTo);
  }

  /* Close the database connection opened on database file zFilename
  ** and return the result of this function. */
  sqlite3_close(pFile);
  return rc;
}

////////////////////////////////

typedef struct
{
    sqlite3 * db;
    sqlite3_stmt * insert_stmt;
} Store_DB;

////////////////////////////////

enum
{
    SQL_CREATE,
    SQL_INSERT,
    SQL_SELECT
};

static const char * sql_stmts[] = {
    [SQL_CREATE] =
        "CREATE TABLE IF NOT EXISTS songs(\n"
        "idx            INTEGER PRIMARY KEY,      -- Index in metalist\n"
        "uri            TEXT UNIQUE NOT NULL,     -- Path to file, or URL to webradio etc.\n"
        "id             INTEGER NOT NULL,         -- Song ID in the Database\n"
        "pos            INTEGER NOT NULL,         -- Position in the Queue\n"
        "start          INTEGER NOT NULL,         -- Start of the virtual song within the physical file\n"
        "end            INTEGER NOT NULL,         -- End of the virtual song within the physical file\n"
        "duration       INTEGER NOT NULL,         -- Songudration in Seconds. (0 means unknown)\n"
        "last_modified  INTEGER NOT NULL,         -- Last modified as Posix UTC Timestamp\n"
        "-- Tags:\n"
        "artist         TEXT,\n"
        "album          TEXT,\n"
        "title          TEXT,\n"
        "album_artist   TEXT,\n"
        "track          TEXT,\n"
        "name           TEXT,\n"
        "genre          TEXT,\n"
        "composer       TEXT,\n"
        "performer      TEXT,\n"
        "comment        TEXT,\n"
        "disc           TEXT,\n"
        "-- MB-Tags:\n"
        "musicbrainz_artist_id       TEXT,\n"
        "musicbrainz_album_id        TEXT,\n"
        "musicbrainz_albumartist_id  TEXT,\n"
        "musicbrainz_track           TEXT\n"
        ");\n"
        "-- Create indices for tags that are likely to be searched through.\n"
        "--CREATE INDEX IF NOT EXISTS artist_index ON songs(artist);\n"
        "--CREATE INDEX IF NOT EXISTS album_index ON songs(album);\n"
        "--CREATE INDEX IF NOT EXISTS title_index ON songs(title);\n"
        "--CREATE INDEX IF NOT EXISTS genre_index ON songs(genre);\n"
        "\n"
        "-- Combined index, for the default mode.\n"
        "--CREATE INDEX IF NOT EXISTS artists_album_title_index ON songs(artist, album, title);\n"
        "\n"
        "-- Meta Table, containing information about the metadata itself.\n"
        "CREATE TABLE IF NOT EXISTS meta(\n"
        "      db_version    INTEGER,  -- db version from statistics\n"
        "      pl_version    INTEGER,  -- pl version from status\n"
        "      sc_version    INTEGER   -- schema version\n"
        ");\n",
    [SQL_INSERT] = 
        "INSERT INTO songs VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
    [SQL_SELECT] = 
        "",
};

//////////////////////////////

static void open_connection(sqlite3 ** memDB)
{
    int rc = sqlite3_open(":memory:", memDB);
    if (rc != SQLITE_OK)
    {
        printf("Cannot open db: %s\n", sqlite3_errmsg(*memDB));
        *memDB = NULL;
    }
}

///////////////////////////////

static int idx = 0;

static bool insert_song(Store_DB * db, struct mpd_song * song)
{
    sqlite3_bind_int(db->insert_stmt, 1, idx++);
    sqlite3_bind_text(db->insert_stmt, 2, mpd_song_get_uri(song), -1, NULL);
    sqlite3_bind_int(db->insert_stmt, 3, mpd_song_get_id(song));
    sqlite3_bind_int(db->insert_stmt, 4, mpd_song_get_pos(song));
    sqlite3_bind_int(db->insert_stmt, 5, mpd_song_get_start(song));
    sqlite3_bind_int(db->insert_stmt, 6, mpd_song_get_end(song));
    sqlite3_bind_int(db->insert_stmt, 7, mpd_song_get_duration(song));
    sqlite3_bind_int(db->insert_stmt, 8, mpd_song_get_last_modified(song));

    sqlite3_bind_text(db->insert_stmt, 9, mpd_song_get_tag(song, MPD_TAG_ARTIST, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 10, mpd_song_get_tag(song, MPD_TAG_ALBUM, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 11, mpd_song_get_tag(song, MPD_TAG_TITLE, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 12, mpd_song_get_tag(song, MPD_TAG_ALBUM_ARTIST, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 13, mpd_song_get_tag(song, MPD_TAG_TRACK, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 14, mpd_song_get_tag(song, MPD_TAG_NAME, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 15, mpd_song_get_tag(song, MPD_TAG_GENRE, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 16, mpd_song_get_tag(song, MPD_TAG_COMPOSER, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 17, mpd_song_get_tag(song, MPD_TAG_PERFORMER, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 18, mpd_song_get_tag(song, MPD_TAG_COMMENT, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 19, mpd_song_get_tag(song, MPD_TAG_DISC, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 20, mpd_song_get_tag(song, MPD_TAG_MUSICBRAINZ_ARTISTID, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 21, mpd_song_get_tag(song, MPD_TAG_MUSICBRAINZ_ALBUMID, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 22, mpd_song_get_tag(song, MPD_TAG_MUSICBRAINZ_ALBUMARTISTID, 0), -1, NULL);
    sqlite3_bind_text(db->insert_stmt, 23, mpd_song_get_tag(song, MPD_TAG_MUSICBRAINZ_TRACKID, 0), -1, NULL);

    int err;
    while ( (err = sqlite3_step(db->insert_stmt)) == SQLITE_BUSY);

    sqlite3_reset(db->insert_stmt);
    sqlite3_clear_bindings(db->insert_stmt);

    if (err != SQLITE_DONE)
    {
        g_print("Cannot INSERT: %s\n", sqlite3_errmsg(db->db));
        return false;
    }
    return true;
}

///////////////////////////////

Store_DB * create_store(void)
{
    Store_DB * store = g_new0(Store_DB, 1);

    /* Make initial connection */
    store->db = NULL;
    open_connection(&store->db);
    if (store->db == NULL)
    {
        g_free(store);
        return NULL;
    }

    sqlite3_exec(store->db, sql_stmts[SQL_CREATE], NULL, NULL, NULL);

    /* Prepatre insert statement */
    store->insert_stmt = NULL;
    int prep_err = sqlite3_prepare_v2(
            store->db, 
            sql_stmts[SQL_INSERT],
            -1,
            &store->insert_stmt,
            NULL);

    if(prep_err != SQLITE_OK)
    {
        g_print("cannot prepare statement: %s\n",
                sqlite3_errmsg(store->db));
    }

    return store;
}

///////////////////////////////

int main(int argc, char const *argv[])
{
    Store_DB * store = create_store();

    if (store != NULL)
    {
        struct mpd_connection * con = mpd_connection_new("localhost", 6600, 30 * 1000);
        if (con != NULL)
        {
            mpd_send_list_all_meta(con, "/");
            struct mpd_entity * ent = NULL;
            const struct mpd_song * song = NULL;

            g_print("Sended!\n");

            sqlite3_exec(store->db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);

            while ( (ent = mpd_recv_entity(con)) != NULL)
            {
                if (mpd_entity_get_type(ent) == MPD_ENTITY_TYPE_SONG)
                {
                    song = mpd_entity_get_song(ent);
                    insert_song(store, (struct mpd_song *)song);
                }
                mpd_entity_free(ent);
            }

            sqlite3_exec(store->db, "COMMIT;", NULL, NULL, NULL);

            enum mpd_error err = 0;
            if ( (err = mpd_connection_get_error(con)) != MPD_ERROR_SUCCESS)
            {
                g_print("%d: %s\n", err, mpd_connection_get_error_message(con));
                mpd_connection_clear_error(con);
            }
        }
    }

    loadOrSaveDb(store->db, "out.db", true);
    return EXIT_SUCCESS;
}
