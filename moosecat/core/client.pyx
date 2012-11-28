cimport binds as c

# bool type
from cpython cimport bool

# 'with' statement support
from contextlib import contextmanager

'''
moosecat.core.moose.Client implements the Client.

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
        self._cl = c.mc_proto_create(c.MC_PM_IDLE)

    cdef c.mc_Client * _ptr(self):
        return self._cl

    def __dealloc__(self):
        '''
        Disconnect the Client, and free the mc_ptr()ient structure.
        '''
        c.mc_proto_free(self._ptr())

    def connect(self, host='localhost', port=6600, timeout_sec=2.):
        cdef char * er = NULL
        err = c.mc_proto_connect(self._ptr(), NULL, host, port, timeout_sec)
        print('Connect: ', err)
        return (err == NULL)

    def disconnect(self):
        cdef char * err = NULL
        err = c.mc_proto_disconnect(self._ptr())
        print('Disconnect: ', err)
        return (err == NULL)

    def force_update_metadata(self, event_mask):
        '''Force the Update of Status/Statistics/Current Song

        This is usually not needed, since it happens automatically.
        No callbacks are called.
        '''
        cdef c.mpd_idle event = event_mask
        c.mc_proto_force_sss_update(self._ptr(), event)

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
                c.mc_proto_signal_add_masked(self._ptr(), signal_name,
                                        <void*> c_func,
                                        <void*> data, idle_event)

    def signal_rm(self, signal_name, func):
        'Remove a signal from the callable list'
        with self._valid_signal_name(signal_name):
            c.mc_proto_signal_rm(self._ptr(), signal_name, <void*> func)

    def signal_count(self, signal_name):
        'Return the number of registerd signals'
        with self._valid_signal_name(signal_name):
            return c.mc_proto_signal_length(self._ptr(), signal_name)

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
                c.mc_proto_signal_dispatch(self._ptr(), signal_name, event)
            elif signal_name == 'connectivity':
                server_changed = int(args[0])
                c.mc_proto_signal_dispatch(self._ptr(), signal_name, server_changed)
            elif signal_name == 'error':
                error_id = int(args[0])
                error_msg = args[1]
                error_is_fatal = int(args[2])
                c.mc_proto_signal_dispatch(self._ptr(), signal_name, error_id, error_msg, error_is_fatal)
            elif signal_name == 'progress':
                progress_print_newline = int(args[0])
                progress_msg = args[1]
                c.mc_proto_signal_dispatch(self._ptr(), signal_name, progress_print_newline, progress_msg)
            elif signal_name == 'op-finished':
                op_finished = int(args[0])
                c.mc_proto_signal_dispatch(self._ptr(), signal_name, op_finished)

    ################
    #  Properties  #
    ################

    property is_connected:
        'Check if this Client is still connected  to the server'
        def __get__(self):
            return c.mc_proto_is_connected(self._ptr())

    property signal_names:
        'A list of valid signal names. (May be used to verfiy.)'
        def __get__(self):
            return ['client-event', 'error', 'connectivity', 'op-finished', 'progress']

