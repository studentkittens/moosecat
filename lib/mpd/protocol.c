#include "protocol.h"

/**
 * @brief Connector using two connections, one for sending, one for events.
 *
 * Sending a command:
 * - send it in the cmnd_con, read the response
 * - wait for incoming events asynchronously on idle_con
 */
typedef struct {
  Proto_Connector logic;
  mpd_connection * cmnd_con;
  mpd_connection * idle_con;
} Proto_CmndConnector;

///////////////////

const char * proto_connect(Proto_Connector * type, const char * host, int port, int timeout)
{
    // Call type's do_connect() method
    // It will setup itself basically.
    // return any error that may happened during that.

    return type->do_connect(host, port, timeout);
}

///////////////////

mpd_connection * proto_put(Proto_Connector * type)
{
    // Return the readily put connection
    return type->do_put();
}
