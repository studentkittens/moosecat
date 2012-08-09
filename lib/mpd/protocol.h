#include <mpd/client.h>
#include <stdbool.h>

#ifndef PROTOCOL_H
#define PROTOCOL_H

/* lazy selfdef */
typedef struct mpd_connection mpd_connection;

typedef struct _Proto_Connector
{
    /*
     * Called on connect, initialize the connector.
     * May not be NULL.
     */
    const char * (* do_connect) (struct _Proto_Connector *, const char *, int, int);

    /*
     * Return the command sending connection, made ready to rock.
     * May be NULL.
     */
    mpd_connection * (* do_get) (struct _Proto_Connector *);

    /*
     * Put the connection back to event listening.
     * May be NULL.
     */
    void (* do_put) (struct _Proto_Connector *);

    /*
     * Called on disconnect, close all connections, clean up,
     * and make reentrant.
     * May not be NULL.
     */
    bool (* do_disconnect) (struct _Proto_Connector *);

} Proto_Connector;

///////////////////

/**
 * @brief Connect to a MPD Server specified by host and port.
 *
 * @param self what kind of conncetion handler to use
 * @param host the host, might be a DNS Name, or a IPv4/6 addr
 * @param port a port number (default is 6600)
 * @param timeout timeout in seconds for any operations.
 *
 * @return NULL on success, or an error string describing the kind of error.
 */
const char * proto_connect (Proto_Connector * self, const char * host, int port, int timeout);

/**
 * @brief Return the "send connection" of the Connector.
 *
 * @param self the readily opened connector.
 *
 * @return a working mpd_connection or NULL
 */
mpd_connection * proto_get (Proto_Connector * self);

/**
 * @brief Put conenction back to event listening
 *
 * @param self the connector to operate on
 */
void proto_put (Proto_Connector * self);

/**
 * @brief Disconnect connection and free all.
 *
 * @param self connector to operate on
 */
void proto_disconnect (Proto_Connector * self);

#endif /* end of include guard: PROTOCOL_H */
