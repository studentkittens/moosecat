#include "../moose-config.h"

#include "moose-mpd-client.h"
#include "pm/moose-mpd-cmnd-core.h"
#include "pm/moose-mpd-idle-core.h"

#include "moose-mpd-client.h"
#include "moose-status-private.h"

/* memset() */
#include <string.h>

#define ASSERT_IS_MAINTHREAD(client) \
    g_assert(g_thread_self() == (client)->priv->initial_thread)

/* This will be sended (as Integer)
 * to the Queue to break out of the poll loop
 */
#define THREAD_TERMINATOR (MOOSE_IDLE_STICKER)

enum {
    SIGNAL_CLIENT_EVENT,
    SIGNAL_CONNECTIVITY,
    SIGNAL_LOGGING,
    SIGNAL_FATAL_ERROR,
    NUM_SIGNALS
};

enum {
    PROP_HOST,
    PROP_PORT,
    PROP_TIMEOUT,
    PROP_TIMER_INTERVAL,
    PROP_TIMER_TRIGGER_EVENT,
    PROP_NUMBER
};

static guint SIGNALS[NUM_SIGNALS];

static void * moose_client_command_dispatcher(
    struct MooseJobManager * jm,
    volatile bool * cancel,
    void * user_data,
    void * job_data
);

static gpointer moose_update_thread(gpointer user_data);
static void moose_update_data_push(MooseClient * self, MooseIdle event);
static gboolean moose_update_status_timer_cb(gpointer user_data);

typedef struct _MooseClientPrivate {
    /* This is locked on do_get(),
     * and unlocked on do_put()
     */
    GRecMutex getput_mutex;
    GRecMutex client_attr_mutex;

    /* The thread moose_client_new(MOOSE_PROTOCOL_IDLE) was called from */
    GThread * initial_thread;

    /* Save last connected host / port */
    char * host;
    int port;
    float timeout;

    /* Indicates if store was not connected yet.
     * */
    bool is_virgin;

    /* Job Dispatcher */
    struct MooseJobManager * jm;

    struct {
        /* Id of command list job */
        int is_active;
        GList * commands;
    } command_list;

    GAsyncQueue * event_queue;
    GThread * update_thread;
    MooseStatus * status;

    struct {
        int timeout_id;
        int interval;
        bool trigger_event;
        GTimer * last_update;
        GMutex mutex;
    } status_timer;

    /* ID of the last played song or -1
     * Needed to distinguish between
     * MOOSE_IDLE_SEEK and MOOSE_IDLE_PLAYER.
     * */
    struct {
        long id;
        MooseState state;
    } last_song_data;

    MooseProtocolType protocol;
} MooseClientPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(
    MooseClient, moose_client, G_TYPE_OBJECT
);

////  PUBLIC //////

static void moose_client_init(MooseClient * self) {
    self->priv = moose_client_get_instance_private(self);

    if (self != NULL) {
        self->priv->is_virgin = true;

        /* init the getput mutex */
        g_rec_mutex_init(&self->priv->getput_mutex);
        g_rec_mutex_init(&self->priv->client_attr_mutex);

        self->priv->event_queue = g_async_queue_new();

        g_mutex_init(&self->priv->status_timer.mutex);

        self->priv->last_song_data.id = -1;
        self->priv->last_song_data.state = MOOSE_STATE_UNKNOWN;

        self->priv->update_thread = g_thread_new(
                                        "data-update-thread",
                                        moose_update_thread,
                                        self
                                    );
        self->priv->initial_thread = g_thread_self();
    }
}

MooseClient * moose_client_new(MooseProtocolType protocol) {
    switch (protocol) {
    case MOOSE_PROTOCOL_COMMAND:
        return g_object_new(MOOSE_TYPE_CMD_CLIENT, NULL);
    case MOOSE_PROTOCOL_IDLE:
    default:
        return g_object_new(MOOSE_TYPE_CMD_CLIENT, NULL);
    }
}

static void moose_client_finalize(GObject * gobject) {
    MooseClient * self = MOOSE_CLIENT(gobject);
    if (self == NULL) {
        return;
    }

    ASSERT_IS_MAINTHREAD(self);

    MooseClientPrivate * priv = self->priv;

    /* Disconnect if not done yet */
    moose_client_disconnect(self);

    if (moose_client_timer_get_active(self)) {
        moose_client_timer_set_active(self, false);
    }

    /* Push the termination sign to the Queue */
    g_async_queue_push(priv->event_queue, GINT_TO_POINTER(THREAD_TERMINATOR));

    /* Wait for the thread to finish */
    g_thread_join(priv->update_thread);

    if (priv->status != NULL) {
        moose_status_unref(priv->status);
        priv->status = NULL;
    }

    /* Destroy the Queue */
    g_async_queue_unref(priv->event_queue);

    priv->event_queue = NULL;
    priv->update_thread = NULL;

    g_mutex_clear(&priv->status_timer.mutex);

    /* Kill any previously connected host info */
    g_rec_mutex_lock(&priv->client_attr_mutex);
    if (priv->host != NULL) {
        g_free(priv->host);
        priv->host = NULL;
    }

    g_rec_mutex_unlock(&priv->client_attr_mutex);
    g_rec_mutex_clear(&priv->getput_mutex);
    g_rec_mutex_clear(&priv->client_attr_mutex);

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

static void moose_client_get_property(
    GObject * object,
    guint property_id,
    GValue * value,
    GParamSpec * pspec) {
    MooseClient * self = MOOSE_CLIENT(object);

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    switch (property_id) {
    case PROP_HOST:
        g_value_set_string(value, self->priv->host);
        break;
    case PROP_PORT:
        g_value_set_int(value, self->priv->port);
        break;
    case PROP_TIMEOUT:
        g_value_set_float(value, self->priv->timeout);
        break;
    case PROP_TIMER_INTERVAL:
        g_value_set_float(value, self->priv->status_timer.interval * 1000);
        break;
    case PROP_TIMER_TRIGGER_EVENT:
        g_value_set_boolean(value, self->priv->status_timer.trigger_event);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);
}

static void moose_client_set_property(
    GObject * object,
    guint property_id,
    const GValue * value,
    GParamSpec * pspec) {
    MooseClient * self = MOOSE_CLIENT(object);

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    switch (property_id) {
    case PROP_HOST:
        g_free(self->priv->host);
        self->priv->host = g_value_dup_string(value);
        break;
    case PROP_PORT:
        self->priv->port = g_value_get_int(value);
        break;
    case PROP_TIMEOUT:
        self->priv->timeout = g_value_get_float(value);
        break;
    case PROP_TIMER_INTERVAL:
        self->priv->status_timer.interval = g_value_get_float(value) * 1000;
        break;
    case PROP_TIMER_TRIGGER_EVENT:
        self->priv->status_timer.trigger_event = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);
}

static void moose_client_class_init(MooseClientClass * klass) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_client_finalize;
    gobject_class->get_property = moose_client_get_property;
    gobject_class->set_property = moose_client_set_property;

    SIGNALS[SIGNAL_CLIENT_EVENT] = g_signal_new(
                                       "client-event",
                                       G_OBJECT_CLASS_TYPE(klass),
                                       G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                                       g_cclosure_marshal_VOID__ENUM,
                                       G_TYPE_NONE, 2, G_TYPE_ENUM, G_TYPE_POINTER
                                   );
    SIGNALS[SIGNAL_CONNECTIVITY] = g_signal_new(
                                       "connectivity",
                                       G_OBJECT_CLASS_TYPE(klass),
                                       G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                                       g_cclosure_marshal_VOID__BOOLEAN,
                                       G_TYPE_NONE, 2, G_TYPE_BOOLEAN, G_TYPE_POINTER
                                   );

    GParamSpec * pspec = NULL;
    pspec = g_param_spec_string(
                "host",
                "Hostname",
                "Current host we're connected to",
                NULL, /* default value */
                G_PARAM_READABLE
            );

    /**
     * MooseClient:host: (type utf8)
     *
     * Hostname of the MPD Server we're connected to or NULL.
     */
    g_object_class_install_property(gobject_class, PROP_HOST, pspec);

    pspec = g_param_spec_int(
                "port",
                "Port",
                "Port we're connected to",
                0,        /* Minimum value */
                G_MAXINT, /* Maximum value */
                6600,     /* default value */
                G_PARAM_READABLE
            );

    /**
     * MooseClient:port: (type int)
     *
     * Port of the MPD Server being connected to. Default: 6600
     */
    g_object_class_install_property(gobject_class, PROP_PORT, pspec);

    pspec = g_param_spec_float(
                "timeout",
                "Timeout",
                "Max. amount of time to wait (in seconds) before cancelling the operation",
                1,    /* Minimum value */
                100,  /* Maximum value */
                10,   /* Default value */
                G_PARAM_READWRITE
            );

    /**
     * MooseClient:timeout: (type float)
     *
     * Timeout in seconds, after which an operation is cancelled.
     */
    g_object_class_install_property(gobject_class, PROP_TIMEOUT, pspec);

    pspec = g_param_spec_float(
                "timer-interval",
                "Timerinterval",
                "How many seconds to wait before retrieving the next status",
                0.1,  /* Minimum value */
                100,  /* Maximum value */
                0.5,  /* Default value */
                G_PARAM_READWRITE
            );

    /**
     * MooseClient:timer-interval: (type float)
     *
     * Timer Interval, after which the status is force-updated.
     */
    g_object_class_install_property(gobject_class, PROP_TIMER_INTERVAL, pspec);

    pspec = g_param_spec_boolean(
                "timer-trigger",
                "Trigger event",
                "Emit a client-event when force-updating the Status?",
                TRUE,
                G_PARAM_READWRITE
            );

    /**
     * MooseClient:timer-interval: (type float)
     *
     * Timer Interval, after which the status is force-updated.
     */
    g_object_class_install_property(gobject_class, PROP_TIMER_TRIGGER_EVENT, pspec);
}

static void moose_report_connectivity(MooseClient * self, const char * new_host, int new_port, float new_timeout) {

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    bool server_changed =
        self->priv->is_virgin == false &&
        ((g_strcmp0(new_host, self->priv->host) != 0) || (new_port != self->priv->port));

    /* Defloreate the Client (Wheeeh!) */
    self->priv->is_virgin = false;

    if (self->priv->host != NULL) {
        g_free(self->priv->host);
    }

    self->priv->host = g_strdup(new_host);
    self->priv->port = new_port;
    self->priv->timeout = new_timeout;
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);

    /* Dispatch *after* host being set */
    g_signal_emit(self, SIGNALS[SIGNAL_CONNECTIVITY], 0, server_changed);
}

static char * moose_compose_error_msg(const char * topic, const char * src) {
    if (src && topic) {
        return g_strdup_printf("%s: ,,%s''", topic, src);
    } else {
        return NULL;
    }
}

static bool moose_client_check_error_impl(MooseClient * self, struct mpd_connection * cconn, bool handle_fatal) {
    if (self == NULL || cconn == NULL) {
        if (self != NULL) {
            moose_warning("Connection to MPD was closed (IsFatal=maybe?)");
        }

        return true;
    }

    char * error_message = NULL;
    enum mpd_error error = mpd_connection_get_error(cconn);

    if (error != MPD_ERROR_SUCCESS) {
        switch (error) {
        case MPD_ERROR_SYSTEM:
            error_message = moose_compose_error_msg(
                                "System-Error",
                                g_strerror(mpd_connection_get_system_error(cconn))
                            );
            break;
        case MPD_ERROR_SERVER:
            error_message = moose_compose_error_msg(
                                "Server-Error",
                                mpd_connection_get_error_message(cconn)
                            );
            break;
        default:
            error_message = moose_compose_error_msg(
                                "Client-Error",
                                mpd_connection_get_error_message(cconn)
                            );
            break;
        }

        /* Try to clear the error */
        bool is_fatal = !(mpd_connection_clear_error(cconn));

        /* On really fatal error we better disconnect,
         * than using an invalid connection */
        if (handle_fatal && is_fatal) {
            moose_warning("That was fatal. Disconnecting on mainthread.");
            g_idle_add((GSourceFunc)moose_client_disconnect, self);
        }

        /* Dispatch the error to the users */
        moose_critical(
            "ErrID=%d: %s (IsFatal=%s)",
            error, error_message, is_fatal ? "True" : "False"
        );
        g_free(error_message);
        return true;
    }

    return false;
}

bool moose_client_check_error(MooseClient * self, struct mpd_connection * cconn) {
    return moose_client_check_error_impl(self, cconn, true);
}

bool moose_client_check_error_without_handling(MooseClient * self, struct mpd_connection * cconn) {
    return moose_client_check_error_impl(self, cconn, false);
}

char * moose_client_connect(
    MooseClient * self,
    GMainContext * context,
    const char * host,
    int port,
    float timeout) {
    char * err = NULL;

    if (self == NULL) {
        return g_strdup("Client is null");
    }

    if (moose_client_is_connected(self)) {
        return g_strdup("Already connected.");
    }

    ASSERT_IS_MAINTHREAD(self);

    self->priv->jm = moose_jm_create(moose_client_command_dispatcher, self);

    moose_message("Attempting to connect…");

    /* Actual implementation of the connection in respective protcolmachine */
    err = g_strdup(self->do_connect(self, context, host, port, ABS(timeout)));

    if (err == NULL && moose_client_is_connected(self)) {
        /* Force updating of status/stats/song on connect */
        moose_client_force_sync(self, INT_MAX);

        /* Check if server changed */
        moose_report_connectivity(self, host, port, timeout);

        moose_message("...Fully connected!");
    }
    return err;
}

bool moose_client_is_connected(MooseClient * self) {
    g_assert(self);

    bool result = (self && self->do_is_connected(self));

    return result;
}

void moose_client_put(MooseClient * self) {
    g_assert(self);

    /* Put connection back to event listening. */
    if (moose_client_is_connected(self) && self->do_put != NULL) {
        self->do_put(self);
    }

    /* Make the connection accesible to other threads */
    g_rec_mutex_unlock(&self->priv->getput_mutex);
}

struct mpd_connection * moose_client_get(MooseClient * self) {
    g_assert(self);

    /* Return the readily sendable connection */
    struct mpd_connection * cconn = NULL;

    /* lock the connection to make sure, only one thread
     * can use it. This prevents us from relying on
     * the user to lock himself. */
    g_rec_mutex_lock(&self->priv->getput_mutex);

    if (moose_client_is_connected(self)) {
        cconn = self->do_get(self);
        moose_client_check_error(self, cconn);
    }

    return cconn;
}

char * moose_client_disconnect(
    MooseClient * self) {
    if (self && moose_client_is_connected(self)) {

        ASSERT_IS_MAINTHREAD(self);

        bool error_happenend = false;
        MooseClientPrivate * priv = self->priv;

        /* Lock the connection while destroying it */
        g_rec_mutex_lock(&priv->getput_mutex);
        {
            /* Finish current running command */
            moose_jm_close(priv->jm);
            priv->jm = NULL;

            /* let the connector clean up itself */
            error_happenend = !self->do_disconnect(self);

            /* Reset status/song/stats to NULL */
            if (priv->status != NULL) {
                moose_status_unref(priv->status);
            }
            priv->status = NULL;

            /* Notify user of the disconnect */
            g_signal_emit(self, SIGNALS[SIGNAL_CONNECTIVITY], 0, false);

            /* Unlock the mutex - we can use it now again
             * e.g. - queued commands would wake up now
             *        and notice they are not connected anymore
             */
        }
        g_rec_mutex_unlock(&priv->getput_mutex);

        // TODO
        if (error_happenend) {
            return g_strdup("Unknown Error.");
        } else {
            return NULL;
        }
    }

    return (self == NULL) ? g_strdup("Client is null") : NULL;
}

void moose_client_unref(MooseClient * self) {
    if (self != NULL) {
        g_object_unref(self);
    }
}

void moose_client_force_sync(
    MooseClient * self,
    MooseIdle events) {
    g_assert(self);
    moose_update_data_push(self, events);
}

char * moose_client_get_host(MooseClient * self) {
    g_assert(self);

    char * host = NULL;
    g_object_get(G_OBJECT(self), "host", &host, NULL);
    return host;
}

unsigned moose_client_get_port(MooseClient * self) {
    unsigned port;
    g_object_get(G_OBJECT(self), "port", &port, NULL);
    return port;
}

float moose_client_get_timeout(MooseClient * self) {
    float timeout;
    g_object_get(G_OBJECT(self), "timeout", &timeout, NULL);
    return timeout;
}

bool moose_client_timer_get_active(MooseClient * self) {
    g_assert(self);

    g_mutex_lock(&self->priv->status_timer.mutex);
    bool result = (self->priv->status_timer.timeout_id != -1);
    g_mutex_unlock(&self->priv->status_timer.mutex);

    return result;
}

// TODO Fix status timer.
void moose_client_timer_set_active(MooseClient * self, bool state) {
    g_assert(self);

    if(state == false) {
        if (moose_client_timer_get_active(self)) {
            MooseClientPrivate * priv = self->priv;
            g_mutex_lock(&priv->status_timer.mutex);

            if (priv->status_timer.timeout_id > 0) {
                g_source_remove(priv->status_timer.timeout_id);
            }

            priv->status_timer.timeout_id = -1;
            priv->status_timer.interval = 0;

            if (priv->status_timer.last_update != NULL) {
                g_timer_destroy(priv->status_timer.last_update);
            }
            g_mutex_unlock(&priv->status_timer.mutex);
        }
    } else {
        float timeout = 0.5;
        g_object_get(self, "timer-interval", &timeout, NULL);

        g_mutex_lock(&self->priv->status_timer.mutex);
        MooseClientPrivate * priv = self->priv;
        priv->status_timer.last_update = g_timer_new();
        priv->status_timer.timeout_id =
            g_timeout_add(timeout * 1000, moose_update_status_timer_cb, self);
        g_mutex_unlock(&self->priv->status_timer.mutex);
    }
}

MooseStatus * moose_client_ref_status(MooseClient * self) {
    MooseStatus * status = self->priv->status;
    if (status != NULL) {
        g_object_ref(status);
    }
    return status;
}

//                                 //
//         CLIENT COMMANDS         //
//                                 //

static bool moose_client_command_list_begin(MooseClient * self);
static void moose_client_command_list_append(MooseClient * self, const char * command);
static bool moose_client_command_list_commit(MooseClient * self);

//                                  //
//     Client Command Handlers      //
//                                  //

/**
 * Missing commands:
 *
 *  shuffle_range
 *  (...)
 */
#define COMMAND(_getput_code_, _command_list_code_) \
    if (moose_client_command_list_is_active(self)) {    \
        _command_list_code_;                        \
    } else {                                        \
        _getput_code_;                              \
    }                                               \
 

#define intarg(argument, result)                                   \
    {                                                                  \
        char * endptr = NULL;                                           \
        result = g_ascii_strtoll(argument, &endptr, 10);               \
                                                                   \
        if (endptr != NULL && *endptr != '\0') {                        \
            moose_warning("Could not convert to int: ''%s''", argument);  \
            return false;                                              \
        }                                                              \
    }                                                                  \
 
#define intarg_named(argument, name) \
    int name = 0;                    \
    intarg(argument, name);          \
 

#define floatarg(argument, result)                                       \
    {                                                                    \
        char * endptr = NULL;                                            \
        result = g_ascii_strtod(argument, &endptr);                      \
                                                                         \
        if (endptr != NULL && *endptr != '\0') {                         \
            moose_warning("Could not convert to float: ''%s''", argument);  \
            return false;                                                \
        }                                                                \
    }                                                                    \
 
#define floatarg_named(argument, name) \
    double name = 0;                   \
    floatarg(argument, name);          \
 

static bool handle_queue_add(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * uri = argv[0];
    COMMAND(
        mpd_run_add(conn, uri),
        mpd_send_add(conn, uri)
    )

    return true;
}

static bool handle_queue_clear(MooseClient * self, struct mpd_connection * conn, G_GNUC_UNUSED const char ** argv) {
    COMMAND(
        mpd_run_clear(conn),
        mpd_send_clear(conn)
    )

    return true;
}

static bool handle_consume(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], mode);
    COMMAND(
        mpd_run_consume(conn, mode),
        mpd_send_consume(conn, mode)
    )

    return true;
}

static bool handle_crossfade(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    floatarg_named(argv[0], mode);
    COMMAND(
        mpd_run_crossfade(conn, mode),
        mpd_send_crossfade(conn, mode)
    )

    return true;
}

static bool handle_queue_delete(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], pos);
    COMMAND(
        mpd_run_delete(conn, pos),
        mpd_send_delete(conn, pos)
    )

    return true;
}

static bool handle_queue_delete_id(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], id);
    COMMAND(
        mpd_run_delete_id(conn, id),
        mpd_send_delete_id(conn, id)
    )

    return true;
}

static bool handle_queue_delete_range(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], start);
    intarg_named(argv[1], end);
    COMMAND(
        mpd_run_delete_range(conn, start, end),
        mpd_send_delete_range(conn, start, end)
    )

    return true;
}

static bool handle_output_switch(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    // const char * output_name = argv[0];
    // int output_id = moose_priv_outputs_name_to_id(self->outputs, output_name);
    //
    // TODO
    int output_id = 0;

    intarg_named(argv[1], mode);

    if (output_id != -1) {
        if (mode == TRUE) {
            COMMAND(
                mpd_run_enable_output(conn, output_id),
                mpd_send_enable_output(conn, output_id)
            );
        } else {
            COMMAND(
                mpd_run_disable_output(conn, output_id),
                mpd_send_disable_output(conn, output_id)
            );
        }

        return true;
    } else {
        return false;
    }
}

static bool handle_playlist_load(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * playlist = argv[0];
    COMMAND(
        mpd_run_load(conn, playlist),
        mpd_send_load(conn, playlist)
    );

    return true;
}

static bool handle_mixramdb(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    floatarg_named(argv[0], decibel);
    COMMAND(
        mpd_run_mixrampdb(conn, decibel),
        mpd_send_mixrampdb(conn, decibel)
    );

    return true;
}

static bool handle_mixramdelay(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    floatarg_named(argv[0], seconds);
    COMMAND(
        mpd_run_mixrampdelay(conn, seconds),
        mpd_send_mixrampdelay(conn, seconds)
    );

    return true;
}

static bool handle_queue_move(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], old_id);
    intarg_named(argv[1], new_id);
    COMMAND(
        mpd_run_move_id(conn, old_id, new_id),
        mpd_send_move_id(conn, old_id, new_id)
    );

    return true;
}

static bool handle_queue_move_range(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], start_pos);
    intarg_named(argv[1], end_pos);
    intarg_named(argv[2], new_pos);
    COMMAND(
        mpd_run_move_range(conn, start_pos, end_pos, new_pos),
        mpd_send_move_range(conn, start_pos, end_pos, new_pos)
    );

    return true;
}

static bool handle_next(MooseClient * self, struct mpd_connection * conn, G_GNUC_UNUSED const char ** argv) {
    COMMAND(
        mpd_run_next(conn),
        mpd_send_next(conn)
    )

    return true;
}

static bool handle_password(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * password = argv[0];
    bool rc = false;
    COMMAND(
        rc = mpd_run_password(conn, password),
        mpd_send_password(conn, password)
    );
    return rc;
}

static bool handle_pause(MooseClient * self, struct mpd_connection * conn, G_GNUC_UNUSED const char ** argv) {
    COMMAND(
        mpd_run_toggle_pause(conn),
        mpd_send_toggle_pause(conn)
    )

    return true;
}

static bool handle_play(MooseClient * self, struct mpd_connection * conn, G_GNUC_UNUSED const char ** argv) {
    COMMAND(
        mpd_run_play(conn),
        mpd_send_play(conn)
    )

    return true;
}

static bool handle_play_id(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], id);
    COMMAND(
        mpd_run_play_id(conn, id),
        mpd_send_play_id(conn, id)
    )

    return true;
}

static bool handle_playlist_add(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * name = argv[0];
    const char * file = argv[1];
    COMMAND(
        mpd_run_playlist_add(conn, name, file),
        mpd_send_playlist_add(conn, name, file)
    )

    return true;
}

static bool handle_playlist_clear(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * name = argv[0];
    COMMAND(
        mpd_run_playlist_clear(conn, name),
        mpd_send_playlist_clear(conn, name)
    )

    return true;
}

static bool handle_playlist_delete(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * name = argv[0];
    intarg_named(argv[1], pos);
    COMMAND(
        mpd_run_playlist_delete(conn, name, pos),
        mpd_send_playlist_delete(conn, name, pos)
    )

    return true;
}

static bool handle_playlist_move(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * name = argv[0];
    intarg_named(argv[1], old_pos);
    intarg_named(argv[2], new_pos);
    COMMAND(
        mpd_send_playlist_move(conn, name, old_pos, new_pos);
        mpd_response_finish(conn);
        ,
        mpd_send_playlist_move(conn, name, old_pos, new_pos);
    )

    return true;
}

static bool handle_previous(MooseClient * self, struct mpd_connection * conn, G_GNUC_UNUSED const char ** argv) {
    COMMAND(
        mpd_run_previous(conn),
        mpd_send_previous(conn)
    )

    return true;
}

static bool handle_prio(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], prio);
    intarg_named(argv[1], position);

    COMMAND(
        mpd_run_prio(conn,  prio, position),
        mpd_send_prio(conn, prio, position)
    )

    return true;
}

static bool handle_prio_range(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], prio);
    intarg_named(argv[1], start_pos);
    intarg_named(argv[2], end_pos);

    COMMAND(
        mpd_run_prio_range(conn,  prio, start_pos, end_pos),
        mpd_send_prio_range(conn, prio, start_pos, end_pos)
    )

    return true;
}

static bool handle_prio_id(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], prio);
    intarg_named(argv[1], id);

    COMMAND(
        mpd_run_prio_id(conn,  prio, id),
        mpd_send_prio_id(conn, prio, id)
    )

    return true;
}

static bool handle_random(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], mode);

    COMMAND(
        mpd_run_random(conn, mode),
        mpd_send_random(conn, mode)
    )

    return true;
}

static bool handle_playlist_rename(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * old_name = argv[0];
    const char * new_name = argv[1];

    COMMAND(
        mpd_run_rename(conn, old_name, new_name),
        mpd_send_rename(conn, old_name, new_name)
    );
    return true;
}

static bool handle_repeat(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], mode);

    COMMAND(
        mpd_run_repeat(conn, mode),
        mpd_send_repeat(conn, mode)
    );

    return true;
}

static bool handle_replay_gain_mode(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], replay_gain_mode);

    COMMAND(
        mpd_send_command(conn, "replay_gain_mode", replay_gain_mode, NULL);
        mpd_response_finish(conn);
        , /* send command */
        mpd_send_command(conn, "replay_gain_mode", replay_gain_mode, NULL);
    )

    return true;
}

static bool handle_database_rescan(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * path = argv[0];
    COMMAND(
        mpd_run_rescan(conn, path),
        mpd_send_rescan(conn, path)
    );

    return true;
}

static bool handle_playlist_rm(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * playlist_name = argv[0];
    COMMAND(
        mpd_run_rm(conn, playlist_name),
        mpd_send_rm(conn, playlist_name)
    );

    return true;
}

static bool handle_playlist_save(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * as_name = argv[0];
    COMMAND(
        mpd_run_save(conn, as_name),
        mpd_send_save(conn, as_name)
    );

    return true;
}

static bool handle_seek(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], pos);
    floatarg_named(argv[1], seconds);

    COMMAND(
        mpd_run_seek_pos(conn, pos, seconds),
        mpd_send_seek_pos(conn, pos, seconds)
    );

    return true;
}

static bool handle_seek_id(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], id);
    floatarg_named(argv[1], seconds);

    COMMAND(
        mpd_run_seek_id(conn, id, seconds),
        mpd_send_seek_id(conn, id, seconds)
    );

    return true;
}

static bool handle_seekcur(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    floatarg_named(argv[0], seconds);

    /* there is 'seekcur' in newer mpd versions,
     * but we can emulate it easily */
    if (moose_client_is_connected(self)) {
        int curr_id = 0;

        MooseStatus * status = moose_client_ref_status(self);
        MooseSong * current_song = moose_status_get_current_song(status);
        moose_status_unref(status);

        if (current_song != NULL) {
            curr_id = moose_song_get_id(current_song);
            moose_song_unref(current_song);
        } else {
            moose_song_unref(current_song);
            return false;
        }

        COMMAND(
            mpd_run_seek_id(conn, curr_id, seconds),
            mpd_send_seek_id(conn, curr_id, seconds)
        )
    }

    return true;
}

static bool handle_setvol(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], volume);
    COMMAND(
        mpd_run_set_volume(conn, volume),
        mpd_send_set_volume(conn, volume)
    );

    return true;
}

static bool handle_queue_shuffle(MooseClient * self, struct mpd_connection * conn, G_GNUC_UNUSED const char ** argv) {
    COMMAND(
        mpd_run_shuffle(conn),
        mpd_send_shuffle(conn)
    );
    return true;
}

static bool handle_single(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], mode);
    COMMAND(
        mpd_run_single(conn, mode),
        mpd_send_single(conn, mode)
    );

    return true;
}

static bool handle_stop(MooseClient * self, struct mpd_connection * conn, G_GNUC_UNUSED const char ** argv) {
    COMMAND(
        mpd_run_stop(conn),
        mpd_send_stop(conn)
    );

    return true;
}

static bool handle_queue_swap(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], pos_a);
    intarg_named(argv[1], pos_b);

    COMMAND(
        mpd_run_swap(conn, pos_a, pos_b),
        mpd_send_swap(conn, pos_a, pos_b)
    );

    return true;
}

static bool handle_queue_swap_id(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    intarg_named(argv[0], id_a);
    intarg_named(argv[1], id_b);

    COMMAND(
        mpd_run_swap_id(conn, id_a, id_b),
        mpd_send_swap_id(conn, id_a, id_b)
    );

    return true;
}

static bool handle_database_update(MooseClient * self, struct mpd_connection * conn, const char ** argv) {
    const char * path = argv[0];
    COMMAND(
        mpd_run_update(conn, path),
        mpd_send_update(conn, path)
    );

    return true;
}

//                                  //
//       Private Command Logic      //
//                                  //

typedef bool (* MooseClientHandler)(
    MooseClient * self,                /* Client to operate on */
    struct mpd_connection * conn,    /* Readily prepared connection */
    const char ** args              /* Arguments as string */
);

typedef struct {
    const char * command;
    int num_args;
    MooseClientHandler handler;
} MooseHandlerField;

static const MooseHandlerField HandlerTable[] = {
    {"consume",            1,  handle_consume},
    {"crossfade",          1,  handle_crossfade},
    {"database_rescan",    1,  handle_database_rescan},
    {"database_update",    1,  handle_database_update},
    {"mixramdb",           1,  handle_mixramdb},
    {"mixramdelay",        1,  handle_mixramdelay},
    {"next",               0,  handle_next},
    {"output_switch",      2,  handle_output_switch},
    {"password",           1,  handle_password},
    {"pause",              0,  handle_pause},
    {"play",               0,  handle_play},
    {"play_id",            1,  handle_play_id},
    {"playlist_add",       2,  handle_playlist_add},
    {"playlist_clear",     1,  handle_playlist_clear},
    {"playlist_delete",    2,  handle_playlist_delete},
    {"playlist_load",      1,  handle_playlist_load},
    {"playlist_move",      3,  handle_playlist_move},
    {"playlist_rename",    2,  handle_playlist_rename},
    {"playlist_rm",        1,  handle_playlist_rm},
    {"playlist_save",      1,  handle_playlist_save},
    {"previous",           0,  handle_previous},
    {"prio",               2,  handle_prio},
    {"prio_id",            2,  handle_prio_id},
    {"prio_range",         3,  handle_prio_range},
    {"queue_add",          1,  handle_queue_add},
    {"queue_clear",        0,  handle_queue_clear},
    {"queue_delete",       1,  handle_queue_delete},
    {"queue_delete_id",    1,  handle_queue_delete_id},
    {"queue_delete_range", 2,  handle_queue_delete_range},
    {"queue_move",         2,  handle_queue_move},
    {"queue_move_range",   3,  handle_queue_move_range},
    {"queue_shuffle",      0,  handle_queue_shuffle},
    {"queue_swap",         2,  handle_queue_swap},
    {"queue_swap_id",      2,  handle_queue_swap_id},
    {"random",             1,  handle_random},
    {"repeat",             1,  handle_repeat},
    {"replay_gain_mode",   1,  handle_replay_gain_mode},
    {"seek",               2,  handle_seek},
    {"seekcur",            1,  handle_seekcur},
    {"seek_id",            2,  handle_seek_id},
    {"setvol",             1,  handle_setvol},
    {"single",             1,  handle_single},
    {"stop",               0,  handle_stop},
    {NULL,                 0,  NULL}
};

static const MooseHandlerField * moose_client_find_handler(const char * command) {
    for (int i = 0; HandlerTable[i].command; ++i) {
        if (g_ascii_strcasecmp(command, HandlerTable[i].command) == 0) {
            return &HandlerTable[i];
        }
    }
    return NULL;
}

// TODO: GVariant instead of string parsing.

/**
 * @brief Parses a string like "command arg1 /// arg2" to a string vector of
 * command, arg1, arg2
 *
 * @param input the whole command string
 *
 * @return a newly allocated vector of strings
 */
static char ** moose_client_parse_into_parts(const char * input) {
    if (input == NULL) {
        return NULL;
    }

    /* Fast forward to the first non-space char */
    while (g_ascii_isspace(*input)) {
        ++input;
    }

    /* Extract the command part */
    char * first_space = strchr(input, ' ');
    char * command = g_strndup(
                         input,
                         (first_space) ? (gsize)ABS(first_space - input) : strlen(input)
                     );

    if (command == NULL) {
        return NULL;
    }

    if (first_space != NULL) {
        /* Now split the arguments */
        char ** args = g_strsplit(first_space + 1, " /// ", -1);
        guint args_len = g_strv_length(args);

        /* Allocate the result vector (command + NULL) */
        char ** result_split = g_malloc0(sizeof(char *) * (args_len + 2));

        /* Copy all data to the new vector */
        result_split[0] = command;
        memcpy(&result_split[1], args, sizeof(char *) * args_len);

        /* Free the old vector, but not strings in it */
        g_free(args);
        return result_split;
    } else {
        /* Only a single command */
        char ** result = g_malloc0(sizeof(char *) * 2);
        result[0] = command;
        return result;
    }
}

static bool moose_client_execute(
    MooseClient * self,
    const char * input,
    struct mpd_connection * conn
) {
    g_assert(conn);

    /* success state of the command */
    bool result = false;

    /* Argument vector */
    char ** parts = moose_client_parse_into_parts(input);

    /* find out what handler to call */
    const MooseHandlerField * handler = moose_client_find_handler(g_strstrip(parts[0]));

    if (handler != NULL) {
        /* Count arguments */
        int arguments = 0;
        while (parts[arguments++]) ;

        /* -2 because: -1 for off-by-one and -1 for not counting the command itself */
        if ((arguments - 2) >= handler->num_args) {
            if (moose_client_is_connected(self)) {
                result = handler->handler(self, conn, (const char **)&parts[1]);
            }
        } else {
            moose_critical(
                "API-Misuse: Too many arguments to %s: Expected %d, Got %d\n",
                parts[0], handler->num_args, arguments - 2
            );
        }
    }

    g_strfreev(parts);
    return result;
}

static char moose_client_command_list_is_start_or_end(const char * command) {
    if (g_ascii_strcasecmp(command, "command_list_begin") == 0) {
        return 1;
    }

    if (g_ascii_strcasecmp(command, "command_list_end") == 0) {
        return -1;
    }

    return 0;
}

static void * moose_client_command_dispatcher(
    G_GNUC_UNUSED struct MooseJobManager * jm,
    G_GNUC_UNUSED volatile bool * cancel,
    void * user_data,
    void * job_data) {
    g_assert(user_data);

    bool result = false;

    if (job_data != NULL) {
        /* Client to operate on */
        MooseClient * self = user_data;

        /* Input command left to parse */
        const char * input = job_data;

        /* Free input (since it was strdrup'd) */
        bool free_input = true;
        if (moose_client_is_connected(self) == false) {
            result = false;
        } else /* commit */
            if (moose_client_command_list_is_start_or_end(input) == -1) {
                result = moose_client_command_list_commit(self);
            } else /* active command */
                if (moose_client_command_list_is_active(self)) {
                    moose_client_command_list_append(self, input);
                    if (moose_client_is_connected(self)) {
                        result = true;
                        free_input = false;
                    }
                } else  /* begin */
                    if (moose_client_command_list_is_start_or_end(input) == +1) {
                        moose_client_command_list_begin(self);
                        result = true;
                    } else {
                        struct mpd_connection * conn = moose_client_get(self);
                        if (conn != NULL && moose_client_is_connected(self)) {
                            result = moose_client_execute(self, input, conn);
                            if (mpd_response_finish(conn) == false) {
                                // moose_client_check_error(self, conn);
                            }
                        }
                        moose_client_put(self);
                    }

        if (free_input) {
            /* Input was strdup'd */
            g_free((char *)input);
        }
    }
    return GINT_TO_POINTER(result);
}

//                                      //
//  Code Pretending to do Command List  //
//                                      //

/**
 * A note about the implementation of command_list_{begin, end}:
 *
 * Currently libmoosecat buffers command list commandos.
 * Instead of sending them actually to the server they are stored in a List.
 * Once command_list_end is found, the list is executed as once.
 * (Sending all commandoes at once, and calling mpd_response_finish to wait for
 * the OK or ACK)
 *
 * This has been decided after testing this (with MPD 0.17.0):
 *
 *   Telnet Session #1              Telnet Session #2
 *
 *   > status                       > command_list_begin
 *   repeat 0                       > repeat 1
 *   > status                       >
 *   repeat 0                       > command_list_end
 *   > status                       > OK
 *   repeat 1
 *
 * The server seems to queue the statements anyway. So no reason to implement
 * something more sophisticated here.
 */

static bool moose_client_command_list_begin(MooseClient * self) {
    g_assert(self);

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    self->priv->command_list.is_active = 1;
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);

    return moose_client_command_list_is_active(self);
}

static void moose_client_command_list_append(MooseClient * self, const char * command) {
    g_assert(self);

    /* prepend now, reverse later on commit */
    self->priv->command_list.commands = g_list_prepend(
                                            self->priv->command_list.commands,
                                            (gpointer)command
                                        );
}

static bool moose_client_command_list_commit(MooseClient * self) {
    g_assert(self);

    MooseClientPrivate * priv = self->priv;

    /* Elements were prepended, so we'll just reverse the list */
    priv->command_list.commands = g_list_reverse(
                                      self->priv->command_list.commands
                                  );

    struct mpd_connection * conn = moose_client_get(self);
    if (conn != NULL) {
        if (mpd_command_list_begin(conn, false) != false) {
            for (GList * iter = priv->command_list.commands; iter != NULL; iter = iter->next) {
                const char * command = iter->data;
                moose_client_execute(self, command, conn);
                g_free((char *)command);
            }

            if (mpd_command_list_end(conn) == false) {
                moose_client_check_error(self, conn);
            }
        } else {
            moose_client_check_error(self, conn);
        }

        if (mpd_response_finish(conn) == false) {
            moose_client_check_error(self, conn);
        }
    }

    g_list_free(priv->command_list.commands);
    priv->command_list.commands = NULL;

    /* Put mutex back */
    moose_client_put(self);

    g_rec_mutex_lock(&priv->client_attr_mutex);
    priv->command_list.is_active = 0;
    g_rec_mutex_unlock(&priv->client_attr_mutex);

    return !moose_client_command_list_is_active(self);
}

//                                  //
//    Public Function Interface     //
//                                  //

long moose_client_send(MooseClient * self, const char * command) {
    g_assert(self);

    return moose_jm_send(self->priv->jm, 0, (void *)g_strdup(command));
}

bool moose_client_recv(MooseClient * self, long job_id) {
    g_assert(self);

    moose_jm_wait_for_id(self->priv->jm, job_id);
    return GPOINTER_TO_INT(moose_jm_get_result(self->priv->jm, job_id));
}

void moose_client_wait(MooseClient * self) {
    g_assert(self);

    moose_jm_wait(self->priv->jm);
}

bool moose_client_run(MooseClient * self, const char * command) {
    return moose_client_recv(self, moose_client_send(self, command));
}

bool moose_client_command_list_is_active(MooseClient * self) {
    g_assert(self);

    bool rc = false;

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    rc = self->priv->command_list.is_active;
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);

    return rc;
}

void moose_client_begin(MooseClient * self) {
    g_assert(self);

    moose_client_send(self, "command_list_begin");
}

long moose_client_commit(MooseClient * self) {
    g_assert(self);

    return moose_client_send(self, "command_list_end");
}

//                                //
//    Update Data Retrieval       //
//                                //

const MooseIdle on_status_update = 0
                                   | MOOSE_IDLE_PLAYER
                                   | MOOSE_IDLE_OPTIONS
                                   | MOOSE_IDLE_MIXER
                                   | MOOSE_IDLE_OUTPUT
                                   | MOOSE_IDLE_QUEUE
                                   ;
const MooseIdle on_stats_update = 0
                                  | MOOSE_IDLE_UPDATE
                                  | MOOSE_IDLE_DATABASE
                                  ;
const MooseIdle on_song_update = 0
                                 | MOOSE_IDLE_PLAYER
                                 ;
const MooseIdle on_rg_status_update = 0
                                      | MOOSE_IDLE_OPTIONS
                                      ;

/* Little hack:
 *
 * Set a very high bit of the enum bitmask and take it
 * as a flag to indicate a status timer indcued trigger.
 *
 * If so, we may decide to not call the client-event callback.
 */
#define MOOSE_IDLE_STATUS_TIMER_FLAG (MOOSE_IDLE_SUBSCRIPTION)

static void moose_update_data_push_full(MooseClient * self, MooseIdle event, bool is_status_timer) {
    g_assert(self);

    /* We may not push 0 - that would cause the event_queue to shutdown */
    if (event != 0) {
        MooseIdle send_event = event;
        if (is_status_timer) {
            event |= MOOSE_IDLE_STATUS_TIMER_FLAG;
        }

        g_async_queue_push(self->priv->event_queue, GINT_TO_POINTER(send_event));
    }
}

static void moose_update_context_info_cb(MooseClient * self, MooseIdle events) {
    if (self == NULL || events == 0 || moose_client_is_connected(self) == false) {
        return;
    }

    struct mpd_connection * conn = moose_client_get(self);

    if (conn == NULL) {
        moose_client_put(self);
        return;
    }

    const bool update_status = (events & on_status_update);
    const bool update_stats = (events & on_stats_update);
    const bool update_song = (events & on_song_update);
    const bool update_rg = (events & on_rg_status_update);

    if (!(update_status || update_stats || update_song || update_rg)) {
        return;
    }

    MooseClientPrivate * priv = self->priv;

    /* Send a block of commands, speeds the thing up by 2x */
    mpd_command_list_begin(conn, true);
    {
        /* Note: order of recv == order of send. */
        if (update_status) {
            mpd_send_status(conn);
        }

        if (update_stats) {
            mpd_send_stats(conn);
        }

        if (update_rg) {
            mpd_send_command(conn, "replay_gain_status", NULL);
        }

        if (update_song) {
            mpd_send_current_song(conn);
        }
    }
    mpd_command_list_end(conn);
    moose_client_check_error(self, conn);

    /* Try to receive status */
    if (update_status) {
        struct mpd_status * tmp_status_struct;
        tmp_status_struct = mpd_recv_status(conn);

        if (priv->status_timer.last_update != NULL) {
            /* Reset the status timer to 0 */
            g_timer_start(priv->status_timer.last_update);
        }

        if (tmp_status_struct) {
            MooseStatus * status = moose_client_ref_status(self);

            if (priv->status != NULL) {
                priv->last_song_data.id = moose_status_get_song_id(priv->status);
                priv->last_song_data.state = moose_status_get_state(priv->status);
            } else {
                priv->last_song_data.id = -1;
                priv->last_song_data.state = MOOSE_STATE_UNKNOWN;
            }

            moose_status_unref(priv->status);
            priv->status = moose_status_new_from_struct(tmp_status_struct);
            mpd_status_free(tmp_status_struct);
            moose_status_unref(status);
        }

        mpd_response_next(conn);
        moose_client_check_error(self, conn);
    }

    /* Try to receive statistics as last */
    if (update_stats) {
        struct mpd_stats * tmp_stats_struct;
        tmp_stats_struct = mpd_recv_stats(conn);

        if (tmp_stats_struct) {
            MooseStatus * status = moose_client_ref_status(self);
            moose_status_update_stats(status, tmp_stats_struct);
            moose_status_unref(status);
            mpd_stats_free(tmp_stats_struct);
        }

        mpd_response_next(conn);
        moose_client_check_error(self, conn);
    }

    /* Read the current replay gain status */
    if (update_rg) {
        struct mpd_pair * mode = mpd_recv_pair_named(conn, "replay_gain_mode");
        if (mode != NULL) {
            MooseStatus * status = moose_client_ref_status(self);
            if (status != NULL) {
                moose_status_set_replay_gain_mode(status, mode->value);
            }

            moose_status_unref(status);
            mpd_return_pair(conn, mode);
        }

        mpd_response_next(conn);
        moose_client_check_error(self, conn);
    }

    /* Try to receive the current song */
    if (update_song) {
        struct mpd_song * new_song_struct = mpd_recv_song(conn);
        MooseSong * new_song = moose_song_new_from_struct(new_song_struct);
        if (new_song_struct != NULL) {
            mpd_song_free(new_song_struct);
        }

        /* We need to call recv() one more time
         * so we end the songlist,
         * it should only return  NULL
         * */
        if (new_song_struct != NULL) {
            struct mpd_song * empty = mpd_recv_song(conn);
            g_assert(empty == NULL);
        }

        MooseStatus * status = moose_client_ref_status(self);
        moose_status_set_current_song(status, new_song);
        moose_status_unref(status);
        moose_client_check_error(self, conn);
    }

    /* Finish repsonse */
    if (update_song || update_stats || update_status || update_rg) {
        mpd_response_finish(conn);
        moose_client_check_error(self, conn);
    }

    moose_client_put(self);
}

void moose_priv_outputs_update(MooseClient * self, MooseIdle event) {
    g_assert(self);

    if ((event & MOOSE_IDLE_OUTPUT) == 0) {
        return /* because of no relevant event */;
    }

    struct mpd_connection * conn = moose_client_get(self);

    if (conn != NULL) {
        if (mpd_send_outputs(conn) == false) {
            moose_client_check_error(self, conn);
        } else {
            struct mpd_output * output = NULL;
            MooseStatus * status = moose_client_ref_status(self);
            moose_status_outputs_clear(status);

            while ((output = mpd_recv_output(conn)) != NULL) {
                moose_status_outputs_add(status, output);
                mpd_output_free(output);
            }
        }

        if (mpd_response_finish(conn) == false) {
            moose_client_check_error(self, conn);
        }
    }
    moose_client_put(self);
}

static bool moose_update_is_a_seek_event(MooseClient * self, MooseIdle event_mask) {
    if (event_mask & MOOSE_IDLE_PLAYER) {
        long curr_song_id = -1;
        MooseState curr_song_state = MOOSE_STATE_UNKNOWN;

        /* Get the current data */
        MooseStatus * status = moose_client_ref_status(self);
        curr_song_id = moose_status_get_song_id(status);
        curr_song_state = moose_status_get_state(status);
        moose_status_unref(status);

        if (curr_song_id != -1 && self->priv->last_song_data.id == curr_song_id) {
            if (self->priv->last_song_data.state == curr_song_state) {
                return true;
            }
        }
    }
    return false;
}

static gpointer moose_update_thread(gpointer user_data) {
    g_assert(user_data);

    MooseClient * self = user_data;

    MooseIdle event_mask = 0;

    while ((event_mask = GPOINTER_TO_INT(g_async_queue_pop(self->priv->event_queue))) != THREAD_TERMINATOR) {
        moose_update_context_info_cb(self, event_mask);
        moose_priv_outputs_update(self, event_mask);

        /* Lookup if we need to trigger a client-event (maybe not if * auto-update)*/
        bool trigger_it = true;
        if (event_mask & MOOSE_IDLE_STATUS_TIMER_FLAG && self->priv->status_timer.trigger_event) {
            trigger_it = false;
        }

        /* Maybe we should make this configurable? */
        if (moose_update_is_a_seek_event(self, event_mask)) {
            /* Set the PLAYER bit to 0 */
            event_mask &= ~MOOSE_IDLE_PLAYER;

            /* and add the SEEK bit instead */
            event_mask |= MOOSE_IDLE_SEEK;
        }

        if (trigger_it) {
            g_signal_emit(self, SIGNALS[SIGNAL_CLIENT_EVENT], 0, event_mask);
            g_signal_emit(self, SIGNALS[SIGNAL_CONNECTIVITY], 0, false);
        }
    }

    return NULL;
}

// STATUS TIMER STUFF //

static gboolean moose_update_status_timer_cb(gpointer user_data) {
    g_assert(user_data);
    MooseClient * self = user_data;

    if (moose_client_is_connected(self) == false) {
        return false;
    }

    MooseState state = MOOSE_STATE_UNKNOWN;
    MooseStatus * status = moose_client_ref_status(self);
    if (status != NULL) {
        state = moose_status_get_state(status);
    }
    moose_status_unref(status);

    if (status != NULL) {
        if (state == MOOSE_STATE_PLAY) {

            /* Substract a small amount to include the network latency - a bit of a hack
             * but worst thing that could happen: you miss one status update.
             * */
            gint interval = self->priv->status_timer.interval;
            float compare = MAX(interval - interval / 10, 0);
            float elapsed = g_timer_elapsed(self->priv->status_timer.last_update, NULL) * 1000;

            if (elapsed >= compare) {
                /* MIXER is a harmless event, but it causes status to update */
                moose_update_data_push_full(self, MOOSE_IDLE_MIXER, true);
            }
        }
    }

    return moose_client_timer_get_active(self);
}

static void moose_update_data_push(MooseClient * self, MooseIdle event) {
    moose_update_data_push_full(self, event, false);
}

struct mpd_connection * moose_base_connect(MooseClient * self, const char * host, int port, float timeout, char ** err) {
    struct mpd_connection * con = mpd_connection_new(host, port, timeout * 1000);

    if (con && mpd_connection_get_error(con) != MPD_ERROR_SUCCESS) {
        char * error_message = g_strdup(mpd_connection_get_error_message(con));
        /* Report the error, but don't try to handle it in that early stage */
        moose_client_check_error_without_handling(self, con);

        if (err != NULL) {
            *err = error_message;
        } else {
            g_free(error_message);
        }

        mpd_connection_free(con);
        con = NULL;
    } else if (err != NULL) {
        *err = NULL;
    }

    return con;
}
