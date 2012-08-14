#include <mpd/client.h>
#include <stdbool.h>
#include <glib.h>

#ifndef PROTOCOL_H
#define PROTOCOL_H

/* lazy selfdef */
typedef struct mpd_connection mpd_connection;
typedef enum mpd_error mpd_error;

/* Event callback */
typedef void (* Proto_EventCallback) (int, bool);

/* Error callback */
typedef void (* Proto_ErrorCallback) (mpd_error);

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
     * Create a GSource that can be integrated into a GMainLoop
     * Must not be NULL.
     */
    unsigned (* do_create_glib_adapter) (struct _Proto_Connector *, GMainContext * context);

    /*
     * Called on disconnect, close all connections, clean up,
     * and make reentrant.
     * May not be NULL.
     */
    bool (* do_disconnect) (struct _Proto_Connector *);

    /*
     * Callback lists
     */
    GList * _event_callbacks;
    GList * _error_callbacks;

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
 * @brief Set a callback that is called when any error is happening
 *
 * Note: double given callbacks are not registered.
 *
 * @param self the connector to observer
 * @param error_cb the callback that is executed, prototype must be Proto_ErrorCallback
 */
void proto_add_error_callback (Proto_Connector * self, Proto_ErrorCallback error_cb);

/**
 * @brief Remove a previously added error callback
 *
 * @param self the connector to change
 * @param callback a previously added function
 */
void proto_rm_error_callback (Proto_Connector * self, Proto_ErrorCallback callback);

/**
 * @brief Hang this connector into a GMainLoop as GSource.
 *
 * You can use proto_add_callback to add a callback, that
 * is called on events
 *
 * @param self the connector itself
 * @param context the context of the mainloop, or NULL for the default context
 *
 * @return the id of the GSource, which can be used to disconnect it.
 */
unsigned proto_create_glib_adapter (struct _Proto_Connector * self, GMainContext * context);

/**
 * @brief Add a callback that is called on mpd events
 *
 * If the callback already has been added, it isn't added twice.
 *
 * Note: The mainloop must be running, and
 * proto_create_glib_adapter() must have been integrated
 * into the mainloop.
 *
 * @param self a connector
 * @param callback a Proto_EventCallback function
 */
void proto_add_event_callback (Proto_Connector * self, Proto_EventCallback callback);

/**
 * @brief Remove a previously registered callback.
 *
 * @param self a connector
 * @param callback a Proto_EventCallback function
 */
void proto_rm_event_callback (Proto_Connector * self, Proto_EventCallback callback);

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
