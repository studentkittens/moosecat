CREATE TABLE IF NOT EXISTS songs(
      idx            INTEGER PRIMARY KEY,      -- Index in metalist
      uri            TEXT UNIQUE NOT NULL,     -- Path to file, or URL to webradio etc.
      id             INTEGER NOT NULL,  -- Song ID in the Database
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
);

-- Create indices for tags that are likely to be searched through.
CREATE INDEX IF NOT EXISTS artist_index ON songs(artist);
CREATE INDEX IF NOT EXISTS album_index ON songs(album);
CREATE INDEX IF NOT EXISTS title_index ON songs(title);
CREATE INDEX IF NOT EXISTS genre_index ON songs(genre);

-- Combined index, for the default mode.
--CREATE UNIQUE INDEX IF NOT EXISTS artists_album_title_index ON songs(artist, album, title);

-- Meta Table, containing information about the metadata itself.
CREATE TABLE IF NOT EXISTS meta(
      db_version    INTEGER,  -- db version from statistics
      pl_version    INTEGER,  -- pl version from status
      sc_version    INTEGER   -- schema version
);
