#include "moose-mpd-cmnd-core.h"
#include "../moose-mpd-client-private.h"
#include "../moose-mpd-client.h"
#include "../../moose-config.h"

#include <glib.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <mpd/async.h>
#include <mpd/parser.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>

typedef enum _MooseListenerState {
    LISTENER_ERROR = -2,
    LISTENER_NOT_RUNNING = -1
} MooseListenerState;

typedef struct _MooseCmdClientPrivate {
    /* Connection to send commands */
    struct mpd_connection *cmnd_con;

    /* Protexct get/set of self->cmnd_con */
    GMutex cmnd_con_mtx;

    /* Thread that polls idle_con */
    GThread *listener_thread;

    /* The current state of the listener thread.
     * If this value is less than 0, it's using a member of MooseListenerState.
     * Otherwise the fd of the idle-socket is stored here.
     * On disconnect a "close\n" is sent to the fd.
     */
    volatile MooseListenerState run_listener;

    /* Queue read from the pinger thread,
     * once the queue is written to, the pinger thread exits.
     * */
    GAsyncQueue *run_pinger_queue;

    /* Actual thread for the pinger thread.
     * It is started on first connect,
     * and shut down once the Client is freed.
     * (not on disconnect!) */
    GThread *pinger_thread;

    /* Protect write read access
     * to the run_* flags
     * */
    GMutex flagmtx_run_listener;

    /* Mutex to sync between the two connections on connect.
     * We want to be sure that both connections are up and running
     * before returning to the caller */
    GMutex sync_mutex;

    /* The actual condition variable mentioned above */
    GCond sync_cond;

} MooseCmdClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(MooseCmdClient, moose_cmd_client, MOOSE_TYPE_CLIENT);

/* We want to make helgrind happy. */
static gboolean moose_cmd_client_get_run_listener(MooseCmdClient *self) {
    gboolean result = false;
    g_mutex_lock(&self->priv->flagmtx_run_listener);
    { result = self->priv->run_listener; }
    g_mutex_unlock(&self->priv->flagmtx_run_listener);

    return result;
}

static void moose_cmd_client_set_run_listener(MooseCmdClient *self,
                                              volatile gboolean state) {
    g_mutex_lock(&self->priv->flagmtx_run_listener);
    { self->priv->run_listener = state; }
    g_mutex_unlock(&self->priv->flagmtx_run_listener);
}

static enum mpd_async_event moose_sync_poll(struct mpd_async *async, struct timeval *tv) {
    fd_set rfds, wfds, efds;
    enum mpd_async_event events;
    unsigned fd = mpd_async_get_fd(async);

    if((events = mpd_async_events(async)) == 0) {
        return 0;
    }

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    if(events & MPD_ASYNC_EVENT_READ)
        FD_SET(fd, &rfds);
    if(events & MPD_ASYNC_EVENT_WRITE)
        FD_SET(fd, &wfds);
    if(events & (MPD_ASYNC_EVENT_HUP | MPD_ASYNC_EVENT_ERROR))
        FD_SET(fd, &efds);

    int ret = select(fd + 1, &rfds, &wfds, &efds, tv);
    if(ret > 0) {
        if(!FD_ISSET(fd, &rfds))
            events &= ~MPD_ASYNC_EVENT_READ;
        if(!FD_ISSET(fd, &wfds))
            events &= ~MPD_ASYNC_EVENT_WRITE;
        if(!FD_ISSET(fd, &efds))
            events &= ~(MPD_ASYNC_EVENT_HUP | MPD_ASYNC_EVENT_ERROR);

        return events;
    } else {
        return 0;
    }
}

static void check_async_error(struct mpd_async *async) {
    g_assert(async);

    enum mpd_error error = mpd_async_get_error(async);
    if(error != MPD_ERROR_SUCCESS) {
        const char *error_msg = mpd_async_get_error_message(async);
        moose_critical("cmd-async-error: %s\n", error_msg);
    }
}

static enum mpd_async_event moose_async_read_response(struct mpd_async *async) {
    enum mpd_async_event events = 0;
    char *line = NULL;

    struct mpd_parser *parser = mpd_parser_new();

    while((line = mpd_async_recv_line(async)) != NULL) {
        enum mpd_parser_result result = mpd_parser_feed(parser, line);

        switch(result) {
        case MPD_PARSER_MALFORMED:
            check_async_error(async);
            goto cleanup;
        case MPD_PARSER_SUCCESS:
            goto cleanup;
        case MPD_PARSER_ERROR:
            check_async_error(async);
            goto cleanup;
        case MPD_PARSER_PAIR:
            if(strcmp(mpd_parser_get_name(parser), "changed") == 0) {
                events |= mpd_idle_name_parse(mpd_parser_get_value(parser));
            }
            break;
        }
    }

cleanup:
    mpd_parser_free(parser);
    return events;
}

static gpointer moose_cmd_client_listener_thread(gpointer data) {
    MooseCmdClient *self = MOOSE_CMD_CLIENT(data);
    MooseClient *parent = MOOSE_CLIENT(self);

    char *error_message = NULL;
    char *host = moose_client_get_host(parent);
    float timeout = moose_client_get_timeout(parent);
    unsigned port = moose_client_get_port(parent);

    struct mpd_connection *idle_con =
        moose_base_connect((MooseClient *)self, host, port, timeout, &error_message);

    g_free(host);

    struct mpd_async *async = mpd_connection_get_async(idle_con);
    if(mpd_async_send_command(async, "idle", NULL) == FALSE) {
        check_async_error(async);
    }

    /* Signal the connect function that we're almost up and running */
    g_mutex_lock(&self->priv->sync_mutex);
    {
        if(idle_con && error_message == NULL) {
            moose_cmd_client_set_run_listener(self, mpd_async_get_fd(async));
        } else {
            moose_cmd_client_set_run_listener(self, LISTENER_ERROR);
            moose_critical("listener_thread: cannot connect: %s", error_message);
            if(idle_con != NULL) {
                mpd_connection_free(idle_con);
                idle_con = NULL;
            }
        }
        g_cond_signal(&self->priv->sync_cond);
    }
    g_mutex_unlock(&self->priv->sync_mutex);

    while(1) {
        struct timeval max_timeout = {
            .tv_sec = (int)timeout,
            .tv_usec = (int)(timeout * 1000 * 1000) % (1000 * 1000)};
        enum mpd_async_event events = moose_sync_poll(async, &max_timeout);

        if(events & MPD_ASYNC_EVENT_ERROR || events == 0) {
            check_async_error(async);
            continue;
        }

        if(events & MPD_ASYNC_EVENT_HUP) {
            check_async_error(async);
            break;
        }

        if(mpd_async_io(async, events) == FALSE) {
            moose_message("io-error (expected) - leaving");
            break;
        }

        if(events & MPD_ASYNC_EVENT_READ) {
            enum mpd_async_event events = moose_async_read_response(async);
            moose_client_force_sync(parent, (MooseIdle)events);
            mpd_async_send_command(async, "idle", NULL);
        }
    }

    moose_message("Leaving idle-loop");

    if(idle_con != NULL) {
        moose_cmd_client_set_run_listener(self, LISTENER_NOT_RUNNING);
        mpd_connection_free(idle_con);
        idle_con = NULL;
    }

    return NULL;
}

static void moose_cmd_client_shutdown_pinger(MooseCmdClient *self) {
    g_assert(self);

    GAsyncQueue *queue = self->priv->run_pinger_queue;
    g_async_queue_push(queue, queue);

    if(self->priv->pinger_thread != NULL) {
        g_thread_join(self->priv->pinger_thread);
        self->priv->pinger_thread = NULL;
    }
}

static void moose_cmd_client_reset(MooseCmdClient *self) {
    if(self != NULL) {
        /* Note: Idle thread is not joined.
        *       It frees itself when it exits,
        *       and we told it to exit by calling
        *       moose_cmd_client_set_run_listener() avbove
        */

        int idle_fd = moose_cmd_client_get_run_listener(self);
        if(idle_fd > 0) {
            const char command[] = "close\n";
            if(send(idle_fd, command, sizeof(command), 0) < 0) {
                moose_critical("Unable to send 'close': %s\n", g_strerror(errno));
            }
        }

        g_thread_join(self->priv->listener_thread);
        self->priv->listener_thread = NULL;

        moose_cmd_client_set_run_listener(self, LISTENER_NOT_RUNNING);

        g_mutex_lock(&self->priv->cmnd_con_mtx);
        {
            if(self->priv->cmnd_con) {
                mpd_connection_free(self->priv->cmnd_con);
                self->priv->cmnd_con = NULL;
            }
        }
        g_mutex_unlock(&self->priv->cmnd_con_mtx);
    }
}

/* Loop, and send pings to the server once every connection_timeout_ms milliseconds.
 *
 * Why another thread? Because if we'd using a timeout event
 * (in the mainloop) the whole thing:
 *   a) would only work with the mainloop attached. (not to serious, but still)
 *   b) may interfer with other threads using the connection
 *   c) moose_client_get() may deadlock, if the connection hasn't been previously
 *unlocked.
 *      (this might happen if an client operation invokes the mainloop)
 *
 *
 * The ping-thread will also run while being disconnected, but will not do anything,
 * but sleeping a lot.
 */
static gpointer moose_cmd_client_ping_server(MooseCmdClient *self) {
    g_assert(self);

    /* Timeout in milliseconds, after which we'll get disconnected,
     * without sending a command. Note that this only affects the cmnd_con,
     * since only here this span may be reached. The idle_con will wait
     * for responses from the server, and has therefore this timeout disabled
     * on server-side.
     *
     * The ping-thread only exists to work solely against this purpose. */

    int timeout_ms = moose_client_get_timeout(MOOSE_CLIENT(self));
    int timeout_micro = MIN(MAX(2000, timeout_ms), 20 * 1000) * 1000;

    while(g_async_queue_timeout_pop(self->priv->run_pinger_queue, timeout_micro) ==
          NULL) {
        if(moose_client_is_connected((MooseClient *)self)) {
            struct mpd_connection *con = moose_client_get((MooseClient *)self);
            if(con != NULL) {
                if(mpd_send_command(con, "ping", NULL) == false) {
                    moose_client_check_error((MooseClient *)self, con);
                }

                if(mpd_response_finish(con) == false) {
                    moose_client_check_error((MooseClient *)self, con);
                }
            }
            moose_client_put((MooseClient *)self);
        }
    }
    return NULL;
}

static char *moose_cmd_client_do_connect(MooseClient *parent,
                                         const char *host,
                                         int port,
                                         float timeout) {
    char *error_message = NULL;
    MooseCmdClient *self = MOOSE_CMD_CLIENT(parent);
    MooseCmdClientPrivate *priv = self->priv;
    GError *error = NULL;

    /* start ping thread */
    if(priv->pinger_thread == NULL) {
        priv->pinger_thread = g_thread_new(
            "cmnd-core-pinger", (GThreadFunc)moose_cmd_client_ping_server, self);
    }

    if(priv->listener_thread != NULL) {
        return NULL;
    }

    g_mutex_lock(&priv->cmnd_con_mtx);
    {
        priv->cmnd_con =
            moose_base_connect((MooseClient *)self, host, port, timeout, &error_message);
    }

    if(error_message != NULL && priv->cmnd_con) {
        mpd_connection_free(priv->cmnd_con);
        priv->cmnd_con = NULL;
    }
    g_mutex_unlock(&priv->cmnd_con_mtx);

    /* Set it to not yet initialized */
    moose_cmd_client_set_run_listener(self, LISTENER_NOT_RUNNING);

    /* Start thre thread */
    self->priv->listener_thread = g_thread_try_new(
        "listener-thread", moose_cmd_client_listener_thread, self, &error);

    if(error != NULL) {
        moose_critical("Unable to spawn listener thread: %s", error->message);
        g_error_free(error);
        return "No listener thread.";
    }

    /* Wait for the thread to be connected and running */
    MooseListenerState state;
    g_mutex_lock(&self->priv->sync_mutex);
    {
        while((state = moose_cmd_client_get_run_listener(self)) <= 0) {
            g_cond_wait(&self->priv->sync_cond, &self->priv->sync_mutex);
        }
    }
    g_mutex_unlock(&self->priv->sync_mutex);

    if(state == LISTENER_ERROR) {
        error_message = "Was not able to start a listener-connection";
    }

    return error_message;
}

static gboolean moose_cmd_client_do_is_connected(MooseClient *parent) {
    MooseCmdClient *self = MOOSE_CMD_CLIENT(parent);
    struct mpd_connection *conn = NULL;
    g_mutex_lock(&self->priv->cmnd_con_mtx);
    { conn = self->priv->cmnd_con; }
    g_mutex_unlock(&self->priv->cmnd_con_mtx);

    return (conn != NULL);
}

static gboolean moose_cmd_client_do_disconnect(MooseClient *parent) {
    if(moose_cmd_client_do_is_connected(parent)) {
        MooseCmdClient *self = MOOSE_CMD_CLIENT(parent);
        moose_cmd_client_reset(self);
        return true;
    } else {
        return false;
    }
}

static struct mpd_connection *moose_cmd_client_do_get(MooseClient *client) {
    return MOOSE_CMD_CLIENT(client)->priv->cmnd_con;
}

static void moose_cmd_client_do_put(MooseClient *self) {
    /* NOOP */
    (void)self;
}

static void moose_cmd_client_init(MooseCmdClient *object) {
    MooseClient *parent = MOOSE_CLIENT(object);
    parent->do_disconnect = moose_cmd_client_do_disconnect;
    parent->do_get = moose_cmd_client_do_get;
    parent->do_put = moose_cmd_client_do_put;
    parent->do_connect = moose_cmd_client_do_connect;
    parent->do_is_connected = moose_cmd_client_do_is_connected;

    MooseCmdClient *self = MOOSE_CMD_CLIENT(object);
    self->priv = moose_cmd_client_get_instance_private(self);

    self->priv->run_pinger_queue = g_async_queue_new();

    g_mutex_init(&self->priv->cmnd_con_mtx);
    g_mutex_init(&self->priv->flagmtx_run_listener);
    g_mutex_init(&self->priv->sync_mutex);
    g_cond_init(&self->priv->sync_cond);
}

static void moose_cmd_client_finalize(GObject *gobject) {
    MooseCmdClient *self = MOOSE_CMD_CLIENT(gobject);
    if(self == NULL) {
        return;
    }

    /* Close the ping thread */
    moose_cmd_client_shutdown_pinger(self);
    moose_cmd_client_do_disconnect(MOOSE_CLIENT(self));

    g_async_queue_unref(self->priv->run_pinger_queue);

    g_mutex_clear(&self->priv->cmnd_con_mtx);
    g_mutex_clear(&self->priv->flagmtx_run_listener);
    g_mutex_clear(&self->priv->sync_mutex);
    g_cond_clear(&self->priv->sync_cond);

    /* Remove API */
    MooseClient *parent = MOOSE_CLIENT(gobject);
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

static void moose_cmd_client_class_init(MooseCmdClientClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_cmd_client_finalize;
}
