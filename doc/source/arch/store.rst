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

TODO
