#ifndef MOOSE_CLIENT_H
#define MOOSE_CLIENT_H


#include <stdbool.h>
#include <glib.h>
#include <mpd/client.h>

#include "moose-song.h"
#include "moose-status.h"

/* Job Dispatcher */
#include "../misc/moose-misc-job-manager.h"

G_BEGIN_DECLS

typedef enum MooseIdle {
    /** song database has been updated*/
    MOOSE_IDLE_DATABASE = MPD_IDLE_DATABASE,

    /** a stored playlist has been modified, created, deleted or renamed */
    MOOSE_IDLE_STORED_PLAYLIST = MPD_IDLE_STORED_PLAYLIST,

    /** the queue has been modified */
    MOOSE_IDLE_QUEUE = MPD_IDLE_QUEUE,

    /** the player state has changed: play, stop, pause, seek, ... */
    MOOSE_IDLE_PLAYER = MPD_IDLE_PLAYER,

    /** the volume has been modified */
    MOOSE_IDLE_MIXER = MPD_IDLE_MIXER,

    /** an audio output device has been enabled or disabled */
    MOOSE_IDLE_OUTPUT = MPD_IDLE_OUTPUT,

    /** options have changed: crossfade, random, repeat, ... */
    MOOSE_IDLE_OPTIONS = MPD_IDLE_OPTIONS,

    /** a database update has started or finished. */
    MOOSE_IDLE_UPDATE = MPD_IDLE_UPDATE,

    /** a sticker has been modified. */
    MOOSE_IDLE_STICKER = MPD_IDLE_STICKER,

    /** a client has subscribed to or unsubscribed from a channel */
    MOOSE_IDLE_SUBSCRIPTION = MPD_IDLE_SUBSCRIPTION,

    /** a message on a subscribed channel was received */
    MOOSE_IDLE_MESSAGE = MPD_IDLE_MESSAGE,

    /** a seek event */
    MOOSE_IDLE_SEEK = MPD_IDLE_MESSAGE << 1,

    MOOSE_IDLE_STATUS_TIMER_FLAG = MPD_IDLE_MESSAGE << 2
} MooseIdle;

typedef enum MooseProtocolType {
    MOOSE_PROTOCOL_IDLE,
    MOOSE_PROTOCOL_COMMAND,
    MOOSE_PROTOCOL_N,
    MOOSE_PROTOCOL_DEFAULT = MOOSE_PROTOCOL_IDLE
} MooseProtocolType;

/*
 * Type macros.
 */
#define MOOSE_TYPE_CLIENT \
    (moose_client_get_type())
#define MOOSE_CLIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_CLIENT, MooseClient))
#define MOOSE_IS_CLIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_CLIENT))
#define MOOSE_CLIENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_CLIENT, MooseClientClass))
#define MOOSE_IS_CLIENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_CLIENT))
#define MOOSE_CLIENT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_CLIENT, MooseClientClass))

GType moose_client_get_type(void);

struct _MooseClientPrivate;

typedef struct _MooseClient {
    GObject parent;
    struct _MooseClientPrivate * priv;

    /*
     * Called on connect, initialize the connector.
     * May not be NULL.
     */
    char *(*do_connect)(struct _MooseClient *, GMainContext * context, const char *, int, float);

    /*
     * Return the command sending connection, made ready to rock.
     * May not be NULL.
     */
    struct mpd_connection *(*do_get)(struct _MooseClient *);

    /*
     * Put the connection back to event listening.
     * May be NULL.
     */
    void (* do_put)(struct _MooseClient *);

    /*
     * Called on disconnect, close all connections, clean up,
     * and make reentrant.
     * May not be NULL.
     */
    bool (* do_disconnect)(struct _MooseClient *);

    /**
     * Check if a valid connection is hold by this connector.
     * May not be NULL.
     */
    bool (* do_is_connected)(struct _MooseClient *);
} MooseClient;

typedef struct _MooseClientClass {
    GObjectClass parent_class;
} MooseClientClass;



/**
 * @brief Create a new client with default properties.
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
MooseClient * moose_client_new(MooseProtocolType protocol);

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
    MooseIdle events);

/**
 * @brief Get the hostname being currently connected to.
 *
 * If not connected, NULL is returned.
 *
 * @param self the client to operate on.
 *
 * @return the hostname, free when done
 */
char * moose_client_get_host(MooseClient * self);

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
 * @brief Deactivate the status_timer
 *
 * @param self the client to operate on.
 */
void moose_client_timer_set_active(MooseClient * self, bool state);

/**
 * @brief Returns ttue if the status timer is ative
 *
 * @param self the client to operate on.
 *
 * @return false on inactive status timer
 */
bool moose_client_timer_get_active(MooseClient * self);

/**
 * @brief
 *
 * @param self
 *
 * @return
 */
MooseStatus * moose_client_ref_status(MooseClient * self);

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

bool moose_client_check_error(MooseClient * self, struct mpd_connection * cconn);
bool moose_client_check_error_without_handling(MooseClient * self, struct mpd_connection * cconn);

struct mpd_connection * moose_base_connect(MooseClient * self, const char * host, int port, float timeout, char ** err);

G_END_DECLS

#endif /* end of include guard: MOOSE_CLIENT_H */
