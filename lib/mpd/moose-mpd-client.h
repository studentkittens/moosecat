#ifndef MOOSE_CLIENT_H
#define MOOSE_CLIENT_H

/**
 * SECTION: moose-mpd-client
 * @short_description: Implements an easy-to-use asynchronous, performant MPD-Client.
 *
 * The #MooseClient is the main entry point to libmoosecat. If features a
 * GObject-based interface to MPD. Clients can connect to one Host/Port at a time.
 * Once a connection (demanded via moose_client_connect_to()) is established, the
 * MooseClient:connectivity signal is called.
 *
 * If the state of the server changes, the 'client-event' is changed with the
 * appropiate event. Once the signal handler is called, it is guaranteed, that
 * a #MooseStatus can be retrieved with moose_client_ref_status().
 *
 * Commands can be asynchronously send to the server via the moose_client_send()
 * method. If you're interested if the command could be executed correctly,
 * you can wait's on it's execution via moose_client_recv, or use
 * moose_client_run in the first place. Normally this is not needed though,
 * since most error handling and logging is done for you.
 *
 * The send-mechanism can be used together with command-lists.
 * A bunch of commands can be queued and executed at once. This is useful in
 * particular when executing many commands (like 'add' at once)
 */

/* External */
#include <glib.h>
#include <mpd/client.h>

/* Internal */
#include "moose-status.h"

G_BEGIN_DECLS

/**
 * MooseIdle:
 *
 * An enumeration of all possible events.
 */
typedef enum MooseIdle {
    /* song database has been updated*/
    MOOSE_IDLE_DATABASE = MPD_IDLE_DATABASE,

    /* a stored playlist has been modified, created, deleted or renamed */
    MOOSE_IDLE_STORED_PLAYLIST = MPD_IDLE_STORED_PLAYLIST,

    /* the queue has been modified */
    MOOSE_IDLE_QUEUE = MPD_IDLE_QUEUE,

    /* the player state has changed: play, stop, pause, seek, ... */
    MOOSE_IDLE_PLAYER = MPD_IDLE_PLAYER,

    /* the volume has been modified */
    MOOSE_IDLE_MIXER = MPD_IDLE_MIXER,

    /* an audio output device has been enabled or disabled */
    MOOSE_IDLE_OUTPUT = MPD_IDLE_OUTPUT,

    /* options have changed: crossfade, random, repeat, ... */
    MOOSE_IDLE_OPTIONS = MPD_IDLE_OPTIONS,

    /* a database update has started or finished. */
    MOOSE_IDLE_UPDATE = MPD_IDLE_UPDATE,

    /* a sticker has been modified. */
    MOOSE_IDLE_STICKER = MPD_IDLE_STICKER,

    /* a client has subscribed to or unsubscribed from a channel */
    MOOSE_IDLE_SUBSCRIPTION = MPD_IDLE_SUBSCRIPTION,

    /* a message on a subscribed channel was received */
    MOOSE_IDLE_MESSAGE = MPD_IDLE_MESSAGE,

    /* a seek event */
    MOOSE_IDLE_SEEK = MPD_IDLE_MESSAGE << 1,

    /* Indicates if an event was triggered by the status timer */
    MOOSE_IDLE_STATUS_TIMER_FLAG = MPD_IDLE_MESSAGE << 2,

    /*< private >*/
    MOOSE_THREAD_TERMINATOR = MPD_IDLE_MESSAGE << 3
} MooseIdle;

#define MOOSE_TYPE_IDLE (moose_idle_get_type())
GType moose_idle_get_type(void);

/////////////////////////

/**
 * MooseProtocol:
 *
 * The Protocols that can be used with moose_client_new()
 */
typedef enum MooseProtocol {
    MOOSE_PROTOCOL_IDLE,
    MOOSE_PROTOCOL_COMMAND,
    MOOSE_PROTOCOL_N,
    MOOSE_PROTOCOL_DEFAULT = MOOSE_PROTOCOL_IDLE
} MooseProtocol;

#define MOOSE_TYPE_PROTOCOL_TYPE
GType moose_protocol_get_type(void);

/////////////////////////

#define MOOSE_TYPE_CLIENT (moose_client_get_type())
#define MOOSE_CLIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_CLIENT, MooseClient))
#define MOOSE_IS_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_CLIENT))
#define MOOSE_CLIENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_CLIENT, MooseClientClass))
#define MOOSE_IS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_CLIENT))
#define MOOSE_CLIENT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_CLIENT, MooseClientClass))

GType moose_client_get_type(void);

struct _MooseClientPrivate;

typedef struct _MooseClient {
    GObject parent;
    struct _MooseClientPrivate *priv;

    /* Called on connect, initialize the connector.
     * May not be NULL.
     */
    char *(*do_connect)(struct _MooseClient *self, const char *host, int port,
                        float timeout);

    /* Return the command sending connection, made ready to rock.
     * May not be NULL.
     */
    struct mpd_connection *(*do_get)(struct _MooseClient *self);

    /* Put the connection back to event listening.
     * May be NULL.
     */
    void (*do_put)(struct _MooseClient *self);

    /* Called on disconnect, close all connections, clean up,
     * and make reentrant.
     * May not be NULL.
     */
    gboolean (*do_disconnect)(struct _MooseClient *self);

    /* Check if a valid connection is hold by this connector.
     * May not be NULL.
     */
    gboolean (*do_is_connected)(struct _MooseClient *self);

    GRecMutex getput_mutex;
} MooseClient;

typedef struct _MooseClientClass { GObjectClass parent_class; } MooseClientClass;

/**
 * moose_client_new:
 * @protocol: a #MooseProtocol
 *
 * Creates a new MooseIdleClient or a MooseCmdClient.
 * Note that you cannot instance a MooseClient itself,
 * since it's an abstract class.
 * Convinience method for either:
 *
 *  g_object_new(MOOSE_TYPE_IDLE_CLIENT, ...):
 *
 * or:
 *
 *  g_object_new(MOOSE_TYPE_CMD_CLIENT, ...):
 *
 *
 * Returns: A newly instanced Client ready for your pleasure.
 */
MooseClient *moose_client_new(MooseProtocol protocol);

/**
 * moose_client_connect_to:
 * @self: a #MooseClient
 * @host: The host we want to connect to. E.g. 'localhost'
 * @port: The port we want to connect to. E.g. 6600
 * @timeout: Max amount of seconds to wait before cancelling a network operation.
 *
 * Connects to the specified server.
 * A 'connectivity' signal is triggered on success.
 *
 * Any Errors will be logged through GLib's logging system.
 *
 * Returns: True on success.
 */
gboolean moose_client_connect_to(MooseClient *self, const char *host, int port,
                                 float timeout);

/**
 * moose_client_get: (skip)
 * @self: a #MooseClient
 *
 * This function locks the internally used connection. You can safely use
 * it with the mpd_run_* functions of libmpdclient.
 * This is for lowlevel support, use moose_client_send() instead.
 *
 * WARNING: For normal users: you're not supposed to call this.
 * You would screw things up if you forget to call moose_client_put().
 * Internally libmoosecat is using several threads to work without blocking.
 *
 * Returns: (transfer none): a libmpdclient connection (which is used internally)
 */
struct mpd_connection *moose_client_get(MooseClient *self);

/**
 * moose_client_put:
 * @self: a #MooseClient
 *
 * Called after moose_client_get() to unlock the connection.
 * Otherwise you'll get deadlocked. You have been warned.
 */
void moose_client_put(MooseClient *self);

/**
 * moose_client_is_connected:
 * @self: a #MooseClient
 *
 * Checks if the client is connected.
 *
 * Returns: True when connected.
 */
gboolean moose_client_is_connected(MooseClient *self);

/**
 * moose_client_disconnect:
 * @self: a #MooseClient
 *
 * Disconnects from the previous server. On successful disconenct
 * a 'connectivity' signal is emitted.
 * That's a no-op if the client is already disconnected.
 *
 * Returns: True on succesful disconenct.
 */
gboolean moose_client_disconnect(MooseClient *self);

/**
 * moose_client_unref:
 * @self: a #MooseClient
 *
 * Same as g_object_unref(), but no-ops on NULL.
 * If reference count drops to 0 the client is disconnected and freed.
 */
void moose_client_unref(MooseClient *self);

/**
 * moose_client_force_sync:
 * @self: a #MooseClient
 * @events: a bitmask of #MooseIdle
 *
 * Forces the client to update all status/song/stats information.
 * This is useful for testing purpose, where no mainloop is running,
 * and therefore no events are updated.
 * Note: You can pass INT_MAX to update them all.
 */
void moose_client_force_sync(MooseClient *self, MooseIdle events);

/**
 * moose_client_get_host:
 * @self: a #MooseClient
 *
 * Returns the current hostname.
 *
 * Returns: the hostname or NULL, free with g_free() when done.
 */
char *moose_client_get_host(MooseClient *self);

/**
 * moose_client_get_port:
 * @self: a #MooseClient
 *
 * Returns the current port being connected to.
 *
 * Returns: the port or 0 if not connected.
 */
unsigned moose_client_get_port(MooseClient *self);

/**
 * moose_client_get_timeout:
 * @self: a #MooseClient
 *
 * Returns the current timeout set.
 *
 * Returns: the timeout or 0 if not connected.
 */
float moose_client_get_timeout(MooseClient *self);

/**
 * moose_client_timer_set_active:
 * @self: a #MooseClient
 * @state: a #gboolean, wether to enable or disable to status timer.
 *
 * The status timer updates the #MooseStatus in a fixed interval or whenever an
 * event occurs. This is useful if you want to display the changing kbit-rate
 * of a song. In any other case, normal event-based updating should be enough.
 *
 * See also the "timer-trigger-event" and "timer-interval" properties.
 */
void moose_client_timer_set_active(MooseClient *self, gboolean state);

/**
 * moose_client_timer_get_active:
 * @self: a #MooseClient
 *
 * Returns: True if the status timer is currently active.
 */
gboolean moose_client_timer_get_active(MooseClient *self);

/**
 * moose_client_ref_status:
 * @self: a #MooseClient
 *
 * Returns the current status of the MPD-Server mirrored by the client.
 * This function always increments the refcount so you can safely use it
 * without any locking. If there is not status (when not connected e.g.)
 * this will return NULL. Therefore you should check it.
 *
 * Returns: (transfer none): a #MooseStatus, unref it when done with g_object_unref()
 */
MooseStatus *moose_client_ref_status(MooseClient *self);

/**
 * moose_client_send:
 * @self: a #MooseClient
 * @command: A command to send. This includes arguments.
 *
 * Examples:
 * ('add', '/')
 * ('crossfade', 0.1)
 *
 * For the exact syntax, see g_variant_parse().
 * Invalid strings will return -1;
 *
 * TODO: write docs.
 *
 * Returns: A unique job-id. It can be used to wait on and fetch the result.
 */
long moose_client_send(MooseClient *self, const char *command);

/**
 * moose_client_send_single:
 * @self: a #MooseClient
 * @command_name: A single command without any arguments.
 *
 * This is a shortcurt. Often used with commands like 'play', 'pause' etc.
 *
 * Returns: A unique job-id. It can be used to wait on and fetch the result.
 */
long moose_client_send_single(MooseClient *self, const char *command_name);

/**
 * moose_client_send_variant:
 * @self: a #MooseClient
 * @variant: a #GVariant that can be used.
 *
 * Returns: A unique job-id. It can be used to wait on and fetch the result.
 */
long moose_client_send_variant(MooseClient *self, GVariant *variant);

/**
 * moose_client_recv:
 * @self: a #MooseClient
 * @job_id: The job_id to wait up-on.
 *
 * Wait upon the finalization of the command started with job_id.
 * The result is only a #gboolean, it's not possible to get the actual
 * response of MPD - this is simply not necessary 99% of the time.
 * The boolean indicates if the command ran successfully.
 *
 * Returns: True if no error happened.
 */
gboolean moose_client_recv(MooseClient *self, long job_id);

/**
 * moose_client_run:
 * @self: a #MooseClient
 * @command: The command to run.
 *
 * Shortcut for moose_client_send() + moose_client_recv()
 * It's usually enough to muse moose_client_send() since it's not important
 * if the command worked.
 *
 * Returns: True if the command ran succesfully.
 */
gboolean moose_client_run(MooseClient *self, const char *command);

/**
 * moose_client_run_single:
 * @self: a #MooseClient
 * @command_name: The single command to run.
 *
 * Send a single command like "pause", this is purely for convinience.
 * (Example: moose_client_run_single(client, "pause"))
 *
 * Returns: True if the command ran succesfully.
 */
gboolean moose_client_run_single(MooseClient *self, const char *command_name);

/**
 * moose_client_run_variant:
 * @self: a #MooseClient
 * @variant: The arguments that shall be executed, with the command-name as
 *           first argument.
 *
 * Returns: True if the command ran succesfully.
 */
gboolean moose_client_run_variant(MooseClient *self, GVariant *variant);

/**
 * moose_client_command_list_is_active:
 * @self: a #MooseClient
 *
 * Returns: a #gboolean; True when moose_client_begin() was called recently.
 */
gboolean moose_client_command_list_is_active(MooseClient *self);

/**
 * moose_client_wait:
 * @self: a #MooseClient
 *
 * Wait for all operations still in queue to be finished.
 * This should be called before finalizing the client.
 * This does not include any operations that are still active on the Store!
 */
void moose_client_wait(MooseClient *self);

/**
 * moose_client_begin:
 * @self: Start the command-list-mode.
 *
 * During command-list-mode it's possible to send commands via
 * moose_client_send() as always, but instead of sending them each individually,
 * they are all send in one bulk. This is useful for some commands that usually
 * come in large bulks, like `add`.
 *
 * The bulk can be send via moose_client_commit().
 */
void moose_client_begin(MooseClient *self);

/**
 * moose_client_commit:
 * @self: a #MooseClient
 *
 * Commit a previously hold back bulk of commands.
 * Ends the command-list-mode.
 *
 * Returns: a job-id that can be used to wait upon it's execution.
 */
long moose_client_commit(MooseClient *self);

G_END_DECLS

#endif /* end of include guard: MOOSE_CLIENT_H */
