Developers, Developers, Developers, Developers ...
==================================================

This is internal documentation describing the internal architecture in moosecat.
Many things here may sound familiar for you, but if you want to develop on moosecat
it's advisable to read through this.

Introduction
------------

Moosecat is split in several layers which are from bottom up:

``libmoosecat.so``:

    A C-Library that implements the very basic things, that need to be done fast.
    Apart from handling the connection to the MPD Server it is responsible for storing 
    queried songs into a database, on top of which a simple search mechanism is implemented.
    
    The MPD responsibilites include:

    - Connecting (more than connection is possible)
    - Listening to Events (volume changes, playback changes)
    - Sending/Implementing Commands and watching out for Errors. 
    - Notifying registered subclients on events, errors and connection changes.
    - Keeping data updated: Status, Current Song and Statistics.


    It's the first thing that SCons will build.
    
    .. seealso :: :ref:`libmoosecat`

``moosecython``, also commonly called ``core``:

    A Cython Wrapper around ``libmoosecat.so`` that also implements the other part of the core.
    It's purpose is to make the C-API more Pythonic to use, and to implement tasks like:

        - Access to config files.
        - Maintaining logs and filesystem stuff.
        - Providing a pretty advanced pluginsystem.
        - Application's entry point & Initialization.
    
    .. seealso :: :ref:`moosecython`

``catelitte``: 

    This layer is a collective term for all plugins registered on ``moosecython``. They implement the
    actual functionality seen by the user. Typical plugins would be:

        - GTK UI (or ncurses/kyvi)
        - Avahi server detection.
        - Dynamic playlist generation.
        - ...

    Plugins are a bit special, since they can talk to each other. Therefore we don't call them Plugins but ``Catellites``.
    Derived from ``Satellites`` and ``Cats`` (We like cats.), Catellites can talk to eatch other, over a common 
    base station (The Earth vs. ``moosecython``). They can discover each other and user their functionality via
    so-called ``tags``. A Tag is a interface a Catellite can implement (A Catellite may implement 1..n tags).
    An example tag would be ``gtk_browser``, which would tell us that this Catellite implements a gkt-view that
    can be shown in the sidebar of the GTK-UI. The ``metadata`` tag would require a Catellite to implement 
    certain methods like ``fetch(type, song)``. This way several Catellites can implement a metadata-fetching system.

    .. seealso :: :ref:`plugin_design`

    


