cimport binds as c

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

'''
    # Networking
    mc_Client * mc_proto_create (mc_PmType)
    void mc_proto_put (mc_Client *)
    bool mc_proto_is_connected (mc_Client *)
    char * mc_proto_disconnect (mc_Client *)
    void mc_proto_free (mc_Client *)
    char * mc_proto_connect (mc_Client *, void *, char *, int, float)

    # Signals:
    void mc_proto_signal_add (mc_Client * , char *, void *, void *)
    void mc_proto_signal_add_masked (mc_Client * , char *, void *, void *, mpd_idle)
    void mc_proto_signal_rm (mc_Client * , char *, void *)
    void mc_proto_signal_dispatch (mc_Client * , char *, ...)
    int mc_proto_signal_length (mc_Client * , char *)
    void mc_proto_force_sss_update (mc_Client * , mpd_idle)
'''

cdef class Client:
    def __cinit__(self):
        pass


