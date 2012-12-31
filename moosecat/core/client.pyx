cimport binds as c

# bool type
from libcpp cimport bool

# 'with' statement support
from contextlib import contextmanager

'''
moosecat.core.Client implements the Client.

It is a wrapper around libmoosecat's mc_Client,
but also integrates the Database-Access.

Basic Features:

    * connect, disconnect
    * event registration
    * sendind commands

Events are only dispatched if the mainloop is running.


Used headers:

    lib/mpd/protocol.h
    lib/mpd/client.h
    lib/store/db.h
'''



###########################################################################
#                            Callback Wrapper                             #
###########################################################################


# These callbacks are needed in order to make native Python callbacks possible.
# They are usually called directly from libmoosecat, and do nothing but
# executing the corresponding python function (and passing the Cython-Client
# instance with them)

cdef void _wrap_ClientEventCallback(c.mc_Client * client, c.mpd_idle event, data) with gil:
    data[0](data[1], event)


cdef void _wrap_ErrorCallback(c.mc_Client * client, c.mpd_error error, char * msg, bool is_fatal, data) with gil:
    data[0](data[1], error, msg, is_fatal)


cdef void _wrap_ConnectivityCallback(c.mc_Client * client, bool server_changed, data) with gil:
    data[0](data[1], server_changed)


cdef void _wrap_ProgressCallback(c.mc_Client * client, bool print_newline, char * progress, data) with gil:
    data[0](data[1], print_newline, progress)


cdef void _wrap_OpFinishedCallback(c.mc_Client * client, c.mc_OpFinishedEnum operation, data) with gil:
    data[0](data[1], operation)


###########################################################################
#                            Client Definition                            #
###########################################################################


class UnknownSignalName(Exception):
    '''
    This gets thrown if the signal_* methods
    get passed a bad signal name.
    '''
    pass


cdef class Client:
    cdef c.mc_Client * _cl

    def __cinit__(self):
        '''
        Create the initial mc_Client structure.
        This is not connected yet.
        '''
        self._cl = c.mc_proto_create(c.PM_COMMAND)

    cdef c.mc_Client * _p(self):
        return self._cl

    def __dealloc__(self):
        '''
        Disconnect the Client, and free the mc_p()ient structure.
        '''
        c.mc_proto_free(self._p())

    def connect(self, host='localhost', port=6600, timeout_sec=2.):
        cdef char * er = NULL

        # Convert input to bytes
        b_host = bytify(host)

        err = c.mc_proto_connect(self._p(), NULL, b_host, port, timeout_sec)

        if err == NULL:
            return ''
        else:
            s_err = stringify(err)
            return s_err

    def disconnect(self):
        cdef char * err = NULL
        err = c.mc_proto_disconnect(self._p())
        return (err == NULL)

    def force_update_metadata(self, event_mask=0xFFFFFFFFF):
        '''Force the Update of Status/Statistics/Current Song

        This is usually not needed, since it happens automatically.
        No callbacks are called.
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
            yield
        else:
            raise UnknownSignalName(',,' + signal_name + "'' unknown.")

    def signal_add(self, signal_name, func):
        '''
        Add a func to be called on certain events.

        :signal_name: A string describing a signal. See signal_names.
        :func: A callable that is executed upon a event.
        '''
        self.signal_add_masked(signal_name, func, 0xFFFFFFFF)

    def signal_add_masked(self, signal_name, func, idle_event):
        'Add a masked signal (only with "client-event")'
        cdef void * c_func = NULL

        with self._valid_signal_name(signal_name):
            data = [func, self]

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
                c.mc_proto_signal_add_masked(self._p(), signal_name,
                                        <void*> c_func,
                                        <void*> data, idle_event)

    def signal_rm(self, signal_name, func):
        'Remove a signal from the callable list'
        with self._valid_signal_name(signal_name):
            c.mc_proto_signal_rm(self._p(), signal_name, <void*> func)

    def signal_count(self, signal_name):
        'Return the number of registerd signals'
        with self._valid_signal_name(signal_name):
            return c.mc_proto_signal_length(self._p(), signal_name)

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

        with self._valid_signal_name(signal_name):
            # Check the signal-name, and dispatch it differently.
            if signal_name == 'client-event':
                event = int(args[0])
                c.mc_proto_signal_dispatch(self._p(), signal_name, event)
            elif signal_name == 'connectivity':
                server_changed = int(args[0])
                c.mc_proto_signal_dispatch(self._p(), signal_name, server_changed)
            elif signal_name == 'error':
                error_id = int(args[0])
                error_msg = args[1]
                error_is_fatal = int(args[2])
                c.mc_proto_signal_dispatch(self._p(), signal_name, error_id, error_msg, error_is_fatal)
            elif signal_name == 'progress':
                progress_print_newline = int(args[0])
                progress_msg = args[1]
                c.mc_proto_signal_dispatch(self._p(), signal_name, progress_print_newline, progress_msg)
            elif signal_name == 'op-finished':
                op_finished = int(args[0])
                c.mc_proto_signal_dispatch(self._p(), signal_name, op_finished)

    def status_timer_activate(self, repeat_ms, trigger_idle):
        c.mc_proto_status_timer_register(self._p(), repeat_ms, trigger_idle)

    def status_timer_shutdown(self):
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
            return c.mc_proto_get_host(self._p())

    property port:
        'Get the port this client is currently connected to'
        def __get__(self):
            return c.mc_proto_get_port(self._p())

    property signal_names:
        'A list of valid signal names. (May be used to verfiy.)'
        def __get__(self):
            return ['client-event', 'error', 'connectivity', 'op-finished', 'progress']

    property status:
        'Get the current Status object'
        def __get__(self):
            return status_from_ptr(c.mc_proto_get_status(self._p()))

    property currentsong:
        'Get the current Currentsong object'
        def __get__(self):
            return song_from_ptr(self._p().song)

    property statistics:
        'Get the current Stats object'
        def __get__(self):
            return statistics_from_ptr(self._p().stats)

    #####################
    #  Client Commands  #
    #####################

    def player_next(self):
        c.mc_client_next(self._p())

    def player_previous(self):
        c.mc_client_previous(self._p())

    def player_pause(self):
        c.mc_client_pause(self._p())

    def player_play(self, queue_id=None):
        if queue_id is None:
            c.mc_client_play(self._p())
        else:
            c.mc_client_play_id(self._p(), queue_id)

    def player_stop(self):
        c.mc_client_stop(self._p())

    def db_rescan(self):
        c.mc_client_database_rescan(self._p())

    def db_update(self):
        c.mc_client_database_update(self._p())

    def output_is_enabled(self, output_name):
        return c.mc_client_

'''
void mc_client_next (mc_Client * self);
void mc_client_pause (mc_Client * self);
void mc_client_play (mc_Client * self);
void mc_client_stop (mc_Client * self);
void mc_client_previous (mc_Client * self);
void mc_client_play_id (mc_Client * self, unsigned id);

void mc_client_database_rescan (mc_Client * self, const char * path);
void mc_client_database_update (mc_Client * self, const char * path);


bool mc_client_output_switch (mc_Client * self, const char * output_name, bool mode);
bool mc_client_password (mc_Client * self, const char * pwd);
void mc_client_consume (mc_Client * self, bool mode);
void mc_client_crossfade (mc_Client * self, bool mode);
void mc_client_mixramdb (mc_Client * self, int decibel);
void mc_client_mixramdelay (mc_Client * self, int seconds);
void mc_client_playlist_add (mc_Client * self, const char * name, const char * file);
void mc_client_playlist_clear (mc_Client * self, const char * name);
void mc_client_playlist_delete (mc_Client * self, const char * name, unsigned pos);
void mc_client_playlist_load (mc_Client * self, const char * playlist);
void mc_client_playlist_move (mc_Client * self, const char * name, unsigned old_pos, unsigned new_pos);
void mc_client_playlist_rename (mc_Client * self, const char * old_name, const char * new_name);
void mc_client_playlist_rm (mc_Client * self, const char * playlist_name);
void mc_client_playlist_save (mc_Client * self, const char * as_name);
void mc_client_prio_id (mc_Client * self, unsigned prio, unsigned id);
void mc_client_prio (mc_Client * self, unsigned prio, unsigned position);
void mc_client_prio_range (mc_Client * self, unsigned prio, unsigned start_pos, unsigned end_pos);
void mc_client_queue_add (mc_Client * self, const char * uri);
void mc_client_queue_clear (mc_Client * self);
void mc_client_queue_delete_id (mc_Client * self, int id);
void mc_client_queue_delete (mc_Client * self, int pos);
void mc_client_queue_delete_range (mc_Client * self, int start, int end);
void mc_client_queue_move (mc_Client * self, unsigned old_pos, unsigned new_pos);
void mc_client_queue_move_range (mc_Client * self, unsigned start_pos, unsigned end_pos, unsigned new_pos);
void mc_client_queue_shuffle (mc_Client * self);
void mc_client_queue_swap_id (mc_Client * self, int id_a, int id_b);
void mc_client_queue_swap (mc_Client * self, int pos_a, int pos_b);
void mc_client_random (mc_Client * self, bool mode);
void mc_client_repeat (mc_Client * self, bool mode);
void mc_client_replay_gain_mode (mc_Client * self, const char * replay_gain_mode);
void mc_client_seekcur (mc_Client * self, int seconds);
void mc_client_seekid (mc_Client * self, int id, int seconds);
void mc_client_seek (mc_Client * self, int pos, int seconds);
void mc_client_setvol (mc_Client * self, int volume);
void mc_client_single (mc_Client * self, bool mode);

'''
