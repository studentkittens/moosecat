.. _libmoosecat:

libmoosecat.so
==============


The C-level Library powering all the actual MPD related actions in a fast and transparent way.

Technical Overview
------------------

.. note ::

   I assume some knowledge of C and GLib.
   Knowledge of libmpdclient is not required, but doesn't harm.


A (full, i.e. no batch-) MPD Client needs to fulfill at least the following functions: 

- Send commands to the server.
- Listen on events.
- React on erros and connection changes. 

Additionally one may do: 

- Maintain a local database that contains all queried data. (Speed-up / Easier searching)

So, how may one get all those requirements unter one hood? 

A list of problems
------------------

- how to listen and to send at the same time?
- one could use several threads (one for sending, one for receiving events),
  but that would cause troubles on the userside.
  (i.e. most gui toolkits are not directly threadsafe)
- Several connections may be a solution, but MPD may limit connection count.
- using ``idle`` and ``noidle`` commands is cumbersome, and 
  requires many context-switches, since before sending a command
  one must leave the idle-state of the connection
  (and put it back after use).

How to solve it
---------------

- Integrate the event listening mechanism into the mainloop (using GMainLoop and a custom GSource)
- Either:

  * Use the ``idle`` and ``noidle`` commands of the MPD Protocol.
  * Use two connections, one for sending, one for receiving events.
  * libmoosecat implements both variants, letting you c

- Threads can be used by pushing the results of a thread
  into a Queue (GAsyncQueue to be exact). A custom source is
  queries the Queue every few iterations and calls a callback
  in the **main-thread** if any data is available.

- Database-Creation is done completely behind the scene by wrapping
  all mpd commands that query songs.

- for actual parsing of the mpd protocol ``libmpdclient`` is used.
  (Since re-inventing the wheel is silly.)

- Provide a unified API, being very modular.

- Reconnection can be realized by connecting and disconnecting.

.. todo:: 

   Provide sample telnet sessions.


Technical Details
-----------------

**Command-Core** (``lib/mpd/pm/cmnd_core.c``):

  Use two connections. One for sending and one for receiving events.

  Connect:

    * Connect both connections.
    * Instance a GAsyncQueue.
    * Hang a custom GSource that watches the GAsyncQueue
      into a specified GMainContext.
    * Start a thread that constantly sends the ``ìdle`` command.

  Events:

    * Once a event happens the listener thread wakes up and pushes
      the event-bitmask into the GAsyncQueue.
    * A few iterations later the events is discovered by the mainloop.
      Now pop all items in the queue and filter duplicates (which is good),
      Dispatch by calling all registered callbacks.

  Getting/Putting the connection:

    * Nothing needs to be done. The sending connection is always "ready".
      Perhaps: If the connection needs to be used later by several threads,
      a mutex can be locked in get() and unlocked in put().

  Disconnecting:

    * Join idle thread.
    * Free ressources.

  Advantages: 

    * Fast and efficient, due to it's ability to filter duplicate events.
    * Needs to context changes at all.

  Disadvantages:

    * Needs more polling by the mainloop. The timeout to wait at max.
      is cruuial to the responsiveness of the client.
    * Needs two connections. MPD may have set a max. amount of connection.
      
----------

**Idle-Core** (``lib/mpd/pm/idle_core.c``):

  Use only one connection and switch between send and recv. states via ``ìdle`` and ``noidle``.

  Connect:

    * Connect to MPD.
    * Send ``idle`` over the connection.
    * Done.

  Events:

    * Once a new event occures, the connection will wake up from idle-mode.
    * Dispatch by calling all registered callbacks.

  Getting/Putting the connection:

    * Before commands can be sended, connection must be manually waked up.
    * Send ``ǹoidle``, wait for **OK**. Return ready connection.
    * Client does some stuff with the connection and puts the connection.
    * Re-enter the ìdle-state.

  Disconnecting:

    * Leave idle-state.
    * Free ressources.

  Advantages: 

    * Uses only one connection and no extra thread.. Lightweight.
    * Does not rely on the mainloop, dispatches events immediately.

  Disadvantages:

    * Behaves badly on event-floods. For example scrolling on the volume slider
      will cause a lot of events in the system.
    * Many context-changes, some time passes till a command can be sended.

