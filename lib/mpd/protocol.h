#include <mpd/client.h>
#include <stdbool.h>
#include <glib.h>

#ifndef PROTOCOL_H
#define PROTOCOL_H

/* lazy typedef */
typedef struct mpd_connection mpd_connection;
typedef struct mpd_async mpd_async;
typedef struct mpd_song mpd_song;
typedef struct mpd_status mpd_status;
typedef struct mpd_stats mpd_stats;

typedef enum mpd_error mpd_error;
typedef enum mpd_idle mpd_idle;

/* Event callback */
typedef void (* Proto_EventCallback) (enum mpd_idle, void * userdata);

/* Error callback */
typedef void (* Proto_ErrorCallback) (mpd_error, void * userdata);

/**
 * @brief Structure representing a connection handle,
 * and an interface to send commands and recv. events.
 *
 * It's able to:
 *    - change hosts (connecting/disconnecting) (without loosing registered callbacks)
 *    - Notifying you on events / errors / connection-changes
 *    - Send commands to the server.
 *
 *  Note: There is no method to create this struct,
 *        but there are two options you can choose from:
 *          2Connection-ProtocolMachine: mpd/pm/cmnd_core.h
 *          IdleBusy-ProtocolMachine:    mpd/pm/idle_core.h
 *
 *        Read their respective docs.
 */
typedef struct _Proto_Connector
{
    /*
     * Called on connect, initialize the connector.
     * May not be NULL.
     */
    const char * (* do_connect) (struct _Proto_Connector *, GMainContext * context, const char *, int, int);

    /*
     * Return the command sending connection, made ready to rock.
     * May not be NULL.
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

    /**
     * Check if a valid connection is hold by this connector.
     * May not be NULL.
     */
    bool (* do_is_connected) (struct _Proto_Connector *);

    /*
     * Free the connector. disconnect() won't free it!
     * May be NULL
     */
    void (* do_free) (struct _Proto_Connector *);

    /*
     * Callback lists
     */
    GList * _event_callbacks;
    GList * _error_callbacks;

    /*
     * Up-to-date infos
     */
    mpd_song * song;
    mpd_stats * stats;
    mpd_status * status;

} Proto_Connector;

///////////////////

/**
 * @brief Connect to a MPD Server specified by host and port.
 *
 * @param self what kind of conncetion handler to use
 * @param context the main context to run this in (may be NULL for the default)
 * @param host the host, might be a DNS Name, or a IPv4/6 addr
 * @param port a port number (default is 6600)
 * @param timeout timeout in seconds for any operations.
 *
 * Note to the context:
 * It is used to hang in some custom GSources. If no mainloop is running,
 * no events will be reported to you therefore.
 * The client will be fully functional once the application blocks in the mainloop,
 * but it's perfectly valid to send commands before (you will get corresponding events later)
 *
 * @return NULL on success, or an error string describing the kind of error.
 */
const char * proto_connect (Proto_Connector * self, GMainContext * context, const char * host, int port, int timeout);

/**
 * @brief Set a callback that is called when any error is happening
 *
 * Note: double given callbacks are not registered.
 *
 * @param self the connector to observer
 * @param error_cb the callback that is executed, prototype must be Proto_ErrorCallback
 * @param user_data data being passed to the callback
 */
void proto_add_error_callback (Proto_Connector * self, Proto_ErrorCallback error_cb, void * user_data);

/**
 * @brief Remove a previously added error callback
 *
 * @param self the connector to change
 * @param callback a previously added function
 */
void proto_rm_error_callback (Proto_Connector * self, Proto_ErrorCallback callback);

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
 * @param user_data data being passed to the callback
 */
void proto_add_event_callback (Proto_Connector * self, Proto_EventCallback callback, void * user_data);

/**
 * @brief Like proto_add_error_callback, but only listen to events in mask
 *
 * @param mask a bitmask of mpd_idle events. Only events in mask will be callbacked.
 */
void proto_add_event_callback_mask (Proto_Connector * self, Proto_EventCallback callback, void * user_data, mpd_idle mask);

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
 * @brief Checks if the connector is connected.
 *
 * @param self the connector to operate on
 *
 * @return true when connected
 */
bool proto_is_connected (Proto_Connector * self);

/**
 * @brief Disconnect connection and free all.
 *
 * @param self connector to operate on
 *
 * @return a error string, or NULL if no error happened
 */
const char * proto_disconnect (Proto_Connector * self);

/**
 * @brief Send a event to all registered callbacks.
 *
 * This is usually called internally, but it might
 * be useful for you to update the view of you application
 * on initial start when no events happened yet.
 *
 * @param self connector to operate on
 * @param events a enump mpd_idle
 */
void proto_update (Proto_Connector * self, enum mpd_idle events);


/**
 * @brief Free all data associated with this connector.
 *
 * You have to call proto_disconnect beforehand!
 *
 * @param self the connector to operate on
 */
void proto_free (Proto_Connector * self);

#endif /* end of include guard: PROTOCOL_H */
