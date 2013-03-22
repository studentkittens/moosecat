Plugin System
=============

Important Functions
-------------------

.. currentmodule:: moosecat.plugin_system

.. autosummary::

    PluginSystem.category
    PluginSystem.first

Moosecat's Plugin System is based on the excellent  Yapsy_ Framework.

Details
-------

The Plugin System (PS) is *Category*-based. This means moosecat provides a set
of categories that may be used by the core, a frontend or also other plugins.
Plugins are then accessed by their category and, also, a priority.

.. digraph:: foo

    node [shape=record];

    "moosecat.plugins.xyz.ActualPluginOne" -> "moosecat.plugins.ISomeConcretePlugin"
    "moosecat.plugins.xyz.ActualPluginTwo" -> "moosecat.plugins.ISomeConcretePlugin"
    "moosecat.plugins.ISomeConcretePlugin" -> "moosecat.plugins.IBaseplugin" -> "yapsy.IPlugin"

Plugin User Example: ::

    from moosecat.boot import g

    for plugin in g.psys.category('NetworkProvider'):
        result = plugin.find()
        if result is not None:
            host, port = result
            print('%s:%s' % host, port)


Internal Plugins
----------------

Just place your plugin somewhere in ``moosecat/plugins`` (feel free to make
subdirectories) and fill in something like the following: ::

    # moosecat/plugins/networkprovider/mindreader.py
    from moosecat.plugins import INetworkProvider

    class MindReader(INetworkProvider):
        def fetch(self):
            'INetworkProvider wants fetch to return the host and port as tuple'
            return ('raspberry.pi', 6666)

        def priority(self):
            return 70  # higher (max. 100) is more important, will be queried first.

Apart from that, you will have to place a file called ``mindreader.plugin``
there, containing for example: ::

    [Core]
    Name = MindReaderNetworkProvider          # Can be anything. Used for Display.
    Module = mindreader                       # Actual modules' name

    [Documentation]
    Author = G. W. Bush
    Version = 0.123alpha
    Description = Reads your mind and finds out which server you want to connect to.
    

Every code in moosecat that relies on :class:`moosecat.plugins.INetworkProvider`
will now be able to use your plugin. 

External Plugins
----------------

**TODO 1:** Write a Tutorial for local paths. 

**TODO 2:** Write a Tutorial for Python Eggs.

Reference
---------


.. autoclass:: moosecat.plugin_system.PluginSystem
    :members:

.. _Yapsy: http://yapsy.sourceforge.net/
