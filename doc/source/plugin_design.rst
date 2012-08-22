.. _plugin_design:

Catellites (the Plugin System)
==============================

A Catellite is basically a plugin. So why inventing a new name for this?
Well, normally plugins can't talk to each other, catellites can. 
So you don't get confused: We simply use ``catellite`` and ``plugin`` interchangeable.

.. _mdict_def:

Registering Procedure
---------------------

* Catellite registers on core with:

  - Name of the catellite.
  - List of implemnted tags.
  - Version (Numeric, Version-Scheme provided)
  - Priority (which plugin to try first)

* Core checks if all tags are implemented (via ``hasattr()``)
* Core load the catellite-module object via ``__import__(name)``
* Core puts the module object into a dictionary: ::

    {
        'metadata': {
            'glyr': module_object1,
            'abcd': module_object2
        },
        'gtk_browser': {
            # ...
        }
    }


Accessing methods from other Catellites
---------------------------------------

* Howto access foreign functions ::

   # Explicit version
   # m is the modulelist above.
   core.m['metadata'][0].find_my_metadata(...)

   # Possible shortcut:is a reference to the first module in the list. 
   # mf is a reference to the first module in the list.
   core.mf['metadata'].find_my_metadata(...)

   # Using more than one function:
   # But note: This is not recomned, since in theory a catellite 
   # can be unloaded any time and be replaced by a dummy module,
   # which won't work with this approach.
   # Use this only for atomar operations only.
   #
   # comment: core should try to make this not multithreadded,
   #          so, module undload can only happen synchronously.
   md = core.mf['metadata']
   md.find_my_metadata(...)
   md.make_me_pretty(...)

* Possible thought: A plugin that does not respond anymore to a 'does-still-work' anymore call,
  will be replaced by a dummy object that implementes the specified tags and returns only None.
  But: Hard to implement, will be broken till restart.


* Catellite ran register own tags to the core. Possible usecase would be the GTK-UI,
  that can intoduce custom tags like ``gtk_browser``, that extend itself.
  So, the core only needs to provide some basic tags, and can be dynamically extended.

.. _catellite_api:

Catellite API Proposal
----------------------

.. py:module :: catellite

.. py:function:: register(name, tags, version, priority, description='')

    Register a plugin in the core.

    :param name: The name of the plugins for displaying purpose.
    :param tags: A list of tags that this plugin implements.
    :param version: A version string. Should be consist of "Major.Minor.Micro"
    :param priority: A number between 0-100, 100 is the hightest priority.
    :param description: An optional description for displaying purpose.
    :returns: True if the plugin could be registered.

.. py:function:: unload(name)

    Unload a plugin and internally replace it with a dummy module.
    Does nothing if the module does not exist.

    :param name: the name of the plugin.
    :returns: True if the plugin was found and unloaded.

.. py:function:: add_tag(tag_name, definition)

    :param tag_name: the name of the new tag.
    :param definition: a dictionary defining the interface.
    :returns: True if the tag was valid and was registered.

    .. todo ::

        Define definition dict syntax.

.. py:data:: m

    A dictionary of loaded plugins.
    See :ref:`mdict_def` for its format.

    ``m`` is short for *modulelist*.

.. py:data:: mf

    A reference to the first element of a tag group. 

    Instead of writing the long: ::
        
        psys.m['metadata'][0].some_func()

    the bit shorther/clearer: ::

        psys.mf['metadata'].some_func()

    ``mf`` is short for *modulelist (first)*.


Possible interfaces
-------------------

  - ``gtk_browser``, ``qt_browser``, ``nc_browser``:
    
     Plugins that provide custom screens to the GTK, QT, NC GUI

  - ``metadata``:
    
     Plugins that provide coverart, lyrics etc.

  - ``cfg_loader``: 

     If a plugin needs to save config vars, it implementes this tag,
     so it can be visualized in the UIs.
    
  - ``connection_discover``:
    
     Plugins to find connection data to MPD. 
     Possible: Avahi, Env-vars.
