#include "../../moose-config.h"
#include "../moose-mpd-client-private.h"
#include "moose-mpd-idle-core.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <mpd/async.h>
#include <mpd/parser.h>

/*
 * This code is based/inspired, but not copied from ncmpc, See this for reference:
 *
 *   http://git.musicpd.org/cgit/master/ncmpc.git/tree/src/gidle.c
 */

typedef struct _MooseIdleClientPrivate {
    /* Parent "class" */
    MooseClient logic;

    /* Normal connection to mpd, on top of async_mpd_conn */
    struct mpd_connection *con;

    /* the async connection being watched */
    struct mpd_async *async_mpd_conn;

    /* A Glib IO Channel used as a watch on async_mpd_conn */
    GIOChannel *async_chan;

    /* the id of a watch if active, or 0 */
    int watch_source_id;

    /* libmpdclient's helper for parsing async. recv'd lines */
    struct mpd_parser *parser;

    /* the io events we are currently polling for */
    enum mpd_async_event last_io_events;

    /* true if currently polling for events */
    gboolean is_in_idle_mode;

    /* true if being in the outside world */
    gboolean is_running_extern;

    /* Mutex with descriptive name */
    GMutex one_thread_only_mtx;

    /* Handle the incoming get/put? */
    gboolean handle_getput;

} MooseIdleClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(MooseIdleClient, moose_idle_client, MOOSE_TYPE_CLIENT);

static const int map_io_enums[][2] = {{G_IO_IN, MPD_ASYNC_EVENT_READ},
                                      {G_IO_OUT, MPD_ASYNC_EVENT_WRITE},
                                      {G_IO_HUP, MPD_ASYNC_EVENT_HUP},
                                      {G_IO_ERR, MPD_ASYNC_EVENT_ERROR}};

static const unsigned map_io_enums_size = sizeof(map_io_enums) / sizeof(const int) / 2;

enum mpd_async_event gio_to_mpd_async(GIOCondition condition) {
    int events = 0;

    for(unsigned i = 0; i < map_io_enums_size; i++)
        if(condition & map_io_enums[i][0]) {
            events |= map_io_enums[i][1];
        }

    return (enum mpd_async_event)events;
}

GIOCondition mpd_async_to_gio(enum mpd_async_event events) {
    int condition = 0;

    for(unsigned i = 0; i < map_io_enums_size; i++)
        if(events & map_io_enums[i][1]) {
            condition |= map_io_enums[i][0];
        }

    return (GIOCondition)condition;
}

/* Prototype */
static gboolean moose_idle_client_socket_event(GIOChannel *source, GIOCondition condition,
                                               gpointer data);

static void moose_idle_client_report_error(MooseIdleClient *self,
                                           G_GNUC_UNUSED enum mpd_error error,
                                           const char *error_msg) {
    g_assert(self);
    self->priv->is_in_idle_mode = FALSE;
    moose_critical("idle-error: %s", error_msg);
}

static gboolean moose_idle_client_check_and_report_async_error(MooseIdleClient *self) {
    g_assert(self);
    enum mpd_error error = mpd_async_get_error(self->priv->async_mpd_conn);

    if(error != MPD_ERROR_SUCCESS) {
        const char *error_msg = mpd_async_get_error_message(self->priv->async_mpd_conn);
        moose_idle_client_report_error(self, error, error_msg);
        moose_client_disconnect(MOOSE_CLIENT(self));
        return TRUE;
    }

    return FALSE;
}

static void moose_idle_client_add_watch_kitten(MooseIdleClient *self) {
    g_assert(self);

    MooseIdleClientPrivate *priv = self->priv;
    if(priv->watch_source_id == 0) {
        /* Add a GIOSource that watches the socket for IO */
        enum mpd_async_event events = mpd_async_events(priv->async_mpd_conn);
        priv->watch_source_id = g_io_add_watch(priv->async_chan,
                                               mpd_async_to_gio(events),
                                               moose_idle_client_socket_event,
                                               self);
        /* remember the events we're polling for */
        priv->last_io_events = events;
    }
}

static void moose_idle_client_remove_watch_kitten(MooseIdleClient *self) {
    MooseIdleClientPrivate *priv = self->priv;
    if(priv->watch_source_id != 0) {
        g_source_remove(priv->watch_source_id);
        priv->watch_source_id = 0;
        priv->last_io_events = 0;
    }
}

static gboolean moose_idle_client_leave(MooseIdleClient *self) {
    gboolean rc = true;

    if(self->priv->is_in_idle_mode == true && self->priv->is_running_extern == false) {
        /* Since we're sending actively, we do not want
         * to be informed about it. */
        moose_idle_client_remove_watch_kitten(self);
        MooseIdle event = 0;

        if((event = (MooseIdle)mpd_run_noidle(self->priv->con)) == 0) {
            rc = !moose_idle_client_check_and_report_async_error(self);
        } else {
            /* this event is a duplicate of an already reported one  */
        }

        /* Make calling twice not dangerous */
        self->priv->is_in_idle_mode = false;
    }

    return rc;
}

static void moose_idle_client_enter(MooseIdleClient *self) {
    if(self->priv->is_in_idle_mode == false && self->priv->is_running_extern == false) {
        /* Do not wait for a reply */
        if(mpd_async_send_command(self->priv->async_mpd_conn, "idle", NULL) == false) {
            moose_idle_client_check_and_report_async_error(self);
        } else {
            /* Let's when input happens */
            moose_idle_client_add_watch_kitten(self);
            /* Indicate we're idling now */
            self->priv->is_in_idle_mode = true;
        }
    }
}

static void moose_idle_client_dispatch_events(MooseIdleClient *self, MooseIdle events) {
    g_assert(self);
    /* Clients can now use the connection.
     * We need to take care though, so when they get/put the connection,
     * we should not fire a "noidle"/"idle" now.
     * So we set this flag to indicate we're running.
     * It is resette by moose_idle_client_enter().
     * */
    moose_client_force_sync((MooseClient *)self, events);

    self->priv->is_in_idle_mode = FALSE;

    /* reenter idle-mode (we did not leave by calling moose_idle_client_leave though!) */
    moose_idle_client_enter(self);
}

static gboolean moose_idle_client_process_received(MooseIdleClient *self) {
    g_assert(self);
    char *line = NULL;
    MooseIdle event_mask = 0;

    while((line = mpd_async_recv_line(self->priv->async_mpd_conn)) != NULL) {
        enum mpd_parser_result result = mpd_parser_feed(self->priv->parser, line);

        switch(result) {
        case MPD_PARSER_MALFORMED:
            moose_idle_client_report_error(self, MPD_ERROR_MALFORMED,
                                           "cannot parse malformed response");
            return false;

        case MPD_PARSER_SUCCESS:
            moose_idle_client_dispatch_events(self, event_mask);
            return true;

        case MPD_PARSER_ERROR:
            return !moose_idle_client_check_and_report_async_error(self);

        case MPD_PARSER_PAIR:
            if(strcmp(mpd_parser_get_name(self->priv->parser), "changed") == 0) {
                event_mask |=
                    mpd_idle_name_parse(mpd_parser_get_value(self->priv->parser));
            }

            break;
        }
    }

    return true;
}

static gboolean moose_idle_client_socket_event(GIOChannel *source, GIOCondition condition,
                                               gpointer data)
/* Called once something happens to the socket.
 * The exact event is in @condition.
 *
 * This is basically where all the magic happens.
 * */
{
    g_assert(source);
    g_assert(data);

    MooseIdleClient *self = (MooseIdleClient *)data;
    enum mpd_async_event events = gio_to_mpd_async(condition);

    /* We need to lock here because in the meantime a get/put
     * could happen from another thread. This could alter the state
     * of the client while we're processing
     */
    g_rec_mutex_lock(&self->parent.getput_mutex);

    if(mpd_async_io(self->priv->async_mpd_conn, events) == FALSE) {
        /* Failure during IO */
        return !moose_idle_client_check_and_report_async_error(self);
    }

    if(condition & G_IO_IN) {
        if(moose_idle_client_process_received(self) == false) {
            /* Just continue as if nothing happened */
        }
    }

    events = mpd_async_events(self->priv->async_mpd_conn);

    gboolean keep_notify = FALSE;
    if(events == 0) {
        /* No events need to be polled - so disable the watch
         * (I've never seen this happen though.
         * */
        self->priv->watch_source_id = 0;
        self->priv->last_io_events = 0;
        keep_notify = FALSE;
    } else if(events != self->priv->last_io_events) {
        /* different event-mask, so we cannot reuse the current watch */
        moose_idle_client_remove_watch_kitten(self);
        moose_idle_client_add_watch_kitten(self);
        keep_notify = FALSE;
    } else {
        /* Keep the old watch */
        keep_notify = TRUE;
    }

    /* Unlock again - We're now ready to take other
     * get/puts again
     */
    g_rec_mutex_unlock(&self->parent.getput_mutex);
    return keep_notify;
}

/* code that is shared for connect/disconnect */
static void moose_idle_client_reset_struct(MooseIdleClient *self) {
    /* Initially there is no watchkitteh, 0 tells us that */
    self->priv->watch_source_id = 0;
    /* Currently no io events */
    self->priv->last_io_events = 0;
    /* Set initial flag params */
    self->priv->is_in_idle_mode = FALSE;
    /* Indicate we're not running in extern code */
    self->priv->is_running_extern = FALSE;
}

static char *moose_idle_client_do_connect(MooseClient *parent, const char *host, int port,
                                          float timeout) {
    g_assert(parent);

    MooseIdleClient *self = MOOSE_IDLE_CLIENT(parent);

    g_mutex_lock(&self->priv->one_thread_only_mtx);

    char *error = NULL;
    self->priv->con = moose_base_connect(parent, host, port, timeout, &error);

    if(error != NULL) {
        goto failure;
    }

    /* Get the underlying async connection */
    self->priv->async_mpd_conn = mpd_connection_get_async(self->priv->con);
    int async_fd = mpd_async_get_fd(self->priv->async_mpd_conn);
    self->priv->async_chan = g_io_channel_unix_new(async_fd);

    if(self->priv->async_chan == NULL) {
        error = (char *)"Created GIOChannel is NULL (probably a bug!)";
        goto failure;
    }

    /* the parser object needs to be instantiated only once */
    self->priv->parser = mpd_parser_new();

    /* Set all required flags */
    moose_idle_client_reset_struct(self);

    /* Enter initial 'idle' loop */
    moose_idle_client_enter(self);

failure:
    g_mutex_unlock(&self->priv->one_thread_only_mtx);
    return g_strdup(error);
}

static gboolean moose_idle_client_do_is_connected(MooseClient *client) {
    MooseIdleClient *self = MOOSE_IDLE_CLIENT(client);

    g_mutex_lock(&self->priv->one_thread_only_mtx);
    gboolean result = (self && self->priv->con);
    g_mutex_unlock(&self->priv->one_thread_only_mtx);
    return result;
}

static struct mpd_connection *moose_idle_client_do_get(MooseClient *self) {
    g_assert(self);

    if(moose_idle_client_do_is_connected(self) &&
       moose_idle_client_leave(MOOSE_IDLE_CLIENT(self))) {
        return MOOSE_IDLE_CLIENT(self)->priv->con;
    } else {
        return NULL;
    }
}

static void moose_idle_client_do_put(MooseClient *self) {
    g_assert(self);

    if(moose_idle_client_do_is_connected(self)) {
        moose_idle_client_enter(MOOSE_IDLE_CLIENT(self));
    }
}

static gboolean moose_idle_client_do_disconnect(MooseClient *parent) {
    if(moose_idle_client_do_is_connected(parent)) {
        MooseIdleClient *self = MOOSE_IDLE_CLIENT(parent);

        g_mutex_lock(&self->priv->one_thread_only_mtx);

        /* Be nice and send a final noidle if needed */
        moose_idle_client_leave(self);

        /* Underlying async connection is free too */
        mpd_connection_free(self->priv->con);
        self->priv->con = NULL;
        self->priv->async_mpd_conn = NULL;

        if(self->priv->watch_source_id != 0) {
            g_source_remove(self->priv->watch_source_id);
            self->priv->watch_source_id = 0;
        }

        g_io_channel_unref(self->priv->async_chan);
        self->priv->async_mpd_conn = NULL;

        if(self->priv->parser != NULL) {
            mpd_parser_free(self->priv->parser);
        }

        /* Make the struct reconnect-able */
        moose_idle_client_reset_struct(self);
        g_mutex_unlock(&self->priv->one_thread_only_mtx);

        return TRUE;
    } else {
        return FALSE;
    }
}

static void moose_idle_client_init(MooseIdleClient *object) {
    MooseClient *parent = MOOSE_CLIENT(object);

    parent->do_disconnect = moose_idle_client_do_disconnect;
    parent->do_get = moose_idle_client_do_get;
    parent->do_put = moose_idle_client_do_put;
    parent->do_connect = moose_idle_client_do_connect;
    parent->do_is_connected = moose_idle_client_do_is_connected;

    MooseIdleClient *self = MOOSE_IDLE_CLIENT(object);
    self->priv = moose_idle_client_get_instance_private(self);
    g_mutex_init(&self->priv->one_thread_only_mtx);
}

static void moose_idle_client_finalize(GObject *gobject) {
    MooseIdleClient *self = MOOSE_IDLE_CLIENT(gobject);
    moose_idle_client_do_disconnect(MOOSE_CLIENT(self));
    g_mutex_clear(&self->priv->one_thread_only_mtx);

    MooseClient *parent = MOOSE_CLIENT(self);
    parent->do_disconnect = NULL;
    parent->do_get = NULL;
    parent->do_put = NULL;
    parent->do_connect = NULL;
    parent->do_is_connected = NULL;

    G_OBJECT_CLASS(moose_idle_client_parent_class)->finalize(gobject);
}

static void moose_idle_client_class_init(MooseIdleClientClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_idle_client_finalize;
}
