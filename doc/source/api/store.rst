moosecat.core.Store
====================

The Store is used to manage all songs queried from MPD. Internally every song
only exists once - although you can have more than one Song object wrapping it.

The Store saves the following:

    * Contents of the Database
    * Contents of the Queue (subset of Database)
    * Contents of all Stored Playlist (that are marked as loaded)


Important Functions
-------------------

.. currentmodule:: moosecat.core

* Queue, Database:

    .. autosummary::

        Store.query

* Stored Playlists:

    .. autosummary::

        Store.stored_playlist_load
        Store.stored_playlist_query
        Store.stored_playlist_names

**Accessing the Store:**

    .. autosummary::

        Client.store
        Client.store_initialize

Example
-------

Full example to get a working Store: :: 

    >>> import moosecat.core as m
    >>> client = m.Client()
    >>> client.connect(host='localhost', port=6600)
    >>> client.store_initialize('/tmp')
    >>> store = client.store

Reference
---------

.. autoclass:: moosecat.core.Store
   :members:
