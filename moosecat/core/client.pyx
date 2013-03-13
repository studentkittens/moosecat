cimport binds as c

# bool type
from libcpp cimport bool

# Exception logging
from cpython cimport PyErr_Print, PyErr_Occurred

# 'with' statement support
from contextlib import contextmanager

# For signal handler that do not work well.
import logging

'''
moosecat.core.Client implements the Client.

It is a wrapper around libmoosecat's mc_Client,
but also integrates the Database-Access.

Basic Features:

    * connect, disconnect
    * event registration
    * sendind commands

Events are only dispatched if the GMainLoop is running.

There is a bit of hackery in here.

Used headers:

    lib/mpd/protocol.h
    lib/mpd/client.h
    lib/store/db.h
'''



###########################################################################
#                            Callback Wrapper                             #
###########################################################################


def log_exception(func):
    logging.exception('Unhandled Exception in SignalHandler <{mod}.{func}>'.format(
        mod=func.__module__,
        func=func.__name__))

# These callbacks are needed in order to make native Python callbacks possible.
# They are usually called directly from libmoosecat, and do nothing but
# executing the corresponding python function (and passing the Cython-Client
# instance with them)
#
# This is repeated code since it is hard to make it generalized.
# Also it would add stuff to the callstack.

cdef void _wrap_ClientEventCallback(
    c.mc_Client * client,
    c.mpd_idle event,
    object data
) with gil:
    try:
        data[0](data[1], event)
    except:
        log_exception(data[0])


cdef void _wrap_ErrorCallback(
    c.mc_Client * client,
    c.mpd_error error,
    char * msg,
    bool is_fatal,
    object data
) with gil:
    try:
        s_msg= stringify(msg)
        data[0](data[1], error, s_msg, is_fatal)
    except:
        log_exception(data[0])


cdef void _wrap_ConnectivityCallback(
    c.mc_Client * client,
    bool server_changed,
    object data
) with gil:
    try:
        data[0](data[1], server_changed)
    except:
        log_exception(data[0])

cdef void _wrap_ProgressCallback(
    c.mc_Client * client,
    bool print_newline,
    char * progress,
    object data
) with gil:
    try:
        s_progress = stringify(progress)
        data[0](data[1], print_newline, s_progress)
    except:
        log_exception(data[0])


cdef void _wrap_OpFinishedCallback(
    c.mc_Client * client,
    c.mc_OpFinishedEnum operation,
    object data
) with gil:
    try:
        data[0](data[1], operation)
    except:
        log_exception(data[0])


###########################################################################
#                            Client Definition                            #
###########################################################################


class UnknownSignalName(Exception):
    '''
    This exception gets thrown if the signal_* methods
    get passed a bad signal name.
    '''
    pass


# Idle-Enum
class Idle:
    '''
    Numerical constants for Idle Events.
    This is the same enum as in mpd/idle.h.

    You can pass it to client.sync(),
    or get it when writing a idle-event handler.

    Example::

        >>> import moosecat.core as m
        >>> # Update all data that depend on the Queue and Player status
        >>> your_client.sync(m.Idle.QUEUE | m.Idle.PLAYER)

    Constants (named after the event they represent obviously): ::

        DATABASE
        STORED_PLAYLIST
        QUEUE
        PLAYER
        MIXER
        OUTPUT
        OPTIONS
        UPDATE
        STICKER
        SUBSCRIPTION
        MESSAGE

    '''
    DATABASE = c.MPD_IDLE_DATABASE
    STORED_PLAYLIST = c.MPD_IDLE_STORED_PLAYLIST
    QUEUE = c.MPD_IDLE_QUEUE
    PLAYLIST = c.MPD_IDLE_QUEUE
    PLAYER = c.MPD_IDLE_PLAYER
    MIXER = c.MPD_IDLE_MIXER
    OUTPUT = c.MPD_IDLE_OUTPUT
    OPTIONS = c.MPD_IDLE_OPTIONS
    UPDATE = c.MPD_IDLE_UPDATE
    STICKER = c.MPD_IDLE_STICKER
    SUBSCRIPTION = c.MPD_IDLE_SUBSCRIPTION
    MESSAGE = c.MPD_IDLE_MESSAGE


cdef class Client:
    cdef c.mc_Client * _cl
    cdef c.mc_StoreDB * _store
    cdef object _store_wrapper
    cdef object _signal_data_map

    def __cinit__(self, protocol_machine='idle'):
        '''
        Create the initial mc_Client structure.
        This is not connected yet.
        '''
        if protocol_machine == 'idle':
            self._cl = c.mc_proto_create(c.PM_IDLE)
        else:
            self._cl = c.mc_proto_create(c.PM_COMMAND)

        self._store = NULL
        self._store_wrapper = None

    def __init__(self):
        'Create a new client object'
        self._signal_data_map = {}

    cdef c.mc_Client * _p(self):
        return self._cl

    def __dealloc__(self):
        '''
        Disconnect the Client, and free the mc_p()ient structure.
        '''
        c.mc_proto_free(self._p())

    def connect(self, host='localhost', port=6600, timeout_sec=2.):
        '''Connect this client to a server.

        Will trigger the "connectivity" signal.

        :host: the host (might be a IP(v4 or v6) or a DNS name), default: localhost
        :port: the port number (6600 as in mpd's default)
        :timeout_sec: timeout in seconds.
        :returns: None on success, a string describing an error on failure
        '''
        cdef char * er = NULL

        # Convert input to bytes
        b_host = bytify(host)

        err = c.mc_proto_connect(self._p(), NULL, b_host, port, timeout_sec)

        if err == NULL:
            return None
        else:
            s_err = stringify(err)
            return s_err

    def disconnect(self):
        '''
        Disconnect from previous server.

        Will trigger the connectivity signal.
        '''
        cdef char * err = NULL
        err = c.mc_proto_disconnect(self._p())
        return (err == NULL)

    def sync(self, event_mask=0xFFFFFFFFF):
        '''Force the Update of Status/Statistics/Current Song

        This is usually not needed, since it happens automatically.

        **No callbacks are called.**
        '''
        cdef c.mpd_idle event = event_mask
        c.mc_proto_force_sss_update(self._p(), event)

    #############
    #  Signals  #
    #############

    @contextmanager
    def _valid_signal_name(self, signal_name):
        '''
        with context manager. Only executes when signal_name is valid.

        Othwerwise a UnknownSignalName Exception is raised, with the unkown
        signal_name included in the error message.
        '''
        if signal_name in self.signal_names:
            b_name = bytify(signal_name)
            yield b_name
        else:
            raise UnknownSignalName(',,' + signal_name + "'' unknown.")

    def signal_add(self, signal_name, func, mask=0xFFFFFFFF):
        '''
        Add a func to be called on certain events.

        :signal_name: A string describing a signal. See signal_names.
        :func: A callable that is executed upon a event.
        :mask:
        '''
        cdef void * c_func = NULL

        with self._valid_signal_name(signal_name) as b_name:
            # This data is passed to the callback.
            # The callback is actually called on the C-side, and
            # just forwards a Py_Object object.
            data = [func, self]

            # Since signal_add() returns immediately, we have to make sure
            # the local "data" survives the return. Since callbacks might
            # be called several times we cannot use Py_INCREF here.
            self._signal_data_map[func] = data

            if signal_name == 'client-event':
                c_func = <void*>_wrap_ClientEventCallback
            elif signal_name == 'error':
                c_func = <void*>_wrap_ErrorCallback
            elif signal_name == 'connectivity':
                c_func = <void*>_wrap_ConnectivityCallback
            elif signal_name == 'progress':
                c_func = <void*>_wrap_ProgressCallback
            elif signal_name == 'op-finished':
                c_func = <void*>_wrap_OpFinishedCallback
            else:
                print('Warning: Unknown signal passed. This should not happen.')

            if c_func != NULL:
                c.mc_proto_signal_add_masked(self._p(), b_name,
                                        <void*> c_func,
                                        <void*> data, mask)

    def signal_rm(self, signal_name, func):
        'Remove a signal from the callable list'
        with self._valid_signal_name(signal_name) as b_name:
            c.mc_proto_signal_rm(self._p(), b_name, <void*> func)

            # Remove the ref to the internally hold callback data,
            # so this can get garbage collected, properly counted.
            del self._signal_data_map[func]

    def signal_count(self, signal_name):
        'Return the number of registerd signals'
        with self._valid_signal_name(signal_name) as b_name:
            return c.mc_proto_signal_length(self._p(), b_name)

    def signal_dispatch(self, signal_name, *args):
        'Dispatch a signal manually'
        # Somehow this needs to be declared on top.
        cdef c.mpd_idle event
        cdef int server_changed
        cdef c.mc_OpFinishedEnum op_finished

        cdef char * progress_msg
        cdef int progress_print_newline

        cdef c.mpd_error error_id
        cdef char * error_msg
        cdef int error_is_fatal

        with self._valid_signal_name(signal_name) as b_name:
            # Check the signal-name, and dispatch it differently.
            if signal_name == 'client-event':
                event = int(args[0])
                c.mc_proto_signal_dispatch(self._p(), b_name, self._p(), event)
            elif signal_name == 'connectivity':
                server_changed = int(args[0])
                c.mc_proto_signal_dispatch(self._p(), b_name,self._p(), server_changed)
            elif signal_name == 'error':
                error_id = int(args[0])
                error_msg = args[1]
                error_is_fatal = int(args[2])
                c.mc_proto_signal_dispatch(self._p(), b_name, self._p(), error_id, error_msg, error_is_fatal)
            elif signal_name == 'progress':
                progress_print_newline = int(args[0])
                progress_msg = args[1]
                c.mc_proto_signal_dispatch(self._p(), b_name, self._p(), progress_print_newline, progress_msg)
            elif signal_name == 'op-finished':
                op_finished = int(args[0])
                c.mc_proto_signal_dispatch(self._p(), b_name, self._p(), op_finished)

    def signal(self, signal_name, mask=None):
        'For use as a decorator over a function.'
        def _signal(function):
            # Register the function
            self.signal_add(signal_name, function)
        return _signal

    def status_timer_activate(self, repeat_ms=500, trigger_idle=True):
        '''
        Retrieve the mpd's status additionally every repeat_ms seconds.

        This is useful to receive changing things like the kbitrate.
        By default, no status timer is active. (status gets only updated on normal events)

        :repeat_ms: Number of seconds to wait betwenn status updates.
        :trigger_idle: If the idle-signal shall be called.
        '''
        c.mc_proto_status_timer_register(self._p(), repeat_ms, trigger_idle)

    def status_timer_shutdown(self):
        'Reverse a previous call to status_timer_activate()'
        c.mc_proto_status_timer_unregister(self._p())

    ################
    #  Properties  #
    ################

    property status_timer_is_active:
        'Check if this Client currently is auto-querying status updates'
        def __get__(self):
            return c.mc_proto_status_timer_is_active(self._p())

    property is_connected:
        'Check if this Client is still connected  to the server'
        def __get__(self):
            return c.mc_proto_is_connected(self._p())

    property timeout:
        'Get the timeout in seconds which is set for this client'
        def __get__(self):
            return c.mc_proto_get_timeout(self._p())

    property host:
        'Get the host this client is currently connected to'
        def __get__(self):
            b_host = <char*>c.mc_proto_get_host(self._p())
            return stringify(b_host)

    property port:
        'Get the port this client is currently connected to'
        def __get__(self):
            return c.mc_proto_get_port(self._p())

    property signal_names:
        'A list of valid signal names. (May be used to verfiy.)'
        def __get__(self):
            return ['client-event', 'error', 'connectivity', 'op-finished', 'progress']

    property status:
        'Get the current :class:`.Status` object.'
        def __get__(self):
            return status_from_ptr(c.mc_proto_get_status(self._p()), self._p())

    property currentsong:
        'Get the current :class:`.Song` object.'
        def __get__(self):
            return song_from_ptr(c.mc_proto_get_current_song(self._p()))

    property statistics:
        'Get the current :class:`.Statistics` object.'
        def __get__(self):
            return statistics_from_ptr(c.mc_proto_get_statistics(self._p()))

    property outputs:
        '''
        Get a list of outputs on serverside.

        The retrieved outputs have a name, an ID and a flag indicating if they are enabled.
        If you want to enable them you could do the following::

            >>> audios = client.outputs
            >>> for output in audios:
            ...     output.enabled = True

        :returns: A list of :class:`.AudioOutput` objects.
        '''
        def __get__(self):
            # This is very C-ish. Sorry.
            cdef int size = 0
            cdef c.mpd_output ** op_list = c.mc_proto_get_outputs(self._p(), &size)

            # Iterate over all outputs, and wrap them into a AudioOutput
            return_list = []
            if op_list != NULL:
                i = 0
                while i < size:
                    return_list.append(AudioOutput()._init(op_list[i], self._p()))
                    i += 1

            return return_list

    property store:
        '''
        a song-store associated to this client.

        This is only available if store_initialize() was called.
        Otherwise it will raise an AttributeError.

        See :class:`.Store` for detailed information what you can do with it.
        '''
        def __get__(self):
            if self._store_wrapper:
                return self._store_wrapper
            else:
                raise AttributeError('store_initialize() not called yet.')

    ####################
    #  Store commands  #
    ####################

    def store_initialize(self, db_directory, use_memory_db=True, use_compression=True, tokenizer=None):
        '''
        Initialize a new store associated with this client

        :db_directory: the directory where the SQLite Database will be saved in.
        :use_memory_db: Cache the whole database in memory (a bit faster)
        :use_compression: save the database file with zip compression on disk
        :tokenizer: a string naming a FTS-Tokenizer. Default is
        '''
        self._store_wrapper = Store()._init(self._p(), db_directory, use_memory_db, use_compression, tokenizer)
        self._store_wrapper.load()

    #####################
    #  Client Commands  #
    #####################

    def player_next(self):
        '''
        Switch to next song.

        Events: Idle.PLAYER
        '''
        c.mc_client_next(self._p())

    def player_previous(self):
        '''
        Switch to previous song.

        Events: Idle.PLAYER
        '''
        c.mc_client_previous(self._p())

    def player_pause(self):
        '''
        Pause the playback, or continue if already paused.

        Events: Idle.PLAYER
        '''
        c.mc_client_pause(self._p())

    def player_play(self, queue_id=None):
        '''
        Start playing. If playing already, do nothing.

        Events: Idle.PLAYER (if something changed)
        '''
        if queue_id is None:
            c.mc_client_play(self._p())
        else:
            c.mc_client_play_id(self._p(), queue_id)

    def player_stop(self):
        '''
        Stop the Playback (reset song to 0 seconds).

        Events: Idle.PLAYER
        '''
        c.mc_client_stop(self._p())

    def database_rescan(self, path='/'):
        '''
        Rescan the whole database, not only changed songs.
        This is a very expensive operation and usually not needed.

        Events: Idle.DATABASE
        '''
        b_path = bytify(path)
        c.mc_client_database_rescan(self._p(), b_path)

    def database_update(self, path='/'):
        '''
        Update the database with new songs.

        Events: Idle.DATABASE if something changed
        '''
        b_path = bytify(path)
        c.mc_client_database_update(self._p(), b_path)

    def authenticate(self, password):
        '''
        Send a password to the server to gain privleged rights.

        :returns: true if server accepted the password.
        '''
        b_password = bytify(password)
        return c.mc_client_password (self._p(), b_password)

    def player_seek(self, seconds, song=None):
        '''
        Seek into a specific song.

        Event: Idle.PLAYER

        :seconds: the position to seek into from 0.
        :song: the song to seek into, if None the current song is used.
        '''
        if song is None:
            c.mc_client_seekcur(self._p(), seconds)
        else:
            c.mc_client_seekid(self._p(), song.queue_id, seconds)

    def set_priority(self, prio, song, song_end=None):
        '''
        Set the priority of a certain song.

        Events: Idle.QUEUE

        :prio: A priority [0-100]
        :song: the song to set the priority to.
        :son_end: If not none, apply the priority to the range [song-song_end]
        '''
        if song_end is None:
            c.mc_client_prio(self._p(), prio, song.queue_pos)
        elif song_end is not None and song.queue_pos > song_end.queue_pos:
            c.mc_client_prio_range(self._p(), prio, song.queue_pos, song_end.queue_pos)
        else:
            raise ValueError('song >= song_end')

    #####################
    #  Queue Commands   #
    #####################

    def queue_add(self, uri):
        '''
        Add a URI recursively to the Queue.

        Example: ``queue_add('/')`` adds the whole database.
        Use queue_add_song() if you want to add a song instance only.
        '''
        b_uri = bytify(uri)
        c.mc_client_queue_add(self._p(), b_uri)

    def queue_add_song(self, song):
        '''
        Add specified song to the Queue. If it is already in the Queue, it simply gets appended.
        '''
        b_uri = bytify(song.uri)
        c.mc_client_queue_add(self._p(), b_uri)

    def queue_clear(self):
        '''
        Clear the whole Queue, leaving it empty.

        Events: Idle.QUEUE
        '''
        c.mc_client_queue_clear(self._p())

    def queue_delete(self, song, song_end=None):
        '''
        Delete a certain song or a range of songs from the Queue.

        Events: Idle.QUEUE

        :song: the song the delete. (a moosecat.core.Song).
        :song_end: If not None, the range between song and song_end is moved.
        '''
        if song_end is None:
            c.mc_client_queue_delete_id (self._p(), song.queue_id)
        else:
            c.mc_client_queue_delete_range(self._p(), song.queue_id, song_end.queue_id)

    def queue_move(self, song, song_end=None, offset=1):
        '''
        Move a song or a range of songs in the Queue by a certain offset.

        Events: Idle.QUEUE

        :song_end: If not None, move the whole range between song and song_range
        :offset: Offset that the song will be moved, may be negative or 0 (does nothing).
        '''
        if offset is not 0:
            if song_end is None:
                c.mc_client_queue_move(self._p(), song.queue_pos, song.queue_pos + offset)
            else:
                c.mc_client_queue_move_range(self._p(), song.queue_pos, song_end.queue_pos, song.queue_pos + offset)

    def queue_shuffle(self):
        '''
        Shuffle the Queue in a random order.
        Will cause a full Queue reload.

        Events: Idle.QUEUE
        '''
        c.mc_client_queue_shuffle(self._p())

    def queue_swap(self, song_fst, song_snd):
        '''
        Swap two songs in the Queue.

        Events: Idle.QUEUE
        '''
        c.mc_client_queue_swap_id(self._p(), song_fst.queue_id, song_snd.queue_id)

    def queue_save(self, as_name):
        '''
        Save the current Queue as playlist named "as_name"

        Events: Idle.STORED_PLAYLIST
        '''
        b_as_name = bytify(as_name)
        c.mc_client_playlist_save(self._p(), b_as_name)

    ###########################
    #  Command List Commands  #
    ###########################

    @contextmanager
    def command_list_mode(self):
        '''
        A contextmanager.

        Execute Client commands in command_list_mode.
        Commands get send at once to the server, which processes them at once,
        and replies them as one. If one command fails the rest of the list is discarded.

        It is supposed if many commands need to be send to the server, but no output is expected.

        .. note:: Since the Python Part is not trusted to mess with the connection:
                  There is no way to get the response of the sended commands.


        Example: ::

            >>> import moosecat.core as m
            >>> c = m.Client()
            >>> c.connect()
            >>> with c.command_list_mode():
            ...     c.player_next()
            ...     c.player_previous()
            >>> # got sended.
        '''
        if self.is_connected:
            c.mc_client_command_list_begin(self._p())
            if c.mc_client_command_list_is_active(self._p()):
                try:
                    yield
                finally:
                    c.mc_client_command_list_commit(self._p())
