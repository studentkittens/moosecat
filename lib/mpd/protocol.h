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
typedef struct _mc_Client
{
    /*
     * Called on connect, initialize the connector.
     * May not be NULL.
     */
    char * (* do_connect) (struct _mc_Client *, GMainContext * context, const char *, int, int);

    /*
     * Return the command sending connection, made ready to rock.
     * May not be NULL.
     */
    mpd_connection * (* do_get) (struct _mc_Client *);

    /*
     * Put the connection back to event listening.
     * May be NULL.
     */
    void (* do_put) (struct _mc_Client *);

    /*
     * Called on disconnect, close all connections, clean up,
     * and make reentrant.
     * May not be NULL.
     */
    bool (* do_disconnect) (struct _mc_Client *);

    /**
     * Check if a valid connection is hold by this connector.
     * May not be NULL.
     */
    bool (* do_is_connected) (struct _mc_Client *);

    /*
     * Free the connector. disconnect() won't free it!
     * May be NULL
     */
    void (* do_free) (struct _mc_Client *);

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

} mc_Client;

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
char * mc_proto_connect (mc_Client * self, GMainContext * context, const char * host, int port, int timeout);

/**
 * @brief Set a callback that is called when any error is happening
 *
 * Note: double given callbacks are not registered.
 *
 * @param self the connector to observer
 * @param error_cb the callback that is executed, prototype must be Proto_ErrorCallback
 * @param user_data data being passed to the callback
 */
void mc_proto_add_error_callback (mc_Client * self, Proto_ErrorCallback error_cb, void * user_data);

/**
 * @brief Remove a previously added error callback
 *
 * @param self the connector to change
 * @param callback a previously added function
 */
void mc_proto_rm_error_callback (mc_Client * self, Proto_ErrorCallback callback);

/**
 * @brief Add a callback that is called on mpd events
 *
 * If the callback already has been added, it isn't added twice.
 *
 * Note: The mainloop must be running, and
 * mc_proto_create_glib_adapter() must have been integrated
 * into the mainloop.
 *
 * @param self a connector
 * @param callback a Proto_EventCallback function
 * @param user_data data being passed to the callback
 */
void mc_proto_add_event_callback (mc_Client * self, Proto_EventCallback callback, void * user_data);

/**
 * @brief Like mc_proto_add_error_callback, but only listen to events in mask
 *
 * @param mask a bitmask of mpd_idle events. Only events in mask will be callbacked.
 */
void mc_proto_add_event_callback_mask (mc_Client * self, Proto_EventCallback callback, void * user_data, mpd_idle mask);

/**
 * @brief Remove a previously registered callback.
 *
 * @param self a connector
 * @param callback a Proto_EventCallback function
 */
void mc_proto_rm_event_callback (mc_Client * self, Proto_EventCallback callback);

/**
 * @brief Return the "send connection" of the Connector.
 *
 * @param self the readily opened connector.
 *
 * @return a working mpd_connection or NULL
 */
mpd_connection * mc_proto_get (mc_Client * self);

/**
 * @brief Put conenction back to event listening
 *
 * @param self the connector to operate on
 */
void mc_proto_put (mc_Client * self);

/**
 * @brief Checks if the connector is connected.
 *
 * @param self the connector to operate on
 *
 * @return true when connected
 */
bool mc_proto_is_connected (mc_Client * self);

/**
 * @brief Disconnect connection and free all.
 *
 * @param self connector to operate on
 *
 * @return a error string, or NULL if no error happened
 */
char * mc_proto_disconnect (mc_Client * self);

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
void mc_proto_update (mc_Client * self, enum mpd_idle events);


/**
 * @brief Free all data associated with this connector.
 *
 * You have to call mc_proto_disconnect beforehand!
 *
 * @param self the connector to operate on
 */
void mc_proto_free (mc_Client * self);

#endif /* end of include guard: PROTOCOL_H */
