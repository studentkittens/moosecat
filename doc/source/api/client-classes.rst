Classes supplementing moosecat.core.Client
==========================================

There are some classes that are used alongside of :class:`moosecat.core.Client`:

    * :class:`moosecat.core.Status`
    * :class:`moosecat.core.Song`
    * :class:`moosecat.core.Statistics`
    * :class:`moosecat.core.AudioOutput`
    * :class:`moosecat.core.Playlist`

        * :class:`moosecat.core.StoredPlaylist`

They are mostly used for information retrieval (like :class:`.Song`), but can be also used
to talk to MPD (like :class:`.Status`, :class:`.StoredPlaylist` and :class:`.AudioOutput`).

**Examplary functions/properties:**

.. currentmodule:: moosecat.core

.. autosummary::

    Status.random   
    Statistics.number_of_albums
    Song.artist


Examples
--------

:class:`moosecat.core.Status` : ::

    >>> client.status.random
    False
    >>> client.status.random = True
    >>> client.status.random
    False  # be prepared for this
    >>> client.force_update_metadata()
    >>> client.status.random
    True # this will be done implicitly if you let the mainloop spin

You might argue that this might be a weird behaviour for a property, but it
won't be not less confusing if it would be a function. Therefore settable
properties were used to be consistent with other classes like mentioned here.


:class:`moosecat.core.Song` : ::

    >>> client.currentsong.artist
    'Nachtgeschrei'  # there is nothing surprising here.

:class:`moosecat.core.Statistics` : ::

    >>> client.statistics.number_of_songs
    34705  # no surprise hidden either.
 
:class:`moosecat.core.AudioOutput` : ::

    >>> print('Enable all the Outputs!')
    >>> for output in client.outputs:
    ...     print(output.id, output.enabled, output.name, sep='\t')
    ...     output.enabled = True
    ...
    0   True    AlsaOutput
    1   False   HttpOutput

:class:`moosecat.core.Playlist` :

A playlist is a container for a set of songs. 

::

    >>> # Some Init required
    >>> import moosecat.core as m 
    >>> c = m.Client()
    >>> c.connect()
    >>> c.store_initialize('/tmp')
    >>> c.store.wait()
    >>> # Init done! 
    >>> full_queue = client.store.query()
    >>> full_queue
    <moosecat.core.moose.Playlist object at 0x7f49fe163140>
    >>> full_queue.name
    'queue'
    >>> full_queue.last_modified
    0  # unknown, not a stored playlist.
    >>> len(full_queue)
    20477
    >>> list(full_queue)
    [<moosecat.core.moose.Song object at 0x7f49fe029310>, ...]
    >>> full_queue[500].artist
    'Alestorm'

:class:`moosecat.core.StoredPlaylist`: ::

    ...

Reference
---------

.. autoclass:: moosecat.core.Status
   :members:

-----

.. autoclass:: moosecat.core.Song
   :members:

-----

.. autoclass:: moosecat.core.Statistics
   :members:

-----

.. autoclass:: moosecat.core.AudioOutput
   :members:

-----

.. autoclass:: moosecat.core.Playlist
   :members:

-----

.. autoclass:: moosecat.core.StoredPlaylist
   :members:
