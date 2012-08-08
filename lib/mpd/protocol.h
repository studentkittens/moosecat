#include <mpd/client.h>
#include <stdbool.h>

/* lazy typedef */
typedef struct mpd_connection mpd_connection;

typedef struct {
    /*
     * Called on connect, initialize the connector.
     * May not be NULL. 
     */
    const char * (* do_connect)(const char *, int, int);

    /*
     * Return the command sending connection, made ready to rock.
     * May not be NULL.
     */
    mpd_connection * (* do_put)(void);

    /*
     * Called on disconnect, close all connections, clean up,
     * and make reentrant.
     * May not be NULL. 
     */
    bool (* do_disconnect)(void);

} Proto_Connector;

///////////////////

/**
 * @brief Connect to a MPD Server specified by host and port.
 *
 * @param type what kind of conncetion handler to use
 * @param host the host, might be a DNS Name, or a IPv4/6 addr
 * @param port a port number (default is 6600)
 * @param timeout timeout in seconds for any operations.
 *
 * @return NULL on success, or an error string describing the kind of error.
 */
const char * proto_connect(Proto_Connector * type, const char * host, int port, int timeout);

/**
 * @brief Return the "send connection" of the Connector.
 *
 * @param type the readily opened connector.
 *
 * @return a working mpd_connection or NULL
 */
mpd_connection * proto_put(Proto_Connector * type);
