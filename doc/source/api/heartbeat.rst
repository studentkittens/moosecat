Heartbeat
=========

Description
-----------

A pretty simple class.

Heartbeat looks for ``client-event`` signals and remembers the elpased time of
the currently playing songs. On accessing the ``elapsed`` property it will
calculate the currently elapsed seconds based on the last sync with the server.

The purpose of this class is obvious: We do not want to synchronize with MPD all
the time. (Although there is libmoosecat's status_timer).

As :class:`moosecat.core.Client`'s Signalsystem does, Heartbeat needs a running
Mainloop.

You can access a pre-instanced Heartbeat-object via the ``g`` object: ::

    >>> from moosceat.boot import g
    >>> print(g.heartbeat.elapsed)

Example
-------

Print the calculated value every 500 milliseconds: ::

    from moosecat.boot import boot_base, g
    from gi.repository import GLib

    def timeout_callback():
        print(g.heartbeat.elapsed)
        return True

    boot_base()
    GLib.timeout_add(500, timeout_callback)
    GLib.MainLoop().run()

Reference
---------

.. autoclass:: moosecat.heartbeat.Heartbeat
    :members:
