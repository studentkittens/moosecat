#include "moose-mpd-cmnd-core.h"
#include "../moose-mpd-client-private.h"
#include "../moose-mpd-client.h"
#include "../../moose-config.h"

#include <glib.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>

/* time to check between sleep if pinger thread
 * needs to be closed down
 */
#define PING_SLEEP_TIMEOUT 500 // ms

typedef struct _MooseCmdClientPrivate {
    /* Connection to send commands */
    struct mpd_connection * cmnd_con;

    /* Protexct get/set of self->cmnd_con */
    GMutex cmnd_con_mtx;

    /* Thread that polls idle_con */
    GThread * listener_thread;

    /* Table of listener threads in the format:
     *
     *  <GThread* 1>: [true|false]
     *  <GThread* 2>: [true|false]
     *  ...
     *
     *  Indicates if the key-thread should be running.
     *  There's at last one active thread. (The one in listener_thread or NULL)
     *  All other references are invalid and shall not be accessed.
     */
    GHashTable * run_listener_table;

    /* Indicates if the ping thread is supposed to run.
     * We need to ping the server because of the connection-timeout ,,feature'',
     * Otherwise we'll get disconnected after (by default) ~60 seconds.
     * Setting this flag to false will stop running threads. */
    volatile gboolean run_pinger;

    /* Actual thread for the pinger thread.
     * It is started on first connect,
     * and shut down once the Client is freed.
     * (not on disconnect!) */
    GThread * pinger_thread;

    /* Protect write read access
     * to the run_* flags
     */
    GMutex flagmtx_run_pinger;
    GMutex flagmtx_run_listener;

    GMutex sync_mutex;
    GCond sync_cond;

} MooseCmdClientPrivate;

typedef enum _MooseListenerState {
    LISTENER_NOT_RUNNING = FALSE,
    LISTENER_OK = 1,
    LISTENER_ERROR = 2
} MooseListenerState;

G_DEFINE_TYPE_WITH_PRIVATE(
    MooseCmdClient, moose_cmd_client, MOOSE_TYPE_CLIENT
);

/* We want to make helgrind happy. */
static gboolean moose_cmd_client_get_run_pinger(MooseCmdClient * self) {
    volatile gboolean result = false;
    g_mutex_lock(&self->priv->flagmtx_run_pinger);
    result = self->priv->run_pinger;
    g_mutex_unlock(&self->priv->flagmtx_run_pinger);

    return result;
}

static void moose_cmd_client_set_run_pinger(MooseCmdClient * self, volatile gboolean state) {
    g_mutex_lock(&self->priv->flagmtx_run_pinger); {
        self->priv->run_pinger = state;
    }
    g_mutex_unlock(&self->priv->flagmtx_run_pinger);
}

static gboolean moose_cmd_client_get_run_listener(MooseCmdClient * self, GThread * thread) {
    volatile gboolean result = false;
    g_mutex_lock(&self->priv->flagmtx_run_listener);
    result = GPOINTER_TO_INT(
                 g_hash_table_lookup(
                     self->priv->run_listener_table, thread
                 ));
    g_mutex_unlock(&self->priv->flagmtx_run_listener);

    return result;
}

static void moose_cmd_client_set_run_listener(MooseCmdClient * self, GThread * thread, volatile gboolean state) {
    g_mutex_lock(&self->priv->flagmtx_run_listener);
    g_hash_table_insert(self->priv->run_listener_table, thread, GINT_TO_POINTER(state));
    g_mutex_unlock(&self->priv->flagmtx_run_listener);
}

static gpointer moose_cmd_client_listener_thread(gpointer data) {
    MooseCmdClient * self = MOOSE_CMD_CLIENT(data);
    MooseClient * parent = MOOSE_CLIENT(self);
    MooseIdle events = 0;

    char * error_message = NULL;

    struct mpd_connection * idle_con =
        moose_base_connect(
            (MooseClient *)self,
            moose_client_get_host(parent),
            moose_client_get_port(parent),
            moose_client_get_timeout(parent),
            &error_message
        );

    /* Signal the connect function that we're almost up and running */
    g_mutex_lock(&self->priv->sync_mutex); {
        if (idle_con && error_message == NULL) {
            moose_cmd_client_set_run_listener(self, g_thread_self(), LISTENER_OK);
        } else {
            moose_cmd_client_set_run_listener(self, g_thread_self(), LISTENER_ERROR);
            moose_critical("listener_thread: cannot connect: %s", error_message);
            if(idle_con != NULL) {
                mpd_connection_free(idle_con);
                idle_con = NULL;
            }
        }
        g_cond_signal(&self->priv->sync_cond);
    }
    g_mutex_unlock(&self->priv->sync_mutex);

    while (idle_con && moose_cmd_client_get_run_listener(self, g_thread_self())) {
        if (mpd_send_idle(idle_con) == false) {
            if (mpd_connection_get_error(idle_con) == MPD_ERROR_TIMEOUT) {
                /* Sometimes a timeout is triggered.
                 * This is okay. I don't know why though.
                 * I only expect timeout on mpd_recv_idle, but well.
                 */
                mpd_connection_clear_error(idle_con);
            } else {
                moose_client_check_error(MOOSE_CLIENT(self), idle_con);
            }
        } else {
            if ((events = (MooseIdle)mpd_recv_idle(idle_con, false)) == 0) {
                if (mpd_connection_get_error(idle_con) == MPD_ERROR_TIMEOUT) {
                    mpd_connection_clear_error(idle_con);
                } else if(moose_client_check_error(MOOSE_CLIENT(self), idle_con)) {
                    break;
                }
            }

            if (events) {
                moose_client_force_sync((MooseClient *)self, events);
            }
        }
    }

    if(idle_con != NULL) {
        mpd_connection_free(idle_con);
        idle_con = NULL;
    }

    g_thread_unref(g_thread_self());
    return NULL;
}

static void moose_cmd_client_shutdown_pinger(MooseCmdClient * self) {
    g_assert(self);

    if (self->priv->pinger_thread != NULL) {
        moose_cmd_client_set_run_pinger(self, FALSE);
        g_thread_join(self->priv->pinger_thread);
        self->priv->pinger_thread = NULL;
    }
}

static void moose_cmd_client_shutdown_listener(MooseCmdClient * self) {
    /* Interrupt the idling, and tell the idle thread to die. */
    moose_cmd_client_set_run_listener(self, self->priv->listener_thread, FALSE);

    /* Note: Idle thread is not joined.
     *       It frees itself when it exits,
     *       and we told it to exit by calling
     *       moose_cmd_client_set_run_listener() avbove
     */
    self->priv->listener_thread = NULL;
}

static void moose_cmd_client_reset(MooseCmdClient * self) {
    if (self != NULL) {
        moose_cmd_client_shutdown_listener(self);

        g_mutex_lock(&self->priv->cmnd_con_mtx); {
            if (self->priv->cmnd_con) {
                mpd_connection_free(self->priv->cmnd_con);
                self->priv->cmnd_con = NULL;
            }
        }
        g_mutex_unlock(&self->priv->cmnd_con_mtx);
    }
}

void moose_cmd_client_sleep_grained(MooseCmdClient * self, unsigned ms, unsigned interval_ms) {
    g_assert(self);

    if (interval_ms == 0) {
        if (moose_cmd_client_get_run_pinger(self)) {
            g_usleep((ms) * 1000);
        }
    } else {
        unsigned n = ms / interval_ms;

        for (unsigned i = 0; i < n && moose_cmd_client_get_run_pinger(self); ++i) {
            g_usleep(interval_ms * 1000);
        }

        if (moose_cmd_client_get_run_pinger(self)) {
            g_usleep((ms % interval_ms) * 1000);
        }
    }
}

/* Loop, and send pings to the server once every connection_timeout_ms milliseconds.
 *
 * Why another thread? Because if we'd using a timeout event
 * (in the mainloop) the whole thing:
 *   a) would only work with the mainloop attached. (not to serious, but still)
 *   b) may interfer with other threads using the connection
 *   c) moose_client_get() may deadlock, if the connection hasn't been previously unlocked.
 *      (this might happen if an client operation invokes the mainloop)
 *
 *
 * The ping-thread will also run while being disconnected, but will not do anything,
 * but sleeping a lot.
 */
static gpointer moose_cmd_client_ping_server(MooseCmdClient * self) {
    g_assert(self);

    /* Timeout in milliseconds, after which we'll get disconnected,
     * without sending a command. Note that this only affects the cmnd_con,
     * since only here this span may be reached. The idle_con will wait
     * for responses from the server, and has therefore this timeout disabled
     * on server-side.
     *
     * The ping-thread only exists to work solely against this purpose. */
    int timeout_ms = MIN(MAX(2000, moose_client_get_timeout(MOOSE_CLIENT(self))), 20 * 1000);

    while (moose_cmd_client_get_run_pinger(self)) {
        moose_cmd_client_sleep_grained(self, timeout_ms / 2, PING_SLEEP_TIMEOUT);

        if (moose_cmd_client_get_run_pinger(self) == false) {
            break;
        }

        if (moose_client_is_connected((MooseClient *)self)) {
            struct mpd_connection * con = moose_client_get((MooseClient *)self);

            if (con != NULL) {
                if (mpd_send_command(con, "ping", NULL) == false) {
                    moose_client_check_error((MooseClient *)self, con);
                }

                if (mpd_response_finish(con) == false) {
                    moose_client_check_error((MooseClient *)self, con);
                }
            }

            moose_client_put((MooseClient *)self);
        }

        if (moose_cmd_client_get_run_pinger(self)) {
            moose_cmd_client_sleep_grained(self, timeout_ms, PING_SLEEP_TIMEOUT);
        }
    }

    return NULL;
}

static char * moose_cmd_client_do_connect(
    MooseClient * parent,
    const char * host,
    int port,
    float timeout) {

    char * error_message = NULL;
    MooseCmdClient * self = MOOSE_CMD_CLIENT(parent);
    MooseCmdClientPrivate * priv = self->priv;
    GError *error = NULL;

    /* start ping thread */
    if (priv->pinger_thread == NULL) {
        moose_cmd_client_set_run_pinger(self, TRUE);
        priv->pinger_thread = g_thread_new(
                                  "cmnd-core-pinger",
                                  (GThreadFunc)moose_cmd_client_ping_server,
                                  self
                              );
    }

    if(priv->listener_thread != NULL) {
        return NULL;
    }

    g_mutex_lock(&priv->cmnd_con_mtx);
    {
        priv->cmnd_con =
            moose_base_connect(
                (MooseClient *)self, host, port, timeout, &error_message
            );
    }

    if (error_message != NULL && priv->cmnd_con) {
        mpd_connection_free(priv->cmnd_con);
        priv->cmnd_con = NULL;
    }
    g_mutex_unlock(&priv->cmnd_con_mtx);

    /* Set it to not yet initialized */
    moose_cmd_client_set_run_listener(self, self->priv->listener_thread, LISTENER_NOT_RUNNING);

    /* Start thre thread */
    self->priv->listener_thread = g_thread_try_new(
                                      "listener-thread",
                                      moose_cmd_client_listener_thread,
                                      self,
                                      &error
                                  );

    if(error != NULL) {
        moose_critical("Unable to spawn listener thread: %s", error->message);
        g_error_free(error);
        return "No listener thread.";
    }

    /* Wait for the thread to be connected and running */
    g_mutex_lock(&self->priv->sync_mutex);
    MooseListenerState state;
    while((state = moose_cmd_client_get_run_listener(self, self->priv->listener_thread)) == LISTENER_NOT_RUNNING) {
        g_cond_wait(&self->priv->sync_cond, &self->priv->sync_mutex);
    }
    g_mutex_unlock(&self->priv->sync_mutex);
    
    if(state == LISTENER_ERROR) {
        error_message = "Was not able to start a listener-connection";
    }

    return error_message;
}

static gboolean moose_cmd_client_do_is_connected(MooseClient * parent) {
    MooseCmdClient * self = MOOSE_CMD_CLIENT(parent);

    g_mutex_lock(&self->priv->cmnd_con_mtx);
    struct mpd_connection * conn = self->priv->cmnd_con;
    g_mutex_unlock(&self->priv->cmnd_con_mtx);

    return (conn != NULL);
}

static gboolean moose_cmd_client_do_disconnect(MooseClient * parent) {
    if (moose_cmd_client_do_is_connected(parent)) {
        MooseCmdClient * self = MOOSE_CMD_CLIENT(parent);
        moose_cmd_client_reset(self);
        return true;
    } else {
        return false;
    }
}

static struct mpd_connection * moose_cmd_client_do_get(MooseClient * client) {
    return MOOSE_CMD_CLIENT(client)->priv->cmnd_con;
}

static void moose_cmd_client_do_put(MooseClient * self) {
    /* NOOP */
    (void)self;
}

static void moose_cmd_client_init(MooseCmdClient * object) {
    MooseClient * parent = MOOSE_CLIENT(object);
    parent->do_disconnect = moose_cmd_client_do_disconnect;
    parent->do_get = moose_cmd_client_do_get;
    parent->do_put = moose_cmd_client_do_put;
    parent->do_connect = moose_cmd_client_do_connect;
    parent->do_is_connected = moose_cmd_client_do_is_connected;

    MooseCmdClient * self = MOOSE_CMD_CLIENT(object);
    self->priv = moose_cmd_client_get_instance_private(self);
    self->priv->run_listener_table = g_hash_table_new(
                                         g_direct_hash, g_int_equal
                                     );

    g_mutex_init(&self->priv->cmnd_con_mtx);
    g_mutex_init(&self->priv->flagmtx_run_pinger);
    g_mutex_init(&self->priv->flagmtx_run_listener);
    g_mutex_init(&self->priv->sync_mutex);
    g_cond_init(&self->priv->sync_cond);
}

static void moose_cmd_client_finalize(GObject * gobject) {
    MooseCmdClient * self = MOOSE_CMD_CLIENT(gobject);
    if (self == NULL) {
        return;
    }

    moose_cmd_client_do_disconnect(MOOSE_CLIENT(self));

    /* Close the ping thread */
    moose_cmd_client_shutdown_pinger(self);

    g_hash_table_destroy(self->priv->run_listener_table);
    g_mutex_clear(&self->priv->cmnd_con_mtx);
    g_mutex_clear(&self->priv->flagmtx_run_pinger);
    g_mutex_clear(&self->priv->flagmtx_run_listener);
    g_mutex_clear(&self->priv->sync_mutex);
    g_cond_clear(&self->priv->sync_cond);

    /* Remove API */
    MooseClient * parent = MOOSE_CLIENT(gobject);
    parent->do_disconnect = NULL;
    parent->do_get = NULL;
    parent->do_put = NULL;
    parent->do_connect = NULL;
    parent->do_is_connected = NULL;

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

static void moose_cmd_client_class_init(MooseCmdClientClass * klass) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_cmd_client_finalize;
}
