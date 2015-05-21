#include "moose-mpd-client.h"
#include "moose-mpd-client-private.h"
#include "pm/moose-mpd-cmnd-core.h"
#include "pm/moose-mpd-idle-core.h"

#include "moose-mpd-client.h"
#include "moose-status-private.h"
#include "moose-song-private.h"

#include "../misc/moose-misc-job-manager.h"
#include "../moose-config.h"

/* memset() */
#include <string.h>

//////////////////////////////////////////////////////////////
//                                                          //
//                   Types and Defintions                   //
//                                                          //
//////////////////////////////////////////////////////////////

#define ASSERT_IS_MAINTHREAD(client) \
    g_assert(g_thread_self() == (client)->priv->initial_thread)

enum { SIGNAL_CLIENT_EVENT, SIGNAL_CONNECTIVITY, SIGNAL_LOG_MESSAGE, NUM_SIGNALS };

enum {
    PROP_HOST = 1,
    PROP_PORT,
    PROP_TIMEOUT,
    PROP_TIMER_INTERVAL,
    PROP_TIMER_TRIGGER_EVENT,
    PROP_TIMER_ONLY_WHEN_PLAYING,
    PROP_NUMBER
};

static guint SIGNALS[NUM_SIGNALS];

const MooseIdle ON_STATUS_UPDATE_FLAGS = 0 | MOOSE_IDLE_PLAYER | MOOSE_IDLE_OPTIONS |
                                         MOOSE_IDLE_MIXER | MOOSE_IDLE_OUTPUT |
                                         MOOSE_IDLE_QUEUE | MOOSE_IDLE_STATUS_TIMER_FLAG;
const MooseIdle ON_STATS_UPDATE_FLAGS = 0 | MOOSE_IDLE_UPDATE | MOOSE_IDLE_DATABASE;
const MooseIdle ON_CURRENT_SONG_UPDATE_FLAGS = 0 | MOOSE_IDLE_PLAYER;
const MooseIdle ON_REPLAYGAIN_UPDATE_FLAGS = 0 | MOOSE_IDLE_OPTIONS;

typedef struct _MooseClientPrivate {
    /* This is locked on do_get(), * and unlocked on do_put() */
    GRecMutex getput_mutex;
    GRecMutex client_attr_mutex;

    /* The thread moose_client_new(MOOSE_PROTOCOL_IDLE) was called from */
    GThread *initial_thread;

    /* Save last connected host / port */
    char *host;
    int port;
    float timeout;

    /* Indicates if store was not connected yet. */
    gboolean is_virgin;

    /* Job Dispatcher */
    MooseJobManager *jm;

    struct {
        /* Id of command list job */
        int is_active;
        GList *commands;
    } command_list;

    GAsyncQueue *event_queue;
    GThread *update_thread;
    MooseStatus *status;

    struct {
        int timeout_id;
        int interval;
        gboolean trigger_event;
        gboolean only_when_playing;
        GTimer *last_update;
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

    MooseProtocol protocol;
} MooseClientPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(MooseClient, moose_client, G_TYPE_OBJECT);

typedef struct {
    MooseClient *self;
    MooseIdle event;
} MooseClientEventTag;

//////////////////////////////////////////////////////////////
//                                                          //
//                   Private Implementation                 //
//                                                          //
//////////////////////////////////////////////////////////////

static void moose_report_connectivity(MooseClient *self,
                                      const char *new_host,
                                      int new_port,
                                      float new_timeout) {
    gboolean server_changed;
    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    {
        /* Im weird, sorry. TODO. */
        server_changed = 1 && self->priv->is_virgin == false &&
                         ((g_strcmp0(new_host, self->priv->host) != 0) ||
                          (new_port != self->priv->port));

        /* Defloreate the Client (Wheeeh!) */
        self->priv->is_virgin = false;

        g_free(self->priv->host);
        self->priv->host = g_strdup(new_host);
        self->priv->port = new_port;
        self->priv->timeout = new_timeout;
    }
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);

    /* Dispatch *after* host being set */
    g_signal_emit(self, SIGNALS[SIGNAL_CONNECTIVITY], 0, server_changed);
}

static char *moose_compose_error_msg(const char *topic, const char *src) {
    if(src && topic) {
        return g_strdup_printf("%s: ,,%s''", topic, src);
    } else {
        return NULL;
    }
}

static gboolean moose_client_disconnect_idle(MooseClient *self) {
    moose_client_disconnect(self);
    return FALSE;
}

static gboolean moose_client_check_error_impl(MooseClient *self,
                                              struct mpd_connection *cconn,
                                              gboolean handle_fatal) {
    if(self == NULL || cconn == NULL) {
        return true;
    }

    char *error_message = NULL;
    enum mpd_error error = mpd_connection_get_error(cconn);

    if(error != MPD_ERROR_SUCCESS) {
        switch(error) {
        case MPD_ERROR_SYSTEM:
            error_message = moose_compose_error_msg(
                "System-Error", g_strerror(mpd_connection_get_system_error(cconn)));
            break;
        case MPD_ERROR_SERVER:
            error_message = moose_compose_error_msg(
                "Server-Error", mpd_connection_get_error_message(cconn));
            break;
        default:
            error_message = moose_compose_error_msg(
                "Client-Error", mpd_connection_get_error_message(cconn));
            break;
        }

        /* Try to clear the error */
        gboolean is_fatal = !(mpd_connection_clear_error(cconn));

        /* Dispatch the error to the users */
        moose_critical("Error #%d: %s (fatal=%s)", error, error_message,
                       is_fatal ? "yes" : "no");

        /* On really fatal error we better disconnect,
         * than using an invalid connection */
        if(handle_fatal && is_fatal) {
            moose_warning("That was fatal. Disconnecting on mainthread.");
            g_idle_add((GSourceFunc)moose_client_disconnect_idle, self);
        }

        g_free(error_message);
        return true;
    }

    return false;
}

gboolean moose_client_check_error(MooseClient *self, struct mpd_connection *cconn) {
    return moose_client_check_error_impl(self, cconn, true);
}

gboolean moose_client_check_error_without_handling(MooseClient *self,
                                                   struct mpd_connection *cconn) {
    return moose_client_check_error_impl(self, cconn, false);
}

/* Prototype */
static void *moose_client_command_dispatcher(MooseJobManager *jm,
                                             volatile gboolean *cancel,
                                             void *job_data,
                                             void *user_data);

gboolean moose_client_connect_to(MooseClient *self,
                                 const char *host,
                                 int port,
                                 float timeout) {
    char *error = NULL;

    /* Do not connect if already connected. */
    if(self == NULL || moose_client_is_connected(self)) {
        return FALSE;
    }

    ASSERT_IS_MAINTHREAD(self);

    /* Launch a workerthread in the background that will do the communication */
    self->priv->jm = moose_job_manager_new();
    g_signal_connect(self->priv->jm, "dispatch",
                     G_CALLBACK(moose_client_command_dispatcher), self);

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    {
        self->priv->timeout = timeout;
        self->priv->port = port;
        self->priv->host = g_strdup(host);
    }
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);

    moose_message("Attempting to connect…");

    /* Actual implementation of the connection in respective protcolmachine */
    error = self->do_connect(self, host, port, ABS(timeout));

    if(error == NULL && moose_client_is_connected(self)) {
        /* Force updating of status/stats/song on connect */
        moose_client_force_sync(self, INT_MAX);

        /* Check if server changed and trigger a signal. */
        moose_report_connectivity(self, host, port, timeout);
        moose_message("…Fully connected!");
    } else {
        g_signal_emit(self, SIGNALS[SIGNAL_CONNECTIVITY], 0, false);
        moose_critical("…Cannot connect: %s", error);
    }

    return (error == NULL);
}

gboolean moose_client_is_connected(MooseClient *self) {
    return (self && self->do_is_connected && self->do_is_connected(self));
}

void moose_client_put(MooseClient *self) {
    g_assert(self);

    /* Put connection back to event listening. */
    if(moose_client_is_connected(self) && self->do_put != NULL) {
        self->do_put(self);
    }

    /* Make the connection accesible to other threads */
    g_rec_mutex_unlock(&self->getput_mutex);
}

struct mpd_connection *moose_client_get(MooseClient *self) {
    g_assert(self);

    /* Return the readily sendable connection */
    struct mpd_connection *conn = NULL;

    /* lock the connection to make sure, only one thread
     * can use it. This prevents us from relying on
     * the user to lock himself. */
    g_rec_mutex_lock(&self->getput_mutex);

    if(moose_client_is_connected(self) && self->do_get != NULL) {
        conn = self->do_get(self);
        moose_client_check_error(self, conn);
    }
    return conn;
}

gboolean moose_client_disconnect(MooseClient *self) {
    gboolean error_happenend = true;
    MooseClientPrivate *priv = self->priv;

    g_printerr("DOING DISCONNECT\n!");

    if(self == NULL || moose_client_is_connected(self) == false) {
        return error_happenend;
    }

    ASSERT_IS_MAINTHREAD(self);

    /* Lock the connection while destroying it */
    g_rec_mutex_lock(&self->getput_mutex);
    {
        /* Finish current running command */
        moose_job_manager_unref(priv->jm);
        priv->jm = NULL;

        /* Reset status/song/stats to NULL */
        MooseStatus *status = priv->status;
        priv->status = NULL;
        moose_status_unref(status);

        /* let the connector clean up itself */
        error_happenend = (self->do_disconnect) ? !self->do_disconnect(self) : true;
    }
    g_rec_mutex_unlock(&self->getput_mutex);

    /* Notify user of the disconnect, but after unlocking to prevent
     * deadlocks. The user could call some locking command.  */
    g_signal_emit(self, SIGNALS[SIGNAL_CONNECTIVITY], 0, false);

    return error_happenend;
}

void moose_client_unref(MooseClient *self) {
    if(self != NULL) {
        g_object_unref(self);
    }
}

static void moose_updata_data_push(MooseClient *self, MooseIdle event,
                                   gboolean is_status_timer) {
    g_assert(self);

    /* We may not push 0 - that would cause the event_queue to shutdown */
    if(event != 0) {
        MooseIdle send_event = event;
        if(is_status_timer) {
            event |= MOOSE_IDLE_STATUS_TIMER_FLAG;
        }

        g_async_queue_push(self->priv->event_queue, GINT_TO_POINTER(send_event));
    }
}

void moose_client_force_sync(MooseClient *self, MooseIdle events) {
    g_assert(self);
    moose_updata_data_push(self, events, false);
}

char *moose_client_get_host(MooseClient *self) {
    g_assert(self);

    char *host = NULL;
    g_object_get(G_OBJECT(self), "host", &host, NULL);
    return host;
}

unsigned moose_client_get_port(MooseClient *self) {
    unsigned port = 6600;
    if(moose_client_is_connected(self)) {
        g_object_get(G_OBJECT(self), "port", &port, NULL);
    }
    return port;
}

float moose_client_get_timeout(MooseClient *self) {
    float timeout = 20;
    if(moose_client_is_connected(self)) {
        g_object_get(G_OBJECT(self), "timeout", &timeout, NULL);
    }
    return timeout;
}

gboolean moose_client_timer_get_active(MooseClient *self) {
    g_assert(self);

    g_mutex_lock(&self->priv->status_timer.mutex);
    gboolean result = (self->priv->status_timer.timeout_id != -1);
    g_mutex_unlock(&self->priv->status_timer.mutex);

    return result;
}

static gboolean moose_update_status_timer_cb(gpointer user_data) {
    MooseClient *self = MOOSE_CLIENT(user_data);
    if(moose_client_is_connected(self) == false) {
        return moose_client_timer_get_active(self);
    }

    MooseState state = MOOSE_STATE_UNKNOWN;
    MooseStatus *status = moose_client_ref_status(self);
    {
        if(status != NULL) {
            state = moose_status_get_state(status);
        }
    }
    moose_status_unref(status);

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    {
        if(status != NULL &&
           (state == MOOSE_STATE_PLAY || !self->priv->status_timer.only_when_playing)) {
            /* Substract a small amount to include the network latency - a bit of a hack
            * but worst thing that could happen: you miss one status update.
            * */
            gint interval = self->priv->status_timer.interval;
            float compare = MAX(interval - interval / 10, 0);
            float elapsed =
                1000 * g_timer_elapsed(self->priv->status_timer.last_update, NULL);

            if(elapsed >= compare) {
                /* MIXER is a harmless event, but it causes the status to update */
                moose_updata_data_push(self, MOOSE_IDLE_STATUS_TIMER_FLAG, true);
            }

            /* Restart. */
            g_timer_start(self->priv->status_timer.last_update);
        }
    }
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);

    /* If false the source is disconnected */
    return moose_client_timer_get_active(self);
}

void moose_client_timer_set_active(MooseClient *self, gboolean state) {
    g_assert(self);
    MooseClientPrivate *priv = self->priv;

    if(state == false && moose_client_timer_get_active(self)) {
        /* Unregister */
        g_mutex_lock(&priv->status_timer.mutex);
        {
            if(priv->status_timer.timeout_id > 0) {
                g_source_remove(priv->status_timer.timeout_id);
                priv->status_timer.timeout_id = -1;
            }

            priv->status_timer.interval = 0;

            if(priv->status_timer.last_update != NULL) {
                g_timer_destroy(priv->status_timer.last_update);
                priv->status_timer.last_update = NULL;
            }
        }
        g_mutex_unlock(&priv->status_timer.mutex);
    } else {
        /* Register */
        float timeout = 0.5;
        g_object_get(self, "timer-interval", &timeout, NULL);
        timeout = MAX(0.1, timeout);

        g_mutex_lock(&self->priv->status_timer.mutex);
        {
            MooseClientPrivate *priv = self->priv;
            priv->status_timer.last_update = g_timer_new();
            priv->status_timer.timeout_id =
                g_timeout_add(timeout * 1000, moose_update_status_timer_cb, self);
        }
        g_mutex_unlock(&self->priv->status_timer.mutex);
    }
}

MooseStatus *moose_client_ref_status(MooseClient *self) {
    MooseStatus *status = NULL;

    /* Make helgrind happy, g_object_ref should be safe already. */
    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    {
        if(self->priv->status != NULL) {
            status = g_object_ref(self->priv->status);
        }
    }
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);

    return status;
}

/* Prototypes */
static gboolean moose_client_command_list_begin(MooseClient *self);
static void moose_client_command_list_append(MooseClient *self, GVariant *variant);
static gboolean moose_client_command_list_commit(MooseClient *self);

/**
 * Missing commands:
 *
 *  shuffle_range
 *  (...)
 */
#define COMMAND(_getput_code_, _command_list_code_) \
    if(moose_client_command_list_is_active(self)) { \
        _command_list_code_;                        \
    } else {                                        \
        _getput_code_;                              \
    }

static gboolean handle_queue_add(MooseClient *self, struct mpd_connection *conn,
                                 const char *format, GVariant *variant) {
    const char *uri = NULL;
    g_variant_get(variant, format, NULL, &uri);
    COMMAND(mpd_run_add(conn, uri), mpd_send_add(conn, uri))

    return true;
}

static gboolean handle_queue_clear(MooseClient *self, struct mpd_connection *conn,
                                   G_GNUC_UNUSED const char *format,
                                   G_GNUC_UNUSED GVariant *variant) {
    COMMAND(mpd_run_clear(conn), mpd_send_clear(conn))

    return true;
}

static gboolean handle_consume(MooseClient *self, struct mpd_connection *conn,
                               const char *format, GVariant *variant) {
    gboolean mode = false;
    g_variant_get(variant, format, NULL, &mode);

    COMMAND(mpd_run_consume(conn, mode), mpd_send_consume(conn, mode))

    return true;
}

static gboolean handle_crossfade(MooseClient *self, struct mpd_connection *conn,
                                 const char *format, GVariant *variant) {
    double xfade = 0;
    g_variant_get(variant, format, NULL, &xfade);

    COMMAND(mpd_run_crossfade(conn, xfade), mpd_send_crossfade(conn, xfade))

    return true;
}

static gboolean handle_queue_delete(MooseClient *self, struct mpd_connection *conn,
                                    const char *format, GVariant *variant) {
    int pos = 0;
    g_variant_get(variant, format, NULL, &pos);

    COMMAND(mpd_run_delete(conn, pos), mpd_send_delete(conn, pos))

    return true;
}

static gboolean handle_queue_delete_id(MooseClient *self, struct mpd_connection *conn,
                                       const char *format, GVariant *variant) {
    int id = 0;
    g_variant_get(variant, format, NULL, &id);

    COMMAND(mpd_run_delete_id(conn, id), mpd_send_delete_id(conn, id))

    return true;
}

static gboolean handle_queue_delete_range(MooseClient *self, struct mpd_connection *conn,
                                          const char *format, GVariant *variant) {
    int start = 0, end = 1;
    g_variant_get(variant, format, NULL, &start, &end);

    COMMAND(mpd_run_delete_range(conn, start, end),
            mpd_send_delete_range(conn, start, end))

    return true;
}

static gboolean handle_output_switch(MooseClient *self, struct mpd_connection *conn,
                                     const char *format, GVariant *variant) {
    char *output_name = NULL;
    gboolean mode = true;
    int output_id;
    gboolean result = false;

    g_variant_get(variant, format, NULL, &output_name, &mode);

    MooseStatus *status = moose_client_ref_status(self);
    { output_id = moose_status_output_lookup_id(status, output_name); }
    moose_status_unref(status);

    if(output_id != -1) {
        if(mode == TRUE) {
            COMMAND(mpd_run_enable_output(conn, output_id),
                    mpd_send_enable_output(conn, output_id));
        } else {
            COMMAND(mpd_run_disable_output(conn, output_id),
                    mpd_send_disable_output(conn, output_id));
        }
        result = true;
    } else {
        moose_warning("switch-output: Cannot find name for '%s'", output_name);
        result = false;
    }

    g_free(output_name);
    return result;
}

static gboolean handle_playlist_load(MooseClient *self, struct mpd_connection *conn,
                                     const char *format, GVariant *variant) {
    const char *playlist = NULL;
    g_variant_get(variant, format, NULL, &playlist);
    if(playlist == NULL) {
        return false;
    }

    COMMAND(mpd_run_load(conn, playlist), mpd_send_load(conn, playlist));

    return true;
}

static gboolean handle_mixramdb(MooseClient *self, struct mpd_connection *conn,
                                const char *format, GVariant *variant) {
    double decibel = 0;
    g_variant_get(variant, format, NULL, &decibel);

    COMMAND(mpd_run_mixrampdb(conn, decibel), mpd_send_mixrampdb(conn, decibel));

    return true;
}

static gboolean handle_mixramdelay(MooseClient *self, struct mpd_connection *conn,
                                   const char *format, GVariant *variant) {
    double seconds = 0;
    g_variant_get(variant, format, NULL, &seconds);

    COMMAND(mpd_run_mixrampdelay(conn, seconds), mpd_send_mixrampdelay(conn, seconds));

    return true;
}

static gboolean handle_queue_move(MooseClient *self, struct mpd_connection *conn,
                                  const char *format, GVariant *variant) {
    int old_id = 0, new_id = 0;
    g_variant_get(variant, format, NULL, &old_id, &new_id);

    COMMAND(mpd_run_move_id(conn, old_id, new_id),
            mpd_send_move_id(conn, old_id, new_id));

    return true;
}

static gboolean handle_queue_move_range(MooseClient *self, struct mpd_connection *conn,
                                        const char *format, GVariant *variant) {
    int start_pos = 0, end_pos = 1, new_pos = 0;
    g_variant_get(variant, format, NULL, &start_pos, &end_pos, &new_pos);

    COMMAND(mpd_run_move_range(conn, start_pos, end_pos, new_pos),
            mpd_send_move_range(conn, start_pos, end_pos, new_pos));

    return true;
}

static gboolean handle_next(MooseClient *self, struct mpd_connection *conn,
                            G_GNUC_UNUSED const char *format,
                            G_GNUC_UNUSED GVariant *variant) {
    COMMAND(mpd_run_next(conn), mpd_send_next(conn))
    return true;
}

static gboolean handle_password(MooseClient *self, struct mpd_connection *conn,
                                const char *format, GVariant *variant) {
    const char *password = NULL;
    gboolean rc = false;

    g_variant_get(variant, format, NULL, &password);

    if(password == NULL) {
        return FALSE;
    }

    COMMAND(rc = mpd_run_password(conn, password), mpd_send_password(conn, password));
    return rc;
}

static gboolean handle_pause(MooseClient *self, struct mpd_connection *conn,
                             G_GNUC_UNUSED const char *format,
                             G_GNUC_UNUSED GVariant *variant) {
    COMMAND(mpd_run_toggle_pause(conn), mpd_send_toggle_pause(conn))

    return true;
}

static gboolean handle_play(MooseClient *self, struct mpd_connection *conn,
                            G_GNUC_UNUSED const char *format,
                            G_GNUC_UNUSED GVariant *variant) {
    COMMAND(mpd_run_play(conn), mpd_send_play(conn))

    return true;
}

static gboolean handle_play_id(MooseClient *self, struct mpd_connection *conn,
                               const char *format, GVariant *variant) {
    int id = 0;
    g_variant_get(variant, format, NULL, &id);

    COMMAND(mpd_run_play_id(conn, id), mpd_send_play_id(conn, id))

    return true;
}

static gboolean handle_playlist_add(MooseClient *self, struct mpd_connection *conn,
                                    const char *format, GVariant *variant) {
    const char *name = NULL, *file = NULL;

    g_variant_get(variant, format, NULL, &name, &file);

    if(name == NULL || file == NULL) {
        return FALSE;
    }

    COMMAND(mpd_run_playlist_add(conn, name, file),
            mpd_send_playlist_add(conn, name, file))

    return true;
}

static gboolean handle_playlist_clear(MooseClient *self, struct mpd_connection *conn,
                                      const char *format, GVariant *variant) {
    const char *name = NULL;

    g_variant_get(variant, format, NULL, &name);

    if(name == NULL) {
        return FALSE;
    }

    COMMAND(mpd_run_playlist_clear(conn, name), mpd_send_playlist_clear(conn, name))

    return true;
}

static gboolean handle_playlist_delete(MooseClient *self, struct mpd_connection *conn,
                                       const char *format, GVariant *variant) {
    const char *name = NULL;
    int pos = 0;

    g_variant_get(variant, format, NULL, &name, &pos);

    if(name == NULL) {
        return FALSE;
    }

    COMMAND(mpd_run_playlist_delete(conn, name, pos),
            mpd_send_playlist_delete(conn, name, pos))

    return true;
}

static gboolean handle_playlist_move(MooseClient *self, struct mpd_connection *conn,
                                     const char *format, GVariant *variant) {
    const char *name = NULL;
    int old_pos = 0, new_pos = 1;

    g_variant_get(variant, format, NULL, &name, &old_pos, &new_pos);

    if(name == NULL) {
        return FALSE;
    }

    COMMAND(mpd_send_playlist_move(conn, name, old_pos, new_pos);
            mpd_response_finish(conn);
            , mpd_send_playlist_move(conn, name, old_pos, new_pos);)

    return true;
}

static gboolean handle_previous(MooseClient *self, struct mpd_connection *conn,
                                G_GNUC_UNUSED const char *format,
                                G_GNUC_UNUSED GVariant *variant) {
    COMMAND(mpd_run_previous(conn), mpd_send_previous(conn))

    return true;
}

static gboolean handle_prio(MooseClient *self, struct mpd_connection *conn,
                            const char *format, GVariant *variant) {
    int prio = 0, position = 0;
    g_variant_get(variant, format, NULL, &prio, &position);

    COMMAND(mpd_run_prio(conn, prio, position), mpd_send_prio(conn, prio, position))

    return true;
}

static gboolean handle_prio_range(MooseClient *self, struct mpd_connection *conn,
                                  const char *format, GVariant *variant) {
    int prio = 0, start_pos = 0, end_pos = 1;
    g_variant_get(variant, format, NULL, &prio, &start_pos, &end_pos);

    COMMAND(mpd_run_prio_range(conn, prio, start_pos, end_pos),
            mpd_send_prio_range(conn, prio, start_pos, end_pos))

    return true;
}

static gboolean handle_prio_id(MooseClient *self, struct mpd_connection *conn,
                               const char *format, GVariant *variant) {
    int prio = 0;
    int id = 0;

    g_variant_get(variant, format, NULL, &prio, &id);

    COMMAND(mpd_run_prio_id(conn, prio, id), mpd_send_prio_id(conn, prio, id))

    return true;
}

static gboolean handle_random(MooseClient *self, struct mpd_connection *conn,
                              const char *format, GVariant *variant) {
    gboolean mode = FALSE;

    g_variant_get(variant, format, NULL, &mode);

    COMMAND(mpd_run_random(conn, mode), mpd_send_random(conn, mode))

    return true;
}

static gboolean handle_playlist_rename(MooseClient *self, struct mpd_connection *conn,
                                       const char *format, GVariant *variant) {
    const char *old_name = NULL;
    const char *new_name = NULL;
    g_variant_get(variant, format, NULL, &old_name, &new_name);

    if(old_name == NULL || new_name == NULL) {
        return FALSE;
    }

    COMMAND(mpd_run_rename(conn, old_name, new_name),
            mpd_send_rename(conn, old_name, new_name));
    return true;
}

static gboolean handle_repeat(MooseClient *self, struct mpd_connection *conn,
                              const char *format, GVariant *variant) {
    gboolean mode = false;
    g_variant_get(variant, format, NULL, &mode);

    COMMAND(mpd_run_repeat(conn, mode), mpd_send_repeat(conn, mode));

    return true;
}

static gboolean handle_replay_gain_mode(MooseClient *self, struct mpd_connection *conn,
                                        const char *format, GVariant *variant) {
    const char *replay_gain_mode = NULL;
    g_variant_get(variant, format, NULL, &replay_gain_mode);
    if(replay_gain_mode == NULL) {
        return FALSE;
    }

    COMMAND(mpd_send_command(conn, "replay_gain_mode", replay_gain_mode, NULL);
            mpd_response_finish(conn);
            , /* send command */
            mpd_send_command(conn, "replay_gain_mode", replay_gain_mode, NULL);)

    return true;
}

static gboolean handle_database_rescan(MooseClient *self, struct mpd_connection *conn,
                                       const char *format, GVariant *variant) {
    const char *path = NULL;
    g_variant_get(variant, format, NULL, &path);
    if(path == NULL) {
        return FALSE;
    }

    COMMAND(mpd_run_rescan(conn, path), mpd_send_rescan(conn, path));

    return true;
}

static gboolean handle_playlist_rm(MooseClient *self, struct mpd_connection *conn,
                                   const char *format, GVariant *variant) {
    const char *playlist_name = NULL;
    g_variant_get(variant, format, NULL, &playlist_name);
    if(playlist_name == NULL) {
        return FALSE;
    }

    COMMAND(mpd_run_rm(conn, playlist_name), mpd_send_rm(conn, playlist_name));

    return true;
}

static gboolean handle_playlist_save(MooseClient *self, struct mpd_connection *conn,
                                     const char *format, GVariant *variant) {
    const char *as_name = NULL;
    g_variant_get(variant, format, NULL, &as_name);
    if(as_name == NULL) {
        return FALSE;
    }

    COMMAND(mpd_run_save(conn, as_name), mpd_send_save(conn, as_name));

    return true;
}

static gboolean handle_seek(MooseClient *self, struct mpd_connection *conn,
                            const char *format, GVariant *variant) {
    int pos = 0;
    double seconds = 0;
    g_variant_get(variant, format, NULL, &pos, &seconds);

    COMMAND(mpd_run_seek_pos(conn, pos, seconds), mpd_send_seek_pos(conn, pos, seconds));

    return true;
}

static gboolean handle_seek_id(MooseClient *self, struct mpd_connection *conn,
                               const char *format, GVariant *variant) {
    int id = 0;
    double seconds = 0;
    g_variant_get(variant, format, NULL, &id, &seconds);

    COMMAND(mpd_run_seek_id(conn, id, seconds), mpd_send_seek_id(conn, id, seconds));

    return true;
}

static gboolean handle_seekcur(MooseClient *self, struct mpd_connection *conn,
                               const char *format, GVariant *variant) {
    int seconds = 0;
    g_variant_get(variant, format, NULL, &seconds);

    /* there is 'seekcur' in newer mpd versions,
     * but we can emulate it easily */
    if(moose_client_is_connected(self)) {
        int curr_id = 0;

        MooseSong *current_song = NULL;
        MooseStatus *status = moose_client_ref_status(self);
        { current_song = moose_status_get_current_song(status); }
        moose_status_unref(status);

        if(current_song != NULL) {
            curr_id = moose_song_get_id(current_song);
            moose_song_unref(current_song);
        } else {
            moose_song_unref(current_song);
            return false;
        }

        COMMAND(mpd_run_seek_id(conn, curr_id, seconds),
                mpd_send_seek_id(conn, curr_id, seconds))
    }

    return true;
}

static gboolean handle_setvol(MooseClient *self, struct mpd_connection *conn,
                              const char *format, GVariant *variant) {
    int volume = 100;
    g_variant_get(variant, format, NULL, &volume);

    COMMAND(mpd_run_set_volume(conn, volume), mpd_send_set_volume(conn, volume));

    return true;
}

static gboolean handle_queue_shuffle(MooseClient *self, struct mpd_connection *conn,
                                     G_GNUC_UNUSED const char *format,
                                     G_GNUC_UNUSED GVariant *variant) {
    COMMAND(mpd_run_shuffle(conn), mpd_send_shuffle(conn));
    return true;
}

static gboolean handle_single(MooseClient *self, struct mpd_connection *conn,
                              const char *format, GVariant *variant) {
    gboolean mode = false;
    g_variant_get(variant, format, NULL, &mode);

    COMMAND(mpd_run_single(conn, mode), mpd_send_single(conn, mode));

    return true;
}

static gboolean handle_stop(MooseClient *self, struct mpd_connection *conn,
                            G_GNUC_UNUSED const char *format,
                            G_GNUC_UNUSED GVariant *variant) {
    COMMAND(mpd_run_stop(conn), mpd_send_stop(conn));

    return true;
}

static gboolean handle_queue_swap(MooseClient *self, struct mpd_connection *conn,
                                  const char *format, GVariant *variant) {
    int pos_a = 0, pos_b = 0;
    g_variant_get(variant, format, NULL, &pos_a, &pos_b);

    COMMAND(mpd_run_swap(conn, pos_a, pos_b), mpd_send_swap(conn, pos_a, pos_b));

    return true;
}

static gboolean handle_queue_swap_id(MooseClient *self, struct mpd_connection *conn,
                                     const char *format, GVariant *variant) {
    int id_a = 0, id_b = 0;
    g_variant_get(variant, format, NULL, &id_a, &id_b);

    COMMAND(mpd_run_swap_id(conn, id_a, id_b), mpd_send_swap_id(conn, id_a, id_b));

    return true;
}

static gboolean handle_database_update(MooseClient *self, struct mpd_connection *conn,
                                       const char *format, GVariant *variant) {
    const char *path = NULL;
    g_variant_get(variant, format, NULL, &path);

    if(path != NULL) {
        return FALSE;
    }

    COMMAND(mpd_run_update(conn, path), mpd_send_update(conn, path));

    return true;
}

typedef gboolean (*MooseClientHandler)(
    MooseClient *self,           /* Client to operate on */
    struct mpd_connection *conn, /* Readily prepared connection */
    const char *format,          /* Variant Type format */
    GVariant *variant            /* Variant storing the data */
    );

typedef struct {
    const char *command;
    int num_args;
    const char *format;
    MooseClientHandler handler;
} MooseHandlerField;

static const MooseHandlerField HandlerTable[] = {
    {"consume", 1, "(sb)", handle_consume},
    {"crossfade", 1, "(sb)", handle_crossfade},
    {"database-rescan", 1, "(ss)", handle_database_rescan},
    {"database-update", 1, "(ss)", handle_database_update},
    {"mixramdb", 1, "(sd)", handle_mixramdb},
    {"mixramdelay", 1, "(sd)", handle_mixramdelay},
    {"next", 0, "(s)", handle_next},
    {"output-switch", 2, "(ssb)", handle_output_switch},
    {"password", 1, "(ss)", handle_password},
    {"pause", 0, "(s)", handle_pause},
    {"play", 0, "(s)", handle_play},
    {"play-id", 1, "(si)", handle_play_id},
    {"playlist-add", 2, "(sss)", handle_playlist_add},
    {"playlist-clear", 1, "(ss)", handle_playlist_clear},
    {"playlist-delete", 2, "(ssi)", handle_playlist_delete},
    {"playlist-load", 1, "(ss)", handle_playlist_load},
    {"playlist-move", 3, "(ssii)", handle_playlist_move},
    {"playlist-rename", 2, "(sss)", handle_playlist_rename},
    {"playlist-rm", 1, "(ss)", handle_playlist_rm},
    {"playlist-save", 1, "(ss)", handle_playlist_save},
    {"previous", 0, "(s)", handle_previous},
    {"prio", 2, "(sii)", handle_prio},
    {"prio-id", 2, "(sii)", handle_prio_id},
    {"prio-range", 3, "(siii)", handle_prio_range},
    {"queue-add", 1, "(ss)", handle_queue_add},
    {"queue-clear", 0, "(s)", handle_queue_clear},
    {"queue-delete", 1, "(si)", handle_queue_delete},
    {"queue-delete-id", 1, "(si)", handle_queue_delete_id},
    {"queue-delete-range", 2, "(sii)", handle_queue_delete_range},
    {"queue-move", 2, "(sii)", handle_queue_move},
    {"queue-move-range", 3, "(siii)", handle_queue_move_range},
    {"queue-shuffle", 0, "(s)", handle_queue_shuffle},
    {"queue-swap", 2, "(sii)", handle_queue_swap},
    {"queue-swap-id", 2, "(sii)", handle_queue_swap_id},
    {"random", 1, "(sb)", handle_random},
    {"repeat", 1, "(sb)", handle_repeat},
    {"replay-gain-mode", 1, "(ss)", handle_replay_gain_mode},
    {"seek", 2, "(sid)", handle_seek},
    {"seekcur", 1, "(sd)", handle_seekcur},
    {"seek-id", 2, "(sid)", handle_seek_id},
    {"setvol", 1, "(si)", handle_setvol},
    {"single", 1, "(sb)", handle_single},
    {"stop", 0, "(s)", handle_stop},
    {NULL, 0, NULL, NULL}};

static const MooseHandlerField *moose_client_find_handler(const char *command) {
    for(int i = 0; HandlerTable[i].command; ++i) {
        if(g_ascii_strcasecmp(command, HandlerTable[i].command) == 0) {
            return &HandlerTable[i];
        }
    }
    return NULL;
}

static gboolean moose_client_execute(MooseClient *self,
                                     GVariant *variant,
                                     struct mpd_connection *conn) {
    g_assert(conn);
    g_return_val_if_fail(variant, FALSE);

    /* success state of the command */
    gboolean result = FALSE;

    /* Argument vector */
    char *command = NULL;
    g_variant_get_child(variant, 0, "s", &command);

    if(command == NULL) {
        return FALSE;
    }

    /* find out what handler to call */
    const MooseHandlerField *handler = moose_client_find_handler(g_strstrip(command));

    if(handler != NULL) {
        /* Count arguments */
        int n_arguments = g_variant_n_children(variant);

        /* -2 because: -1 for off-by-one and -1 for not counting the command itself */
        if((n_arguments - 1) >= handler->num_args) {
            if(moose_client_is_connected(self)) {
                if(g_variant_check_format_string(variant, handler->format, TRUE) ==
                   FALSE) {
                    moose_critical("Invalid commandtype. Type %s required, got %s",
                                   handler->format, g_variant_get_type_string(variant));
                } else {
                    /* Alright. */
                    result = handler->handler(self, conn, handler->format, variant);
                }
            }
        } else {
            moose_critical("API-Misuse: Wrong number of arguments to %s: Expected %d, Got %d\n",
                           command, handler->num_args, n_arguments - 1);
        }
    } else {
        /* No Handler found */
        moose_critical("There is no such command: %s", command);
    }

    g_free(command);
    return result;
}

static char moose_client_command_list_is_start_or_end(GVariant *variant) {
    g_return_val_if_fail(variant, 0);

    char status = 0;
    char *command = NULL;
    g_variant_get_child(variant, 0, "s", &command);

    if(command == NULL) {
        return 0;
    }

    if(g_ascii_strcasecmp(command, "command_list_begin") == 0) {
        status = 1;
    }

    if(g_ascii_strcasecmp(command, "command_list_end") == 0) {
        status = -1;
    }

    g_free(command);
    return status;
}

static void *moose_client_command_dispatcher(G_GNUC_UNUSED MooseJobManager *jm,
                                             G_GNUC_UNUSED volatile gboolean *cancel,
                                             void *job_data,
                                             void *user_data) {
    g_assert(user_data);

    /* Success? */
    gboolean result = false;

    /* Input command left to parse */
    GVariant *input = job_data;

    /* Client to operate on */
    MooseClient *self = MOOSE_CLIENT(user_data);

    /* Free input? (since it was strdrup'd) */
    gboolean free_input = true;

    if(input == NULL) {
        goto cleanup;
    }

    if(moose_client_is_connected(self) == false) {
        goto cleanup;
    }

    /* Shall we commit? */
    if(moose_client_command_list_is_start_or_end(input) == -1) {
        result = moose_client_command_list_commit(self);
        goto cleanup;
    }

    /* Are we in command list mode? */
    if(moose_client_command_list_is_active(self)) {
        moose_client_command_list_append(self, input);
        result = moose_client_is_connected(self);
        free_input = false;
    } else if(moose_client_command_list_is_start_or_end(input) == +1) {
        /* Apparently not, shall we get into command list mode? */
        moose_client_command_list_begin(self);
        result = moose_client_is_connected(self);
    } else {
        /* Nope, just a normal command. */
        struct mpd_connection *conn = moose_client_get(self);
        if(conn != NULL && moose_client_is_connected(self)) {
            result = moose_client_execute(self, input, conn);
            if(mpd_response_finish(conn) == false) {
                // moose_client_check_error(self, conn);
            }
        }
        moose_client_put(self);
    }

cleanup:
    if(free_input) {
        /* Input was strdup'd */
        g_variant_unref(input);
    }
    return GINT_TO_POINTER(result);
}

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

static gboolean moose_client_command_list_begin(MooseClient *self) {
    g_assert(self);

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    { self->priv->command_list.is_active = 1; }
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);

    return moose_client_command_list_is_active(self);
}

static void moose_client_command_list_append(MooseClient *self, GVariant *variant) {
    g_assert(self);

    /* prepend now, reverse later on commit */
    self->priv->command_list.commands =
        g_list_prepend(self->priv->command_list.commands, (gpointer)variant);
}

static gboolean moose_client_command_list_commit(MooseClient *self) {
    g_assert(self);

    MooseClientPrivate *priv = self->priv;

    /* Elements were prepended, so we'll just reverse the list */
    priv->command_list.commands = g_list_reverse(self->priv->command_list.commands);

    struct mpd_connection *conn = moose_client_get(self);
    if(conn != NULL) {
        if(mpd_command_list_begin(conn, false) != false) {
            for(GList *iter = priv->command_list.commands; iter; iter = iter->next) {
                GVariant *variant = iter->data;
                moose_client_execute(self, variant, conn);
                g_variant_unref(variant);
            }

            if(mpd_command_list_end(conn) == false) {
                moose_client_check_error(self, conn);
            }
        } else {
            moose_client_check_error(self, conn);
        }

        if(mpd_response_finish(conn) == false) {
            moose_client_check_error(self, conn);
        }
    }

    g_list_free(priv->command_list.commands);
    priv->command_list.commands = NULL;

    /* Put mutex back */
    moose_client_put(self);

    g_rec_mutex_lock(&priv->client_attr_mutex);
    { priv->command_list.is_active = 0; }
    g_rec_mutex_unlock(&priv->client_attr_mutex);

    return !moose_client_command_list_is_active(self);
}

long moose_client_send_variant(MooseClient *self, GVariant *variant) {
    g_return_val_if_fail(self, -1);

    return moose_job_manager_send(self->priv->jm, 0, g_variant_ref(variant));
}

long moose_client_send_single(MooseClient *self, const char *command_name) {
    g_return_val_if_fail(self, -1);

    const MooseHandlerField *handler = moose_client_find_handler(command_name);
    if(handler != NULL) {
        if(strcmp(handler->format, "(s)") != 0) {
            moose_warning("moose_client_send_single(\"%s\") requires arguments.",
                          command_name);
            return -1;
        }

        return moose_job_manager_send(self->priv->jm, 0,
                                      g_variant_new("(s)", command_name));
    } else {
        return -1;
    }
}

long moose_client_send(MooseClient *self, const char *command) {
    g_return_val_if_fail(self, -1);

    GError *error = NULL;
    GVariant *parsed = g_variant_parse(NULL, command, NULL, NULL, &error);

    if(error != NULL) {
        char *error_string = g_variant_parse_error_print_context(error, command);
        moose_critical("Cannot run command: %s\n:%s", command, error_string);
        g_free(error_string);
        return -1;
    } else {
        return moose_job_manager_send(self->priv->jm, 0, parsed);
    }
}

gboolean moose_client_recv(MooseClient *self, long job_id) {
    g_assert(self);

    moose_job_manager_wait_for_id(self->priv->jm, job_id);
    return GPOINTER_TO_INT(moose_job_manager_get_result(self->priv->jm, job_id));
}

void moose_client_wait(MooseClient *self) {
    g_assert(self);

    moose_job_manager_wait(self->priv->jm);
}

gboolean moose_client_run(MooseClient *self, const char *command) {
    return moose_client_recv(self, moose_client_send(self, command));
}

gboolean moose_client_run_variant(MooseClient *self, GVariant *variant) {
    return moose_client_recv(self, moose_client_send_variant(self, variant));
}

gboolean moose_client_run_single(MooseClient *self, const char *command_name) {
    return moose_client_recv(self, moose_client_send_single(self, command_name));
}

gboolean moose_client_command_list_is_active(MooseClient *self) {
    g_assert(self);

    gboolean rc = false;

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    { rc = self->priv->command_list.is_active; }
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);

    return rc;
}

void moose_client_begin(MooseClient *self) {
    g_assert(self);

    moose_client_send_single(self, "command_list_begin");
}

long moose_client_commit(MooseClient *self) {
    g_assert(self);

    return moose_client_send_single(self, "command_list_end");
}

static void moose_update_context_info_cb(MooseClient *self, MooseIdle events) {
    if(self == NULL || events == 0 || moose_client_is_connected(self) == false) {
        return;
    }

    const gboolean update_status = events & ON_STATUS_UPDATE_FLAGS;
    const gboolean update_stats = events & ON_STATS_UPDATE_FLAGS;
    const gboolean update_song = events & ON_CURRENT_SONG_UPDATE_FLAGS;
    const gboolean update_rg = events & ON_REPLAYGAIN_UPDATE_FLAGS;

    if(!(update_status || update_stats || update_song || update_rg)) {
        return;
    }

    struct mpd_connection *conn = moose_client_get(self);

    if(conn == NULL) {
        moose_client_put(self);
        return;
    }

    MooseClientPrivate *priv = self->priv;

    /* Send a block of commands, speeds the thing up by 2x */
    mpd_command_list_begin(conn, true);
    {
        /* Note: order of recv == order of send. */
        if(update_status) {
            mpd_send_status(conn);
        }

        if(update_stats) {
            mpd_send_stats(conn);
        }

        if(update_rg) {
            mpd_send_command(conn, "replay_gain_status", NULL);
        }

        if(update_song) {
            mpd_send_current_song(conn);
        }
    }
    mpd_command_list_end(conn);
    moose_client_check_error(self, conn);

    /* Try to receive status */
    if(update_status) {
        struct mpd_status *tmp_status_struct;
        tmp_status_struct = mpd_recv_status(conn);

        g_mutex_lock(&self->priv->status_timer.mutex);
        {
            if(priv->status_timer.last_update != NULL) {
                /* Reset the status timer to 0 */
                g_timer_start(priv->status_timer.last_update);
            }
        }
        g_mutex_unlock(&self->priv->status_timer.mutex);

        if(tmp_status_struct) {
            MooseStatus *status = moose_client_ref_status(self);
            {
                if(priv->status != NULL) {
                    priv->last_song_data.id = moose_status_get_song_id(priv->status);
                    priv->last_song_data.state = moose_status_get_state(priv->status);
                } else {
                    priv->last_song_data.id = -1;
                    priv->last_song_data.state = MOOSE_STATE_UNKNOWN;
                }
            }
            moose_status_unref(status);

            /* TODO Locking? */

            /* Swap the value */
            g_rec_mutex_lock(&self->priv->client_attr_mutex);
            {
                MooseStatus *old_status = priv->status;
                priv->status = moose_status_new_from_struct(old_status, tmp_status_struct);
                moose_status_unref(old_status);
            }
            g_rec_mutex_unlock(&self->priv->client_attr_mutex);

            mpd_status_free(tmp_status_struct);
        }

        mpd_response_next(conn);
        moose_client_check_error(self, conn);
    }

    /* Try to receive statistics as last */
    if(update_stats) {
        struct mpd_stats *tmp_stats_struct;
        tmp_stats_struct = mpd_recv_stats(conn);

        if(tmp_stats_struct) {
            MooseStatus *status = moose_client_ref_status(self);
            { moose_status_update_stats(status, tmp_stats_struct); }
            moose_status_unref(status);
            mpd_stats_free(tmp_stats_struct);
        }

        mpd_response_next(conn);
        moose_client_check_error(self, conn);
    }

    /* Read the current replay gain status */
    if(update_rg) {
        struct mpd_pair *mode = mpd_recv_pair_named(conn, "replay_gain_mode");
        if(mode != NULL) {
            MooseStatus *status = moose_client_ref_status(self);
            if(status != NULL) {
                moose_status_set_replay_gain_mode(status, mode->value);
            }

            moose_status_unref(status);
            mpd_return_pair(conn, mode);
        }

        mpd_response_next(conn);
        moose_client_check_error(self, conn);
    }

    /* Try to receive the current song */
    if(update_song) {
        struct mpd_song *new_song_struct = mpd_recv_song(conn);
        MooseSong *new_song = moose_song_new_from_struct(new_song_struct);
        if(new_song_struct != NULL) {
            mpd_song_free(new_song_struct);
        }

        /* We need to call recv() one more time
         * so we end the songlist,
         * it should only return  NULL
         * */
        if(new_song_struct != NULL) {
            struct mpd_song *empty = mpd_recv_song(conn);
            g_assert(empty == NULL);
        }

        MooseStatus *status = moose_client_ref_status(self);
        moose_status_set_current_song(status, new_song);
        moose_status_unref(status);
        moose_client_check_error(self, conn);
    }

    /* Finish repsonse */
    if(update_song || update_stats || update_status || update_rg) {
        mpd_response_finish(conn);
        moose_client_check_error(self, conn);
    }

    moose_client_put(self);
}

void moose_priv_outputs_update(MooseClient *self, MooseIdle event) {
    g_assert(self);

    if((event & MOOSE_IDLE_OUTPUT) == 0) {
        return /* because of no relevant event */;
    }

    struct mpd_connection *conn = moose_client_get(self);

    if(conn != NULL) {
        if(mpd_send_outputs(conn) == false) {
            moose_client_check_error(self, conn);
        } else {
            struct mpd_output *output = NULL;
            MooseStatus *status = moose_client_ref_status(self);
            {
                moose_status_outputs_clear(status);
                while((output = mpd_recv_output(conn)) != NULL) {
                    moose_status_outputs_add(status, output);
                    mpd_output_free(output);
                }
            }
            moose_status_unref(status);
        }

        if(mpd_response_finish(conn) == false) {
            moose_client_check_error(self, conn);
        }
    }
    moose_client_put(self);
}

static gboolean moose_update_is_a_seek_event(MooseClient *self, MooseIdle event_mask) {
    if(event_mask & MOOSE_IDLE_PLAYER) {
        long curr_song_id = -1;
        MooseState curr_song_state = MOOSE_STATE_UNKNOWN;

        /* Get the current data */
        MooseStatus *status = moose_client_ref_status(self);
        {
            if(status != NULL) {
                curr_song_id = moose_status_get_song_id(status);
                curr_song_state = moose_status_get_state(status);
            }
        }
        moose_status_unref(status);

        if(curr_song_id != -1 && self->priv->last_song_data.id == curr_song_id) {
            if(self->priv->last_song_data.state == curr_song_state) {
                return true;
            }
        }
    }
    return false;
}

static gboolean moose_idle_client_event(gpointer user_data) {
    g_assert(user_data);
    MooseClientEventTag *tag = user_data;

    ASSERT_IS_MAINTHREAD(tag->self);

    g_signal_emit(tag->self, SIGNALS[SIGNAL_CLIENT_EVENT], 0, tag->event);
    g_object_unref(tag->self);
    g_free(tag);
    return FALSE; /* Remove this idle event */
}

static gpointer moose_update_thread(gpointer user_data) {
    g_assert(user_data);

    MooseClient *self = user_data;
    MooseIdle event_mask = 0;

    while((event_mask = GPOINTER_TO_INT(g_async_queue_pop(self->priv->event_queue))) !=
          MOOSE_THREAD_TERMINATOR) {
        moose_update_context_info_cb(self, event_mask);
        moose_priv_outputs_update(self, event_mask);

        /* Lookup if we need to trigger a client-event (maybe not if * auto-update)*/
        gboolean trigger_it = true;
        if(1 && (event_mask & MOOSE_IDLE_STATUS_TIMER_FLAG) &&
           !self->priv->status_timer.trigger_event) {
            trigger_it = false;
        }

        /* Maybe we should make this configurable? */
        if(moose_update_is_a_seek_event(self, event_mask)) {
            /* Set the PLAYER bit to 0 */
            event_mask &= ~MOOSE_IDLE_PLAYER;

            /* and add the SEEK bit instead */
            event_mask |= MOOSE_IDLE_SEEK;
        }

        if(trigger_it) {
            MooseClientEventTag *tag = g_new0(MooseClientEventTag, 1);

            /* Make sure client still exists on the other side. */
            tag->self = g_object_ref(self);
            tag->event = event_mask;

            /* Defer the execution on the mainthread */
            g_idle_add_full(G_PRIORITY_HIGH_IDLE, moose_idle_client_event, tag, NULL);
        }
    }

    return NULL;
}

struct mpd_connection *moose_base_connect(MooseClient *self, const char *host, int port,
                                          float timeout, char **err) {
    struct mpd_connection *con = mpd_connection_new(host, port, timeout * 1000);

    if(con && mpd_connection_get_error(con) != MPD_ERROR_SUCCESS) {
        char *error_message = g_strdup(mpd_connection_get_error_message(con));
        /* Report the error, but don't try to handle it in that early stage */
        moose_client_check_error_without_handling(self, con);

        if(err != NULL) {
            *err = error_message;
        } else {
            g_free(error_message);
        }

        mpd_connection_free(con);
        con = NULL;
    } else if(err != NULL) {
        *err = NULL;
    }

    return con;
}

//////////////////////////////////////////////////////////////
//                                                          //
//                   GObject Interface                      //
//                                                          //
//////////////////////////////////////////////////////////////

static void moose_client_init(MooseClient *self) {
    MooseClientPrivate *priv = self->priv = moose_client_get_instance_private(self);

    g_rec_mutex_init(&self->getput_mutex);
    g_rec_mutex_init(&priv->client_attr_mutex);
    g_mutex_init(&priv->status_timer.mutex);

    priv->is_virgin = true;
    priv->event_queue = g_async_queue_new();

    priv->last_song_data.id = -1;
    priv->last_song_data.state = MOOSE_STATE_UNKNOWN;

    priv->status_timer.only_when_playing = TRUE;
    priv->status_timer.trigger_event = TRUE;

    priv->initial_thread = g_thread_self();
    priv->update_thread = g_thread_new("data-update-thread", moose_update_thread, self);
}

MooseClient *moose_client_new(MooseProtocol protocol) {
    switch(protocol) {
    case MOOSE_PROTOCOL_COMMAND:
        return g_object_new(MOOSE_TYPE_CMD_CLIENT, NULL);
    case MOOSE_PROTOCOL_IDLE:
    default:
        return g_object_new(MOOSE_TYPE_IDLE_CLIENT, NULL);
    }
}

static void moose_client_finalize(GObject *gobject) {
    MooseClient *self = MOOSE_CLIENT(gobject);
    if(self == NULL) {
        return;
    }

    // TODO: Think about the shutdown order.
    ASSERT_IS_MAINTHREAD(self);

    MooseClientPrivate *priv = self->priv;

    if(moose_client_timer_get_active(self)) {
        moose_client_timer_set_active(self, false);
    }

    /* Push the termination sign to the Queue */
    g_async_queue_push(priv->event_queue, GINT_TO_POINTER(MOOSE_THREAD_TERMINATOR));

    /* Wait for the thread to finish */
    g_thread_join(priv->update_thread);

    /* Disconnect if not done yet */
    moose_client_disconnect(self);

    moose_status_unref(priv->status);
    priv->status = NULL;

    /* Destroy the Queue */
    g_async_queue_unref(priv->event_queue);

    priv->event_queue = NULL;
    priv->update_thread = NULL;

    g_mutex_clear(&priv->status_timer.mutex);

    /* Kill any previously connected host info */
    g_rec_mutex_lock(&priv->client_attr_mutex);
    if(priv->host != NULL) {
        g_free(priv->host);
        priv->host = NULL;
    }

    g_rec_mutex_unlock(&priv->client_attr_mutex);
    g_rec_mutex_clear(&self->getput_mutex);
    g_rec_mutex_clear(&priv->client_attr_mutex);

    G_OBJECT_CLASS(moose_client_parent_class)->finalize(gobject);
}

static void moose_client_get_property(GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec) {
    MooseClient *self = MOOSE_CLIENT(object);

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    {
        switch(property_id) {
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
        case PROP_TIMER_ONLY_WHEN_PLAYING:
            g_value_set_boolean(value, self->priv->status_timer.only_when_playing);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
        }
    }
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);
}

static void moose_client_set_property(GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec) {
    MooseClient *self = MOOSE_CLIENT(object);

    g_rec_mutex_lock(&self->priv->client_attr_mutex);
    {
        switch(property_id) {
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
            self->priv->status_timer.interval = g_value_get_float(value);
            break;
        case PROP_TIMER_TRIGGER_EVENT:
            self->priv->status_timer.trigger_event = g_value_get_boolean(value);
            break;
        case PROP_TIMER_ONLY_WHEN_PLAYING:
            self->priv->status_timer.only_when_playing = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
        }
    }
    g_rec_mutex_unlock(&self->priv->client_attr_mutex);
}

static void moose_client_class_init(MooseClientClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_client_finalize;
    gobject_class->get_property = moose_client_get_property;
    gobject_class->set_property = moose_client_set_property;

    /**
     * MooseClient::client-event:
     * @client: The client.
     * @events: MooseIdle events that happened.
     *
     * Called on every client event that mpd gave us.
     */
    SIGNALS[SIGNAL_CLIENT_EVENT] = g_signal_new(
        "client-event", G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__FLAGS, G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * MooseClient::connectivity:
     * @client: The client.
     * @server_changed: True if the now connected server differs to older one.
     *
     * server_changed is False on disconnect events.
     * Use moose_client_is_connected() to find out if it's connected.
     */
    SIGNALS[SIGNAL_CONNECTIVITY] = g_signal_new(
        "connectivity", G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    GParamSpec *pspec = NULL;
    pspec = g_param_spec_string("host",
                                "Hostname",
                                "Current host we're connected to",
                                NULL, /* default value */
                                G_PARAM_READABLE);

    /**
     * MooseClient:host: (type utf8)
     *
     * Hostname of the MPD Server we're connected to or NULL.
     */
    g_object_class_install_property(gobject_class, PROP_HOST, pspec);

    pspec = g_param_spec_int("port",
                             "Port",
                             "Port we're connected to",
                             0,        /* Minimum value */
                             G_MAXINT, /* Maximum value */
                             6600,     /* default value */
                             G_PARAM_READABLE);

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
        1,   /* Minimum value */
        100, /* Maximum value */
        20,  /* Default value */
        G_PARAM_READWRITE);

    /**
     * MooseClient:timeout: (type float)
     *
     * Timeout in seconds, after which an operation is cancelled.
     */
    g_object_class_install_property(gobject_class, PROP_TIMEOUT, pspec);

    pspec =
        g_param_spec_float("timer-interval",
                           "Timerinterval",
                           "How many seconds to wait before retrieving the next status",
                           0.05, /* Minimum value */
                           100,  /* Maximum value */
                           0.5,  /* Default value */
                           G_PARAM_READWRITE);

    /**
     * MooseClient:timer-interval: (type float)
     *
     * Timer Interval, after which the status is force-updated.
     */
    g_object_class_install_property(gobject_class, PROP_TIMER_INTERVAL, pspec);

    pspec = g_param_spec_boolean("timer-trigger",
                                 "Trigger event",
                                 "Emit a client-event when force-updating the Status?",
                                 TRUE,
                                 G_PARAM_READWRITE);

    /**
     * MooseClient:timer-interval: (type float)
     *
     * Timer Interval, after which the status is force-updated.
     */
    g_object_class_install_property(gobject_class, PROP_TIMER_TRIGGER_EVENT, pspec);

    pspec = g_param_spec_boolean("timer-only-when-playing",
                                 "Run only when playing",
                                 "Only update status when someting is playing currently?",
                                 TRUE,
                                 G_PARAM_READWRITE);

    /**
     * MooseClient:timer-interval: (type float)
     *
     * Timer Interval, after which the status is force-updated.
     */
    g_object_class_install_property(gobject_class, PROP_TIMER_ONLY_WHEN_PLAYING, pspec);
}

GType moose_idle_get_type(void) {
    static GType flags_type = 0;

    if(flags_type == 0) {
        static GFlagsValue idle_types[] = {
            {MOOSE_IDLE_DATABASE, "MOOSE_IDLE_DATABASE", "database"},
            {MOOSE_IDLE_STORED_PLAYLIST, "MOOSE_IDLE_STORED_PLAYLIST", "stored-playlist"},
            {MOOSE_IDLE_QUEUE, "MOOSE_IDLE_QUEUE", "queue"},
            {MOOSE_IDLE_PLAYER, "MOOSE_IDLE_PLAYER", "player"},
            {MOOSE_IDLE_MIXER, "MOOSE_IDLE_MIXER", "mixer"},
            {MOOSE_IDLE_OUTPUT, "MOOSE_IDLE_OUTPUT", "output"},
            {MOOSE_IDLE_OPTIONS, "MOOSE_IDLE_OPTIONS", "options"},
            {MOOSE_IDLE_UPDATE, "MOOSE_IDLE_UPDATE", "update"},
            {MOOSE_IDLE_STICKER, "MOOSE_IDLE_STICKER", "sticker"},
            {MOOSE_IDLE_SUBSCRIPTION, "MOOSE_IDLE_SUBSCRIPTION", "subscription"},
            {MOOSE_IDLE_MESSAGE, "MOOSE_IDLE_MESSAGE", "message"},
            {MOOSE_IDLE_SEEK, "MOOSE_IDLE_SEEK", "seek"},
            {MOOSE_IDLE_STATUS_TIMER_FLAG, "MOOSE_IDLE_STATUS_TIMER_FLAG", "status-timer"},
            {MOOSE_IDLE_ALL, "MOOSE_IDLE_ALL", "all"},
            {0, NULL, NULL}};

        flags_type = g_flags_register_static("MooseIdle", idle_types);
    }

    return flags_type;
}

GType moose_protocol_get_type(void) {
    static GType enum_type = 0;

    if(enum_type == 0) {
        static GEnumValue protocol_types[] = {
            {MOOSE_PROTOCOL_IDLE, "MOOSE_PROTOCOL_IDLE", "idle"},
            {MOOSE_PROTOCOL_COMMAND, "MOOSE_PROTOCOL_COMMAND", "command"},
            {MOOSE_PROTOCOL_DEFAULT, "MOOSE_PROTOCOL_DEFAULT", "default"},
            {0, NULL, NULL}};

        enum_type = g_enum_register_static("MooseProtocol", protocol_types);
    }

    return enum_type;
}
