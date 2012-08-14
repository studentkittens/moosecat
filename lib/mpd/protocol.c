#include "protocol.h"

///////////////////

const char * proto_connect (Proto_Connector * self, const char * host, int port, int timeout)
{
    /*
     * Setup Connector
     * Error that may happenend while that
     * are returned as string
     */
    return self->do_connect (self, host, port, timeout);
}

///////////////////

void proto_put (Proto_Connector * self)
{
    /*
     * Put connection back to event listening.
     */
    self->do_put (self);
}

///////////////////

mpd_connection * proto_get (Proto_Connector * self)
{
    /*
     * Return the readily sendable connection
     */
    return self->do_get (self);
}

///////////////////

void proto_add_event_callback (Proto_Connector * self, Proto_EventCallback callback)
{
    if (g_list_find (self->_event_callbacks, callback) == NULL)
        self->_event_callbacks = g_list_prepend (self->_event_callbacks, callback);
}

///////////////////

void proto_rm_event_callback (Proto_Connector * self, Proto_EventCallback callback)
{
    self->_event_callbacks = g_list_remove (self->_event_callbacks, callback);
}

///////////////////

void proto_add_error_callback (Proto_Connector * self, Proto_ErrorCallback callback)
{
    if (g_list_find (self->_error_callbacks, callback) == NULL)
        self->_error_callbacks = g_list_prepend (self->_error_callbacks, callback);
}

///////////////////

void proto_rm_error_callback (Proto_Connector * self, Proto_ErrorCallback callback)
{
    self->_error_callbacks = g_list_remove (self->_error_callbacks, callback);
}

///////////////////

void proto_disconnect (Proto_Connector * self)
{
    /* free the callback list */
    g_list_free (self->_event_callbacks);
    g_list_free (self->_error_callbacks);

    /* let the connector clean up itself */
    self->do_disconnect (self);
}

///////////////////

unsigned proto_create_glib_adapter (struct _Proto_Connector * self, GMainContext * context)
{
    return self->do_create_glib_adapter (self, context);
}
