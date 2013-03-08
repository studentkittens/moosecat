moosecat.core.Client
====================

The client is the central object to use. It represents a connection to the MPD.

**Usage example:**

.. doctest::

    >>> import moosecat.core as m
    >>> client = m.Client()
    >>> client.connect(host='localhost', port=6600)  # might return an error!
    >>> client.player_next()  # Switch to next song
    >>> client.disconnect()
    True

**Important functions:**

.. currentmodule:: moosecat.core

.. autosummary::

    Client.connect
    Client.disconnect


Reference
---------

.. autoclass:: moosecat.core.Client
   :members:

-----

.. autoclass:: moosecat.core.Idle
   :members:

Exceptions
----------

.. autoclass:: moosecat.core.UnknownSignalName
   :members:
