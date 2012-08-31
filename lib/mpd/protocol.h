#include <mpd/client.h>
#include <stdbool.h>
#include <glib.h>

/* Signal System */
#include "signal.h"

/* Compiler macros */
#include "../compiler.h"

#ifndef PROTOCOL_H
#define PROTOCOL_H

/* lazy typedef */
typedef struct mpd_connection mpd_connection;
typedef struct mpd_async mpd_async;
typedef struct mpd_song mpd_song;
typedef struct mpd_status mpd_status;
typedef struct mpd_stats mpd_stats;

typedef enum {
    MC_PM_IDLE = 0,
    MC_PM_COMMAND
} mc_PmType;

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
typedef struct mc_Client
{
    /*
     * Called on connect, initialize the connector.
     * May not be NULL.
     */
    char * (* do_connect) (struct mc_Client *, GMainContext * context, const char *, int, int);

    /*
     * Return the command sending connection, made ready to rock.
     * May not be NULL.
     */
    mpd_connection * (* do_get) (struct mc_Client *);

    /*
     * Put the connection back to event listening.
     * May be NULL.
     */
    void (* do_put) (struct mc_Client *);

    /*
     * Called on disconnect, close all connections, clean up,
     * and make reentrant.
     * May not be NULL.
     */
    bool (* do_disconnect) (struct mc_Client *);

    /**
     * Check if a valid connection is hold by this connector.
     * May not be NULL.
     */
    bool (* do_is_connected) (struct mc_Client *);

    /*
     * Free the connector. disconnect() won't free it!
     * May be NULL
     */
    void (* do_free) (struct mc_Client *);

    /* Save last connected host / port */
    char * _host;
    int _port;

    /* Only used for bug_report.c */
    int _timeout;
    mc_PmType _pm;

    /*
     * Signal functions are stored in here
     */
    mc_SignalList _signals;

    /*
     * Up-to-date infos.
     * Can be accessed safely in public.
     */
    mpd_song * song;
    mpd_stats * stats;
    mpd_status * status;

} mc_Client;

///////////////////
///////////////////

/**
 * @brief Create a new client.
 *
 * This allocates some memory, but does no network at all.
 * There are different protocol machines implemented in the background,
 * you may choose one:
 *
 * - MC_PM_IDLE - uses one conenction for sending and events, uses idle and noidle to switch context.
 * - MC_PM_COMMAND uses one command connection, and one event listening conenction.
 *
 * Passing NULL to choose the default (MC_PM_COMMAND).
 *
 * @param protocol_machine the name of the protocol machine
 *
 * @return a newly allocated mc_Client, free with mc_proto_free()
 */
mc_cc_malloc mc_Client * mc_proto_create (mc_PmType pm);

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
char * mc_proto_connect (
    mc_Client * self,
    GMainContext * context,
    const char * host,
    int port,
    int timeout);

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
 * @brief Free all data associated with this connector.
 *
 * You have to call mc_proto_disconnect beforehand!
 *
 * @param self the connector to operate on
 */
void mc_proto_free (mc_Client * self);

///////////////////////////////

/**
 * @brief Add a function that shall be called on a certain event.
 *
 * There are currently 4 valid signal names:
 * - "client-event", See mc_ClientEventCallback
 * - "error", See mc_ErrorCallback
 * - "progress", See mc_ProgressCallback
 * - "connectivity", See mc_ConnectivityCallback
 *
 * Callback Order
 * --------------
 *
 *  Callbacks have a defined order in which they may be called.
 *  - "connectivity" is called when the client is fully ready already,
 *    or if the client is still valid (short for disconnect)
 *
 * @param self the client that may trigger events.
 * @param signal_name the name of the signal, see above.
 * @param callback_func the function to call. Prototype depends on signal_name.
 * @param user_data optional user_data to pass to the function.
 */
void mc_proto_signal_add (
    mc_Client * self,
    const char * signal_name,
    void * callback_func,
    void * user_data);

/**
 * @brief Same as mc_proto_signal_add, but only
 *
 * The mask param has only effect on the client-event.
 *
 * @param self
 * @param signal_name
 * @param callback_func
 * @param user_data
 * @param mask
 */
void mc_proto_signal_add_masked (
    mc_Client * self,
    const char * signal_name,
    void * callback_func,
    void * user_data,
    enum mpd_idle mask);

/**
 * @brief Remove a previously added signal callback.
 *
 * @param self the client to operate on.
 * @param signal_name the signal name this function was registered on.
 * @param callback_addr the addr. of the registered function.
 */
void mc_proto_signal_rm (
    mc_Client * self,
    const char * signal_name,
    void * callback_addr);

/**
 * @brief This function is mostly used internally to call all registered callbacks.
 *
 * @param self the client to operate on.
 * @param signal_name the name of the signal to dispatch.
 * @param ... variable args. See above.
 */
void mc_proto_signal_dispatch (
    mc_Client * self,
    const char * signal_name,
    ...);

/**
 * @brief Simply returns the number of registered signals for this signal_name
 *
 * Mainly useful for testing.
 *
 * @param self the client to operate on
 * @param signal_name the name of the signal to count.
 *
 * @return 0 - inf
 */
int mc_proto_signal_length (
        mc_Client * self, 
        const char * signal_name);

#endif /* end of include guard: PROTOCOL_H */
