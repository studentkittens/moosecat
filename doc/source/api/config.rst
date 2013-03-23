Configuration
=============

Moosecat needs, as most other applications, to save some values in a key-value
fashion, so that they are editable by the user and persistent to the
application.

**Example Config:** 

.. code-block:: yaml

    paths:
        cache_home: /home/sahib/.cache
        config_home: /home/sahib/.config

    plugins:
        AvahiNetworkProvider:
            browse_timeout: 6666
    
    active_profile: default
    profiles:
        default:
            host: localhost
            port: 6600
            timeout: 5.0


**Points to note:**

    - Config implements a Dictionary-like interface (including, get, set, [])
    - Values are accessed by a URN: ``profiles.default.host`` gets you
      ``"localhost"``.
    - You can access the Config easily with ``g.config``.

Default Values
--------------

On startup a Configuration-File is loaded from the disk (usually
``~/.config/moosecat/config.yaml``). Internally the YAML-file gets 
converted to a Python-Dict. 


On Initial-Run only the Default-Values are in the dictionary, so they get saved
to disk as "Default Config".

Note for Plugins
----------------

* Plugins can use the config as every module.
* Some plugins may want to register own configuration values. 
  This can be done with the :class:`moosecat.plugins.IConfigDefaults` Category.
  
  All plugins within this category are automatically added as Fallback with the 
  ``Config.add_defaults()`` method. 

Example
-------

::

    >>> from moosecat.boot import g
    >>> g.config.get('hurr.durr')  # Does not exist yet
    None
    >>> g.config.get('hurr.durr', default='You silly!')  # Pass a default manually
    "You silly"
    >>> g.config.add_defaults({'hurr': {'durr': 'value'}})  # Provide defaults
    >>> g.config.get('hurr.durr')
    'value'
    >>> g.config.set('hurr.durr', 'new_value')  # Override default value
    >>> g.config.get('hurr.durr')
    'new_value'
    >>> g.config.add_defaults({'hurr': {'durr': 'other_value'}})
    >>> g.config.get('hurr.durr')   # won't change, since it explicitly set
    'new_value'



Reference
---------

.. autoclass:: moosecat.config.Config
    :members:
