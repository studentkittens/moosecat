#include "protocol.h"

///////////////////

const char * proto_connect (Proto_Connector * self, const char * host, int port, int timeout)
{
    // Call self's do_connect() method
    // It will setup itself basically.
    // return any error that may happened during that.
    return self->do_connect (self, host, port, timeout);
}

///////////////////

void proto_put (Proto_Connector * self)
{
    // Put connection back to event listening
    self->do_put (self);
}

///////////////////

mpd_connection * proto_get (Proto_Connector * self)
{
    // Return the readily sendable connection
    return self->do_get (self);
}

///////////////////

void proto_disconnect (Proto_Connector * self)
{
    // Let the connector free itself and disconnect everything
    self->do_disconnect (self);
}
