Overview
========

The Store is used to save all queried mpd songs into a Database.
Advantages:

- Saves network performance.
- Easier/more intelligent searching (via SQL statements or FTS)
- Faster Startup (songs can be deserialized from DB, which is faster than slow network)

Disadvantage:

- Bit more work for us, libgda makes this bearable though.
- More Memory Usage, partly, also more CPU Usage (although not much)
  => Network is more important anyway. 100 MB of Memory should be tolerable for ~16000 Songs.


Architecture
============

- The Database is hold in memory, and written to HD on application quit.

  * All metadata of a mpd_song is stored (tags, length etc.).
  * from one row, a mpd_song can be deserialized.
  * for reducing memory, tags (title, album, artist) are stored in seperate tables.
  * Additional to the metadata of songs, a index is saved.
  * The Queue-Version is saved persistent also.

- A list of mpd_song is held internally (probably a GArray). 
  
  * The Database is used for indexing, and searching. 
  * All Selects return indices to the songlist, which can be used to reference certain songs.

- Startup: 

  * Read last Queue-Version.
  * Read Difference to this version from server (plchanges command)
  * Merge differences to DB.
  * De-serialize complete songlist from DB.

- Searching:

  * Do a select, and return a list of pointers. Done.

- Shutdown:

  * Save in-memory db to disk.
