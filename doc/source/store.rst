Songstore
=========

Overview
--------

The Store is used to save all queried mpd songs into a Database.

Advantages
----------

- Saves network performance.
- Easier/more intelligent searching (via SQL statements or FTS)
- Faster Startup (songs can be deserialized from DB, which is faster than slow network)

Disadvantage
------------

- More work for us, but at least libgda makes implementing different backends easy.
- More Memory Usage, partly, also more CPU Usage (although not much)
  => Network is more important anyway. 100 MB of Memory should be tolerable for ~16000 Songs.


Architecture
------------

The Database is hold in memory, and written to HD on application quit.
For reducing memory, tags (title, album, artist) are stored in seperate tables.
A list of mpd_song is held internally (probably a GArray). 

.. _initialization:

Everything below is crap.

**Initialization:**

  #. From one row, a mpd_song can be deserialized since all metadata of a mpd_song is stored (tags, length etc.).
  #. Additional to the metadata of songs, a index is saved.
  #. The Queue-Version is saved in a seperate table ("meta") and may be compared later.

  .. code-block:: sql

      CREATE TABLE IF NOT EXISTS songs(
          index          INTEGER PRIMARY KEY,      -- Index in metalist
          uri            TEXT UNIQUE NOT NULL,     -- Path to file, or URL to webradio etc.
          id             INTEGER UNIQUE NOT NULL,  -- Song ID in the Database
          pos            INTEGER NOT NULL,         -- Position in the Queue
          start          INTEGER NOT NULL,         -- Start of the virtual song within the physical file
          end            INTEGER NOT NULL,         -- End of the virtual song within the physical file
          duration       INTEGER NOT NULL,         -- Songudration in Seconds. (0 means unknown)
          last_modified  INTEGER NOT NULL,         -- Last modified as Posix UTC Timestamp
  
          -- Tags:
          artist         TEXT,
          album          TEXT,
          title          TEXT,
          album_artist   TEXT,
          track          TEXT,
          name           TEXT,
          genre          TEXT,
          composer       TEXT,
          performer      TEXT,
          comment        TEXT,
          disc           TEXT,

          -- MB-Tags:
          musicbrainz_artist_id       TEXT,
          musicbrainz_album_id        TEXT,
          musicbrainz_albumartist_id  TEXT,
          musicbrainz_track           TEXT
          ... 
      );

    -- Create indices for tags that are likely to be searched through.
    CREATE INDEX IF NOT EXISTS artist_index ON songs(artist);
    CREATE INDEX IF NOT EXISTS album_index ON songs(album);
    CREATE INDEX IF NOT EXISTS title_index ON songs(title);
    CREATE INDEX IF NOT EXISTS genre_index ON songs(genre);

    CREATE UNIQUE INDEX IF NOT EXISTS artists_album_title_index ON songs(artist, album, title);

    CREATE TABLE IF NOT EXISTS meta(
          db_version    INTEGER,  -- db version from statistics
          pl_version    INTEGER,  -- pl version from status
          sc_version    INTEGER   -- schema version
    );

**Searching**:

  .. code-block :: sql

     -- WHERE clause needs to be dynamically built once
     -- can be used as prepared statemtn to fill in values afterwards
     SELECT index FROM songs 
     WHERE
       artist = ?,
       album  = ?
       ...
     ;

**Inserting**:

  .. code-block :: sql

     BEGIN IMMEDIATE;
     INSERT INTO songs VALUES(
        ...
     );

     -- Use of precompiled INSERT possible!
     -- Repeat insert for each song.
     COMMIT;

**Updating**:

  .. code-block :: sql

     -- On DB_UPDATE event:
     -- if statistics.db_version == meta.db_version:
     --    pass # Nothing needs to be done
     -- else:
     --    ex("DELETE FROM TABLE songs;")
     --    ex("DROP INDEX...;")
     --    re_initialize()
    
**Shutdown:**

  * Save in-memory db to disk using the backup API.
