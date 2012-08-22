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

  .. literalinclude:: sql/create.sql
     :language: sql

**Searching**:

  .. literalinclude:: sql/search.sql
     :language: sql

**Inserting**:

  .. literalinclude:: sql/insert.sql
     :language: sql

**Updating**:

  .. literalinclude:: sql/update.sql
     :language: sql

**Shutdown:**

  Save in-memory db to disk using `SQLite's backup API <http://www.sqlite.org/backup.html>`_.
