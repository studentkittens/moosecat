Explanation of Moosecat's Plugin System (Draft)
===============================================

Let there be two Plugins: **p1** and **p2**.


Both fulfill a set of tags:

**p2:**

    Tags: ['metadata', 'zeroconf', 'config']

**p2:**

    Tags: ['gtk_browser', 'gtk_config', 'config']

Now let's see what happens in certain stages.

Registration
------------

Before something happens the plugin needs to register it's tags. Why?
It can register itself without, but the **Plugin-System** (**PSys**) will not
recognize the tags without knowing them. So we need to tell **PSys** what tags
exist there: ::

    PSys <----------------------------------- P1
            register_tag('metadata', desc)

If several plugins register the same tags, the first one stays.

After all tags were registered, the Plugin may register itself to **PSys**: ::

    PSys <----------------------------------- P1
            register_plugin(name, import_path, version, priority=50)

Arguments:

    * **name**: A short name of the Plugin.
    * **import_path**: An import path that maybe passed to *__import__*
    * **version**: A version number or string (just for display).
    * **priority**: A priority between 0-100. Higher is more important.

The Plugin-Description can be written as ``__doc__`` string in the Module.


**PSys** will not *load* the plugin. It will just know about it. The user need
to enable it explicitly. 

Tag Description
---------------

Proposal: 

.. code-block:: python

    # A simple Python-Dict
    {
        'name': 'metadata',
        'description': 'Some text describing what metadata plugins do',
        'methods': [
            'get_metadata',
            'cancel_jobs'
        ]
        'classes': [
            'MetaData'
        ]
        'constants': [
            'GLYR_VERSION_USED': 42
        ]
    }

A tag does only very loosely restrict what a Plugin may do internally.
It's even very easy to bypass this ,,interface'', since for examplea *methods* 
does not restrict the number of arguments passed. This should be documented in
the description page.

Plugin-System (**PSys**)
------------------------

**PSys** has several members:

    * A list of loaded modules.
    * A list of registered tags.
    * ...?

Usage
-----

Example Usage:

.. code-block:: python

    from moosecat.plugin.core import psys

    # get a list of registerd plugins for a certain tag:
    # (via implementing __call__)
    for module in psys('metadata'):
        print(module)
        module.method1()

    # get the module with highest priority
    psys['metadata'].method1()

Unloading
---------

A Plugin can be dynamically unloaded.

Problems
--------

* Who loads the Standard Plugins?

    Standard Plugins are loaded during the bootstrap. Via calling psys.load()

* Where are Tag-Description located?

    Maybe a central directory for tag descriptions?

* External Plugins? (out of moosecat's source-tree)

    Shouldn't be a problem as long the plugin are in sys.path

* From where does **PSys** know the *__import__* Path?


