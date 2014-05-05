#include <stdbool.h>
#include <glib.h>
#include <mpd/client.h>

/* Signal System */
#include "moose-mpd-signal.h"

/* Job Dispatcher */
#include "../util/job-manager.h"

#ifndef PROTOCOL_H
#define PROTOCOL_H

typedef enum {
    MC_PM_IDLE = 0,
    MC_PM_COMMAND
} MoosePmType;


/* Hack to distinguish between seek and player (pause, play) events.
 * Use (currently...) highest value * 2
 */
#define MPD_IDLE_SEEK MPD_IDLE_MESSAGE << 1

/* Prototype Update struct */
struct MooseUpdateData;
struct MooseOutputsData;


/**
 * @brief Structure representing a connection handle,
 * and an interface to send commands and recv. events.
 *
 * It's able to:
 *    - change hosts (connecting/disconnecting) (without loosing registered callbacks)
 *    - Notifying you on events / errors / connection-changes
 *    - Send commands to the server.
 */
typedef struct MooseClient {
    /*
     * Called on connect, initialize the connector.
     * May not be NULL.
     */
    char *(* do_connect)(struct MooseClient *, GMainContext *context, const char *, int, float);

    /*
     * Return the command sending connection, made ready to rock.
     * May not be NULL.
     */
    struct mpd_connection *(* do_get)(struct MooseClient *);

    /*
     * Put the connection back to event listening.
     * May be NULL.
     */
    void (* do_put)(struct MooseClient *);

    /*
     * Called on disconnect, close all connections, clean up,
     * and make reentrant.
     * May not be NULL.
     */
    bool (* do_disconnect)(struct MooseClient *);

    /**
     * Check if a valid connection is hold by this connector.
     * May not be NULL.
     */
    bool (* do_is_connected)(struct MooseClient *);

    /*
     * Free the connector. disconnect() won't free it!
     * May be NULL
     */
    void (* do_free)(struct MooseClient *);

    /////////////////////////////
    //    Actual Variables     //
    /////////////////////////////

    /* This is locked on do_get(),
     * and unlocked on do_put()
     */
    GRecMutex _getput_mutex;

    GRecMutex _client_attr_mutex;

    /* Save last connected host / port */
    char *_host;
    int _port;

    /* Indicates if store was not connected yet.
     * */
    bool is_virgin;

    /* Only used for bug_report.c */
    float _timeout;
    MoosePmType _pm;

    /* Signal functions are stored in here */
    MooseSignalList _signals;

    /* Outputs of MPD */
    struct MooseOutputsData * _outputs;

    /* Data Used by the Status/Song/Stats Update Module */
    struct MooseUpdateData *_update_data;

    /* Job Dispatcher */
    struct MooseJobManager *jm;

    struct {
        /* Id of command list job */
        int is_active;
        GMutex is_active_mtx;
        GList *commands;
    } command_list;
    
    /* The thread moose_create() was called from */
    GThread * initial_thread;

} MooseClient;

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
 * @return a newly allocated MooseClient, free with moose_free()
 */
MooseClient *moose_create(MoosePmType pm);

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
char *moose_connect(
    MooseClient *self,
    GMainContext *context,
    const char *host,
    int port,
    float timeout
);

/**
 * @brief Return the "send connection" of the Connector.
 *
 * @param self the readily opened connector.
 *
 * @return a working mpd_connection or NULL
 */
struct mpd_connection *moose_get(MooseClient *self);

/**
 * @brief Put conenction back to event listening
 *
 * @param self the connector to operate on
 */
void moose_put(MooseClient *self);

/**
 * @brief Checks if the connector is connected.
 *
 * @param self the connector to operate on
 *
 * @return true when connected
 */
bool moose_is_connected(MooseClient *self);

/**
 * @brief Disconnect connection and free all.
 *
 * @param self connector to operate on
 *
 * @return a error string, or NULL if no error happened
 */
char *moose_disconnect(MooseClient *self);

/**
 * @brief Free all data associated with this connector.
 *
 * You have to call moose_disconnect beforehand!
 *
 * @param self the connector to operate on
 */
void moose_free(MooseClient *self);

///////////////////////////////

/**
 * @brief Add a function that shall be called on a certain event.
 *
 * There are currently 3 valid signal names:
 * - "client-event", See MooseClientEventCallback
 * - "connectivity", See MooseConnectivityCallback
 * - "logging",      See MooseLoggingCallback
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
void moose_signal_add(
    MooseClient *self,
    const char *signal_name,
    void *callback_func,
    void *user_data);

/**
 * @brief Same as moose_signal_add, but only
 *
 * The mask param has only effect on the client-event.
 *
 * @param self
 * @param signal_name
 * @param callback_func
 * @param user_data
 * @param mask
 */
void moose_signal_add_masked(
    MooseClient *self,
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
void moose_signal_rm(
    MooseClient *self,
    const char *signal_name,
    void *callback_addr);

/**
 * @brief This function is mostly used internally to call all registered callbacks.
 *
 * @param self the client to operate on.
 * @param signal_name the name of the signal to dispatch.
 * @param ... variable args. See above.
 */
void moose_signal_dispatch(
    MooseClient *self,
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
int moose_signal_length(
    MooseClient *self,
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
void moose_force_sync(
    MooseClient *self,
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
const char *moose_get_host(MooseClient *self);

/**
 * @brief Get the Port being currently connected to.
 *
 * @param self the client to operate on.
 *
 * @return the port or -1 on error.
 */
unsigned moose_get_port(MooseClient *self);

/**
 * @brief Get the currently set timeout.
 *
 * @param self the client to operate on.
 *
 * @return the timeout in seconds, or -1 on error.
 */
float moose_get_timeout(MooseClient *self);

/**
 * @brief Activate a status timer
 *
 * This will cause moosecat to schedule status update every repeat_ms ms.
 * Call moose_status_timer_unregister to deactivate it.
 *
 * This is useful for kbit rate changes which will only be update when
 * an idle event requires it.
 *
 * @param self the client to operate on.
 * @param repeat_ms repeat interval
 * @param trigger_event
 */
void moose_status_timer_register(
    MooseClient *self,
    int repeat_ms,
    bool trigger_event);

/**
 * @brief Deactivate the status_timer
 *
 * @param self the client to operate on.
 */
void moose_status_timer_unregister(
    MooseClient *self);

/**
 * @brief Returns ttue if the status timer is ative
 *
 * @param self the client to operate on.
 *
 * @return false on inactive status timer
 */
bool moose_status_timer_is_active(MooseClient *self);


/**
 * @brief Check if an output is enabled.
 *
 * @param self the client to operate on
 * @param output_name a user defined string of the output (like in mpd.conf)
 *
 * @return -1 on "no such name", 0 if disabled, 1 if enabled
 */
bool moose_outputs_get_state(MooseClient *self, const char *output_name);

/**
 * @brief Get a list of known output names
 *
 * @param data the client to operate on 
 *
 * @return a NULL terminated list of names,
 *         free the list (not the contents) with free() when done
 */
const char ** moose_outputs_get_names(MooseClient *self);

/**
 * @brief Set the state of a Output (enabled or disabled)
 *
 * This can also be accomplished with:
 *
 *    moose_client_send("output_switch horst 1"); 
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
bool moose_outputs_set_state(MooseClient *self, const char *output_name, bool state);

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
 *  struct mpd_status * status = moose_lock_status(client);
 *  if(status != NULL) {
 *      mpd_status_get_song_id(status);  // 70
 *      mpd_status_get_song_id(status);  // 70
 *  }
 *  moose_unlock_status(client);
 * 
 * Unlock always. Even if status is NULL!
 *
 * This is the only way to get the status object. 
 *
 * The status lock is also used for moose_get_replay_gain_status()
 *
 * @param self the client holding the object
 *
 * @return the locked mpd_status
 */
struct mpd_status * moose_lock_status(MooseClient *self);


/**
 * @brief Get the current replay gain mode ("album", "track", "auto", "off")
 *
 * Before calling this you should call moose_lock_status()
 *
 * @param self the client to get the replay_gain_mode from .
 *
 * @return a const string. Do not free.
 */
const char * moose_get_replay_gain_mode(MooseClient * self);

/**
 * @brief The pendant to moose_lock_status()
 *
 * See moose_lock_status() for a more detailed description/example.
 *
 * @param self the client holding the status
 */
void moose_unlock_status(MooseClient *self);

/**
 * @brief Lock the current statistics. 
 *
 * See moose_lock_status() for a detailed description.
 *
 * @param self the client that holds the current statistics
 *
 * @return a locked struct mpd_stats
 */
struct mpd_stats * moose_lock_statistics(MooseClient *self);


/**
 * @brief The pendant to moose_lock_statistics()
 *
 * See moose_lock_status() for a more detailed description/example.
 *
 * @param self the client holding the statistics
 */
void moose_unlock_statistics(MooseClient *self);

/**
 * @brief Lock the current song. 
 *
 * See moose_lock_status() for a detailed description.
 *
 * @param self the client that holds the current song
 *
 * @return a locked struct mpd_song
 */
struct mpd_song * moose_lock_current_song(MooseClient *self);

/**
 * @brief The pendant to moose_lock_current_song()
 *
 * See moose_lock_status() for a more detailed description/example.
 *
 * @param self the client holding the current song 
 */
void moose_unlock_current_song(MooseClient *self);

/**
 * @brief Lock the current set of outputs. 
 *
 * See moose_lock_status() for a detailed description.
 *
 * Use moose_outputs_get_names() to acquire a list of names
 *
 * @param self the client that holds the current set of outputs
 */
void moose_lock_outputs(MooseClient *self);

/**
 * @brief The pendant to moose_lock_outputs()
 *
 * See moose_lock_status() for a more detailed description/example.
 *
 * @param self the client holding the current set of outouts
 */
void moose_unlock_outputs(MooseClient *self);


/**
 * @brief Waits for the first sync of status and co.
 *
 * This is useful for debugging purpose, not much for real applications.
 * Will only block if connected.
 *
 * @param self client to wait upon on
 */
void moose_block_till_sync(MooseClient *self);

#endif /* end of include guard: PROTOCOL_H */
