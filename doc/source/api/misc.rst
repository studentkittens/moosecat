Misc Functions and Classes in the Core
======================================

This includes:

    * :class:`moosecat.core.ZeroconfBrowser` - Class for autodetecting MPD Servers in the
      reach via Avahi.
    * :func:`moosecat.core.bug_report` - Function to create a bug report based
      on the current configuration.
    * :func:`moosecat.core.register_external_logs` - Makes GLib's and Gtk's
      ``g_warning()`` and friends redirect to moosecat's logging callback.
    * :func:`moosecat.core.parse_query` - Parse a Database-Query and translate
      it to a raw SQLite FTS query. Useful for testing & validation.

.. currentmodule:: moosecat.core

.. autosummary::

    ZeroconfBrowser
    bug_report
    parse_query
    register_external_logs


Zeroconf Usage Example
----------------------

.. code-block:: python

    import moosecat.core as m
    from gi.repository import GLib


    def server_found_callback(browser):
        if browser.state == m.ZeroconfState.CHANGED:
            print('+ Servers found!')
            for server in browser.server_list:
                print('  {name} ({addr}:{port})'.format(
                    name=server['name'], addr=server['addr'], port=server['port'])
                )
        elif browser.state == m.ZeroconfState.ALL_FOR_NOW:
            print('= All for now I think!')
        elif browser.state == m.ZeroconfState.ERROR:
            print('@ Crap, an error:', browser.error)


    # Instance a Browser and tell it to call server_found_callback when
    # a new server was detected or if the state of the browser changed.
    m.ZeroconfBrowser().register(server_found_callback)

    # Run the mainloop. This will actually start finding servers.
    # Also quit the mainloop after 2 seconds.
    loop = GLib.MainLoop()
    GLib.timeout_add(2000, loop.quit)
    loop.run()



Reference
---------

.. autoclass:: moosecat.core.ZeroconfState
   :members:

.. autoclass:: moosecat.core.ZeroconfBrowser
   :members:

---------

.. autofunction:: moosecat.core.bug_report

.. autofunction:: moosecat.core.register_external_logs

.. autofunction:: moosecat.core.parse_query

.. autoclass:: moosecat.core.QueryParseException
   :members:
