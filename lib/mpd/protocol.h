#include <mpd/client.h>
#include <stdbool.h>
#include <glib.h>

/* Signal System */
#include "signal.h"

/* Compiler macros */
#include "../compiler.h"

/* Job Dispatcher */
#include "../util/job-manager.h"

#ifndef PROTOCOL_H
#define PROTOCOL_H

typedef enum {
    MC_PM_IDLE = 0,
    MC_PM_COMMAND
} mc_PmType;

/* Prototype Update struct */
struct mc_UpdateData;
struct mc_OutputsData;


/**
 * @brief Structure representing a connection handle,
 * and an interface to send commands and recv. events.
 *
 * It's able to:
 *    - change hosts (connecting/disconnecting) (without loosing registered callbacks)
 *    - Notifying you on events / errors / connection-changes
 *    - Send commands to the server.
 */
typedef struct mc_Client {
    /*
     * Called on connect, initialize the connector.
     * May not be NULL.
     */
    char *(* do_connect)(struct mc_Client *, GMainContext *context, const char *, int, float);

    /*
     * Return the command sending connection, made ready to rock.
     * May not be NULL.
     */
    struct mpd_connection *(* do_get)(struct mc_Client *);

    /*
     * Put the connection back to event listening.
     * May be NULL.
     */
    void (* do_put)(struct mc_Client *);

    /*
     * Called on disconnect, close all connections, clean up,
     * and make reentrant.
     * May not be NULL.
     */
    bool (* do_disconnect)(struct mc_Client *);

    /**
     * Check if a valid connection is hold by this connector.
     * May not be NULL.
     */
    bool (* do_is_connected)(struct mc_Client *);

    /*
     * Free the connector. disconnect() won't free it!
     * May be NULL
     */
    void (* do_free)(struct mc_Client *);

    /////////////////////////////
    //    Actual Variables     //
    /////////////////////////////

    /* This is locked on do_get(),
     * and unlocked on do_put()
     */
    GRecMutex _getput_mutex;

    /* Save last connected host / port */
    char *_host;
    int _port;

    /* Only used for bug_report.c */
    float _timeout;
    mc_PmType _pm;

    /*
     * Signal functions are stored in here
     */
    mc_SignalList _signals;

    /* Outputs of MPD */
    struct mc_OutputsData * _outputs;

    /* Support for timed status retrieval */
    /* TODO: Move this to Update Data? */
    struct {
        int timeout_id;
        int interval;
        bool trigger_event;
        GTimer *last_update;
        bool reset_timer;
    } status_timer;

    /* Up-to-date infos. */
    /* TODO: Move this to update Data? */
    struct mpd_song *song;
    struct mpd_stats *stats;
    struct mpd_status *status;
    const char *replay_gain_status;
    
    /* Data Used by the Status/Song/Stats Update Module */
    struct mc_UpdateData *_update_data;

    /* Locking for updating song/status/stats */
    struct {
        GMutex song, status, stats;
    } update_mtx;

    /* Job Dispatcher */
    struct mc_JobManager *jm;

    struct {
        /* Id of command list job */
        int is_active;
        GMutex is_active_mtx;
        GList *commands;
    } command_list;
    
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
mc_cc_malloc mc_Client *mc_proto_create(mc_PmType pm);

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
char *mc_proto_connect(
    mc_Client *self,
    GMainContext *context,
    const char *host,
    int port,
    float timeout);

/**
 * @brief Return the "send connection" of the Connector.
 *
 * @param self the readily opened connector.
 *
 * @return a working mpd_connection or NULL
 */
struct mpd_connection *mc_proto_get(mc_Client *self);

/**
 * @brief Put conenction back to event listening
 *
 * @param self the connector to operate on
 */
void mc_proto_put(mc_Client *self);

/**
 * @brief Checks if the connector is connected.
 *
 * @param self the connector to operate on
 *
 * @return true when connected
 */
bool mc_proto_is_connected(mc_Client *self);

/**
 * @brief Disconnect connection and free all.
 *
 * @param self connector to operate on
 *
 * @return a error string, or NULL if no error happened
 */
char *mc_proto_disconnect(mc_Client *self);

/**
 * @brief Free all data associated with this connector.
 *
 * You have to call mc_proto_disconnect beforehand!
 *
 * @param self the connector to operate on
 */
void mc_proto_free(mc_Client *self);

///////////////////////////////

/**
 * @brief Add a function that shall be called on a certain event.
 *
 * There are currently 3 valid signal names:
 * - "client-event", See mc_ClientEventCallback
 * - "connectivity", See mc_ConnectivityCallback
 * - "logging",      See mc_LoggingCallback
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
void mc_proto_signal_add(
    mc_Client *self,
    const char *signal_name,
    void *callback_func,
    void *user_data);

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
void mc_proto_signal_add_masked(
    mc_Client *self,
    const char *signal_name,
    void *callback_func,
    void *user_data,
    enum mpd_idle mask);

/**
 * @brief Remove a previously added signal callback.
 *
 * @param self the client to operate on.
 * @param signal_name the signal name this function was registered on.
 * @param callback_addr the addr. of the registered function.
 */
void mc_proto_signal_rm(
    mc_Client *self,
    const char *signal_name,
    void *callback_addr);

/**
 * @brief This function is mostly used internally to call all registered callbacks.
 *
 * @param self the client to operate on.
 * @param signal_name the name of the signal to dispatch.
 * @param ... variable args. See above.
 */
void mc_proto_signal_dispatch(
    mc_Client *self,
    const char *signal_name,
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
int mc_proto_signal_length(
    mc_Client *self,
    const char *signal_name);

/**
 * @brief Forces the client to update all status/song/stats information.
 *
 * This is useful for testing purpose, where no mainloop is running,
 * and therefore no events are updated.
 *
 * @param self the client to operate on.
 * @param events an eventmask. Pass INT_MAX to update all.
 */
void mc_proto_force_sss_update(
    mc_Client *self,
    enum mpd_idle events);

/**
 * @brief Get the hostname being currently connected to.
 *
 * If not connected, NULL is returned.
 *
 * @param self the client to operate on.
 *
 * @return the hostname, do not free or change it!
 */
const char *mc_proto_get_host(mc_Client *self);

/**
 * @brief Get the Port being currently connected to.
 *
 * @param self the client to operate on.
 *
 * @return the port or -1 on error.
 */
int mc_proto_get_port(mc_Client *self);

/**
 * @brief Get the currently set timeout.
 *
 * @param self the client to operate on.
 *
 * @return the timeout in seconds, or -1 on error.
 */
int mc_proto_get_timeout(mc_Client *self);

/**
 * @brief Activate a status timer
 *
 * This will cause moosecat to schedule status update every repeat_ms ms.
 * Call mc_proto_status_timer_unregister to deactivate it.
 *
 * This is useful for kbit rate changes which will only be update when
 * an idle event requires it.
 *
 * @param self the client to operate on.
 * @param repeat_ms repeat interval
 * @param trigger_event
 */
void mc_proto_status_timer_register(
    mc_Client *self,
    int repeat_ms,
    bool trigger_event);

/**
 * @brief Deactivate the status_timer
 *
 * @param self the client to operate on.
 */
void mc_proto_status_timer_unregister(
    mc_Client *self);

/**
 * @brief Returns ttue if the status timer is ative
 *
 * @param self the client to operate on.
 *
 * @return false on inactive status timer
 */
bool mc_proto_status_timer_is_active(mc_Client *self);

/**
 * @brief Make sure any data (status/stats/currentsong/outputs) cannot be 
 *        changed while you didn't close it with mc_data_close()
 *
 * @param self the Client where the data is stored.
 */
void mc_data_open(mc_Client *self);


/**
 * @brief Release the lock created mc_data_open()
 *
 * @param self the Client where the data is stored
 */
void mc_data_close(mc_Client *self);


#endif /* end of include guard: PROTOCOL_H */
