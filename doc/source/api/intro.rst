Introduction
============

Short Ovierview of all Classes
------------------------------

.. digraph:: foo

   node [shape=record];
   top  [label="<client>Client" center=true];
   mid  [label="<store>Store | AudioOutput | { Status | Song | Statistics}"]

   top -> mid [label=" offers as members" arrowType="diamond"]

   subgraph { 
    rank = same; Playlist; StoredPlaylist
   }
   
   mid:store -> Playlist [label=" produces"]
   mid:store -> StoredPlaylist
   StoredPlaylist -> Playlist [label=" inherits"]


Quickstart
----------

Reading this should get you already a basic overview of the classes presented
here. Details are referenced, just click on them. 

.. currentmodule:: moosecat.core

* On top of everything there is the :class:`.Client` object.
  It will be the first object you instantiate and the last you will cleanup.
* The :class:`.Client` has members that give you more power.
  These are, as shown above:

    * :class:`.Store`: Store songs and make them searchable.  
    * :class:`.AudioOutput`: Gives info and control over one of MPD's Outputs.
    * :class:`.Status`: Represents current metadata.
    * :class:`.Song`: A single song. The Client offers a
      ``currentsong`` attribute.
    * :class:`.Statistics`

* The most powerful of those is the :class:`.Store` object.
  It offers you access to MPD's Queue, Database and Stored Playlists.
* MPD Clients are mostly signal driven, therefore there one can register signals
  easily with the :py:func:`.Client.signal_add` function. Alternatively you can
  use the more pythonish decorator:: 
  
    >>> client = moosecat.core.Client()
    >>> @client.signal('client-event')
    >>> def event_handler(client, event):
    ...     if event & Idle.PLAYER:
    ...         print('You messed with the Player State!')

Rules for writing Plugins
-------------------------

Ask yourself…

    1) …does the plugin exist already?
    2) …would it better to make the plugin a standalone program?
    3) …should the feature be in a Music Player? (A clock for example is *not*)
