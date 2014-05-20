#include <stdbool.h>
#include <glib.h>
#include "moose-song.h"
#include "moose-status.h"
#include <mpd/client.h>

/* Signal System */
#include "moose-mpd-signal.h"

/* Job Dispatcher */
#include "../misc/moose-misc-job-manager.h"

#ifndef PROTOCOL_H
#define PROTOCOL_H

typedef enum {
    MOOSE_PM_IDLE = 0,
    MOOSE_PM_COMMAND
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
    char *(*do_connect)(struct MooseClient *, GMainContext * context, const char *, int, float);

    /*
     * Return the command sending connection, made ready to rock.
     * May not be NULL.
     */
    struct mpd_connection *(*do_get)(struct MooseClient *);

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
    char * _host;
    int _port;

    /* Indicates if store was not connected yet.
     * */
    bool is_virgin;

    /* Only used for bug_report.c */
    float _timeout;
    MoosePmType _pm;

    /* Signal functions are stored in here */
    MooseSignalList _signals;

    /* Data Used by the Status/Song/Stats Update Module */
    struct MooseUpdateData * _update_data;

    /* Job Dispatcher */
    struct MooseJobManager * jm;

    struct {
        /* Id of command list job */
        int is_active;
        GMutex is_active_mtx;
        GList * commands;
    } command_list;

    /* The thread moose_client_create() was called from */
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
 * - MOOSE_PM_IDLE - uses one conenction for sending and events, uses idle and noidle to switch context.
 * - MOOSE_PM_COMMAND uses one command connection, and one event listening conenction.
 *
 * Passing NULL to choose the default (MOOSE_PM_COMMAND).
 *
 * @param protocol_machine the name of the protocol machine
 *
 * @return a newly allocated MooseClient, free with moose_client_unref()
 */
MooseClient * moose_client_create(MoosePmType pm);

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
char * moose_client_connect(
    MooseClient * self,
    GMainContext * context,
    const char * host,
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
struct mpd_connection * moose_client_get(MooseClient * self);

/**
 * @brief Put conenction back to event listening
 *
 * @param self the connector to operate on
 */
void moose_client_put(MooseClient * self);

/**
 * @brief Checks if the connector is connected.
 *
 * @param self the connector to operate on
 *
 * @return true when connected
 */
bool moose_client_is_connected(MooseClient * self);

/**
 * @brief Disconnect connection and free all.
 *
 * @param self connector to operate on
 *
 * @return a error string, or NULL if no error happened
 */
char * moose_client_disconnect(MooseClient * self);

/**
 * @brief Free all data associated with this connector.
 *
 * You have to call moose_disconnect beforehand!
 *
 * @param self the connector to operate on
 */
void moose_client_unref(MooseClient * self);

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
void moose_client_signal_add(
    MooseClient * self,
    const char * signal_name,
    void * callback_func,
    void * user_data);

/**
 * @brief Same as moose_client_signal_add, but only
 *
 * The mask param has only effect on the client-event.
 *
 * @param self
 * @param signal_name
 * @param callback_func
 * @param user_data
 * @param mask
 */
void moose_client_signal_add_masked(
    MooseClient * self,
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
void moose_client_signal_rm(
    MooseClient * self,
    const char * signal_name,
    void * callback_addr);

/**
 * @brief This function is mostly used internally to call all registered callbacks.
 *
 * @param self the client to operate on.
 * @param signal_name the name of the signal to dispatch.
 * @param ... variable args. See above.
 */
void moose_client_signal_dispatch(
    MooseClient * self,
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
int moose_client_signal_length(
    MooseClient * self,
    const char * signal_name);

/**
 * @brief Forces the client to update all status/song/stats information.
 *
 * This is useful for testing purpose, where no mainloop is running,
 * and therefore no events are updated.
 *
 * @param self the client to operate on.
 * @param events an eventmask. Pass INT_MAX to update all.
 */
void moose_client_force_sync(
    MooseClient * self,
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
const char * moose_client_get_host(MooseClient * self);

/**
 * @brief Get the Port being currently connected to.
 *
 * @param self the client to operate on.
 *
 * @return the port or -1 on error.
 */
unsigned moose_client_get_port(MooseClient * self);

/**
 * @brief Get the currently set timeout.
 *
 * @param self the client to operate on.
 *
 * @return the timeout in seconds, or -1 on error.
 */
float moose_client_get_timeout(MooseClient * self);

/**
 * @brief Activate a status timer
 *
 * This will cause moosecat to schedule status update every repeat_ms ms.
 * Call moose_client_status_timer_unregister to deactivate it.
 *
 * This is useful for kbit rate changes which will only be update when
 * an idle event requires it.
 *
 * @param self the client to operate on.
 * @param repeat_ms repeat interval
 * @param trigger_event
 */
void moose_client_status_timer_register(
    MooseClient * self,
    int repeat_ms,
    bool trigger_event);

/**
 * @brief Deactivate the status_timer
 *
 * @param self the client to operate on.
 */
void moose_client_status_timer_unregister(
    MooseClient * self);

/**
 * @brief Returns ttue if the status timer is ative
 *
 * @param self the client to operate on.
 *
 * @return false on inactive status timer
 */
bool moose_client_status_timer_is_active(MooseClient * self);

/**
 * @brief 
 *
 * @param self
 *
 * @return 
 */
MooseStatus * moose_client_ref_status(MooseClient * self);

/**
 * @brief Destroy client internal structures.
 *
 * This is called for you by moose_client_unref()
 *
 * @param self the client to operate on
 */
void moose_client_destroy(MooseClient * self);

/**
 * @brief Send a command to the server
 *
 * See HandlerTable
 *
 * @param self the client to operate on
 * @param command the command to send
 *
 * @return an ID you can pass to moose_client_recv
 */
long moose_client_send(MooseClient * self, const char * command);

/**
 * @brief Receive the response of a command
 *
 * There is no way to get the exact response,
 * you can only check for errors.
 *
 * @param self the client to operate on
 * @param job_id the job id to wait for a response
 *
 * @return true on success, false on error
 */
bool moose_client_recv(MooseClient * self, long job_id);

/**
 * @brief Shortcut for send/recv
 *
 * @param self the client to operate on
 * @param command same as with moose_client_send
 *
 * @return same as moose_client_recv
 */
bool moose_client_run(MooseClient * self, const char * command);

/**
 * @brief Check if moose_client_begin() was called, but not moose_client_commit()
 *
 * @param self the client to operate on
 *
 * @return true if active
 */
bool moose_client_command_list_is_active(MooseClient * self);


/**
 * @brief Wait for all operations on this client to finish.
 *
 * This does not include Store Operations.
 *
 * @param self the Client to operate on
 */
void moose_client_wait(MooseClient * self);


/**
 * @brief Begin a new Command List.
 *
 * All following commands are hold back and send at once.
 *
 * Use the ID returned by moose_client_commit() to
 * wait for it, if needed.
 *
 *
 * @param self the client to operate on
 */
void moose_client_begin(MooseClient * self);



/**
 * @brief Commit all previously holdback commands.
 *
 * @param self the Client to operate on
 *
 * @return
 */
long moose_client_commit(MooseClient * self);


#endif /* end of include guard: PROTOCOL_H */
