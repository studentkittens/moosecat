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

    /* Indicates if store was not connected yet.
     * */
    bool is_virgin;

    /* Only used for bug_report.c */
    float _timeout;
    mc_PmType _pm;

    /* Signal functions are stored in here */
    mc_SignalList _signals;

    /* Outputs of MPD */
    struct mc_OutputsData * _outputs;

    /* Data Used by the Status/Song/Stats Update Module */
    struct mc_UpdateData *_update_data;

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
 * @return a newly allocated mc_Client, free with mc_free()
 */
mc_cc_malloc mc_Client *mc_create(mc_PmType pm);

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
char *mc_connect(
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
struct mpd_connection *mc_get(mc_Client *self);

/**
 * @brief Put conenction back to event listening
 *
 * @param self the connector to operate on
 */
void mc_put(mc_Client *self);

/**
 * @brief Checks if the connector is connected.
 *
 * @param self the connector to operate on
 *
 * @return true when connected
 */
bool mc_is_connected(mc_Client *self);

/**
 * @brief Disconnect connection and free all.
 *
 * @param self connector to operate on
 *
 * @return a error string, or NULL if no error happened
 */
char *mc_disconnect(mc_Client *self);

/**
 * @brief Free all data associated with this connector.
 *
 * You have to call mc_disconnect beforehand!
 *
 * @param self the connector to operate on
 */
void mc_free(mc_Client *self);

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
void mc_signal_add(
    mc_Client *self,
    const char *signal_name,
    void *callback_func,
    void *user_data);

/**
 * @brief Same as mc_signal_add, but only
 *
 * The mask param has only effect on the client-event.
 *
 * @param self
 * @param signal_name
 * @param callback_func
 * @param user_data
 * @param mask
 */
void mc_signal_add_masked(
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
void mc_signal_rm(
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
void mc_signal_dispatch(
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
int mc_signal_length(
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
void mc_force_sync(
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
const char *mc_get_host(mc_Client *self);

/**
 * @brief Get the Port being currently connected to.
 *
 * @param self the client to operate on.
 *
 * @return the port or -1 on error.
 */
int mc_get_port(mc_Client *self);

/**
 * @brief Get the currently set timeout.
 *
 * @param self the client to operate on.
 *
 * @return the timeout in seconds, or -1 on error.
 */
int mc_get_timeout(mc_Client *self);

/**
 * @brief Activate a status timer
 *
 * This will cause moosecat to schedule status update every repeat_ms ms.
 * Call mc_status_timer_unregister to deactivate it.
 *
 * This is useful for kbit rate changes which will only be update when
 * an idle event requires it.
 *
 * @param self the client to operate on.
 * @param repeat_ms repeat interval
 * @param trigger_event
 */
void mc_status_timer_register(
    mc_Client *self,
    int repeat_ms,
    bool trigger_event);

/**
 * @brief Deactivate the status_timer
 *
 * @param self the client to operate on.
 */
void mc_status_timer_unregister(
    mc_Client *self);

/**
 * @brief Returns ttue if the status timer is ative
 *
 * @param self the client to operate on.
 *
 * @return false on inactive status timer
 */
bool mc_status_timer_is_active(mc_Client *self);


/**
 * @brief Check if an output is enabled.
 *
 * @param self the client to operate on
 * @param output_name a user defined string of the output (like in mpd.conf)
 *
 * @return -1 on "no such name", 0 if disabled, 1 if enabled
 */
bool mc_outputs_get_state(mc_Client *self, const char *output_name);

/**
 * @brief Get a list of known output names
 *
 * @param data the client to operate on 
 *
 * @return a NULL terminated list of names,
 *         free the list (not the contents) with free() when done
 */
const char ** mc_outputs_get_names(mc_Client *self);

/**
 * @brief Set the state of a Output (enabled or disabled)
 *
 * This can also be accomplished with:
 *
 *    mc_client_send("output_switch horst 1"); 
 *
 *  to enable the output horst. This is just a convinience command.
 *
 * @param data the client to operate on 
 * @param output_name The name of the output to alter
 * @param state the state, True means enabled. 
 *              If nothing would change no command is send.
 *
 * @return True if an output with this name could be found.
 */
bool mc_outputs_set_state(mc_Client *self, const char *output_name, bool state);

/**
 * @brief Lock against modification of the current mpd_status object
 * 
 * You usually would do this in order to get an attribute of an status object.
 * If libmoosecat would lock it on every get there might happen things like:
 *
 *   // implict lock
 *   status.get_id() // 70
 *   // implict unlock
 *   // implict lock
 *   status.get_id() // 71
 *   // implict unlock
 *
 *  Therefore you lock the song explicitly and unlock it again when you're done.
 *
 *  struct mpd_status * status = mc_lock_status(client);
 *  if(status != NULL) {
 *      mpd_status_get_song_id(status);  // 70
 *      mpd_status_get_song_id(status);  // 70
 *  }
 *  mc_unlock_status(client);
 * 
 * Unlock always. Even if status is NULL!
 *
 * This is the only way to get the status object. 
 *
 * @param self the client holding the object
 *
 * @return the locked mpd_status
 */
struct mpd_status * mc_lock_status(mc_Client *self);


/**
 * @brief The pendant to mc_lock_status()
 *
 * See mc_lock_status() for a more detailed description/example.
 *
 * @param self the client holding the status
 */
void mc_unlock_status(mc_Client *self);

/**
 * @brief Lock the current statistics. 
 *
 * See mc_lock_status() for a detailed description.
 *
 * @param self the client that holds the current statistics
 *
 * @return a locked struct mpd_stats
 */
struct mpd_stats * mc_lock_statistics(mc_Client *self);


/**
 * @brief The pendant to mc_lock_statistics()
 *
 * See mc_lock_status() for a more detailed description/example.
 *
 * @param self the client holding the statistics
 */
void mc_unlock_statistics(mc_Client *self);

/**
 * @brief Lock the current song. 
 *
 * See mc_lock_status() for a detailed description.
 *
 * @param self the client that holds the current song
 *
 * @return a locked struct mpd_song
 */
struct mpd_song * mc_lock_current_song(mc_Client *self);

/**
 * @brief The pendant to mc_lock_current_song()
 *
 * See mc_lock_status() for a more detailed description/example.
 *
 * @param self the client holding the current song 
 */
void mc_unlock_current_song(mc_Client *self);

/**
 * @brief Lock the current set of outputs. 
 *
 * See mc_lock_status() for a detailed description.
 *
 * Use mc_outputs_get_names() to acquire a list of names
 *
 * @param self the client that holds the current set of outputs
 */
void mc_lock_outputs(mc_Client *self);

/**
 * @brief The pendant to mc_lock_outputs()
 *
 * See mc_lock_status() for a more detailed description/example.
 *
 * @param self the client holding the current set of outouts
 */
void mc_unlock_outputs(mc_Client *self);


/**
 * @brief Waits for the first sync of status and co.
 *
 * This is useful for debugging purpose, not much for real applications.
 *
 * @param self client to wait upon on
 */
void mc_block_till_sync(mc_Client *self);

#endif /* end of include guard: PROTOCOL_H */
