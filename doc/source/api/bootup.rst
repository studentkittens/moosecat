Booting Up Moosecat
===================

.. currentmodule:: moosecat

If you'd need to type all the client connection stuff and store initialization
several times it would become tedious. Thus the ``boot`` module is provided as a
shortcut.

See the respective function documentation below to see what they do exactly for
you. One thing that might be noted apart from this: 
The special ``g`` variable, an instance of :class:`boot.GlobalRegistry`.
Since the client, store and pluginsystem will most likely be instanced only
once, (but are by far not restricted to one instance!) it would be silly to make
each one a Singleton. So think of ``g`` (as in global) as a container for global
variables.

**Usage Example:**

    >>> # Assumes bootup is already done. 
    >>> from moosecat.boot import g
    >>> g.client.player_next()
    >>> # ...

**Full Example:**

    >>> import logging  # For loglevels
    >>> import moosecat.boot as b
    >>> b.boot_base(verbosity=logging.DEBUG)
    >>> b.boot_store(wait=False)  # Do not use store if wait=False!
    >>> some_other_initialization_of_eg_gtk()
    >>> boot.g.client.store.wait()  # Wait for store to finish loading.
    >>> do_stuff_with_client_store_and_psys()
    >>> boot.shutdown_application()


Important Functions at a Glance:
--------------------------------


.. autosummary::

    boot.boot_base
    boot.boot_store
    boot.shutdown_application
    boot.g  


Reference
---------

.. automodule:: moosecat.boot
   :members:
