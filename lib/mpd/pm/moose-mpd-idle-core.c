#include "moose-mpd-idle-core.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "moose-mpd-common.h"
#include "../moose-mpd-signal-helper.h"

#include <mpd/parser.h>

/*
 * This code is based/inspired but not copied from ncmpc, See this for reference:
 *
 *   http://git.musicpd.org/cgit/master/ncmpc.git/tree/src/gidle.c
 */


/* define to cast a parent conneself to the
 * concrete idle conneself
 */
#define child(obj) ((MooseIdleClient *)obj)

///////////////////////
// Private Interface //
///////////////////////


typedef struct {
    /* Parent "class" */
    MooseClient logic;

    /* Normal connection to mpd, on top of async_mpd_conn */
    mpd_connection *con;

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
    bool is_in_idle_mode;

    /* true if being in the outside world */
    bool is_running_extern;

    /* Mutex with descriptive name */
    GMutex one_thread_only_mtx;

} MooseIdleClient;

///////////////////////////////////////////

/* Prototype */
static gboolean idler_socket_event(GIOChannel *source, GIOCondition condition, gpointer data);

///////////////////////////////////////////

static void idler_report_error(
    MooseIdleClient *self,
    G_GNUC_UNUSED enum mpd_error error,
    const char *error_msg)
{
    g_assert(self);
    self->is_in_idle_mode = FALSE;
    self->is_running_extern = TRUE;
    {
        moose_signal_dispatch((MooseClient *) self, "logging", self, error_msg, MC_LOG_ERROR, FALSE);
    }
    self->is_running_extern = FALSE;
}

///////////////////////////////////////////

static bool idler_check_and_report_async_error(MooseIdleClient *self)
{
    g_assert(self);
    enum mpd_error error = mpd_async_get_error(self->async_mpd_conn);

    if (error != MPD_ERROR_SUCCESS) {
        const char *error_msg = mpd_async_get_error_message(self->async_mpd_conn);
        idler_report_error(self, error, error_msg);
        return TRUE;
    }

    return FALSE;
}

///////////////////////////////////////////

static void idler_add_watch_kitten(MooseIdleClient *self)
{
    g_assert(self);

    if (self->watch_source_id == 0) {
        /* Add a GIOSource that watches the socket for IO */
        enum mpd_async_event events = mpd_async_events(self->async_mpd_conn);
        self->watch_source_id = g_io_add_watch(
                                    self->async_chan,
                                    mpd_async_to_gio(events),
                                    idler_socket_event,
                                    self
                                );
        /* remember the events we're polling for */
        self->last_io_events = events;
    }
}


///////////////////////////////////////////

static void idler_remove_watch_kitten(MooseIdleClient *self)
{
    if (self->watch_source_id != 0) {
        g_source_remove(self->watch_source_id);
        self->watch_source_id = 0;
        self->last_io_events = 0;
    }
}

///////////////////////////////////////////

static bool idler_leave(MooseIdleClient *self)
{
    bool rc = true;

    if (self->is_in_idle_mode == true &&
            self->is_running_extern == false) {
        /* Since we're sending actively, we do not want
         * to be informed about it. */
        idler_remove_watch_kitten(self);
        enum mpd_idle event = 0;

        if ((event = mpd_run_noidle(self->con)) == 0) {
            rc = !idler_check_and_report_async_error(self);
        } else {
            /* this event is a duplicate of an already reported one  */
        }

        /* Make calling twice not dangerous */
        self->is_in_idle_mode = false;
    }

    return rc;
}

///////////////////////////////////////////

static void idler_enter(MooseIdleClient *self)
{
    if (self->is_in_idle_mode == false &&
            self->is_running_extern == false) {
        /* Do not wait for a reply */
        if (mpd_async_send_command(self->async_mpd_conn, "idle", NULL) == false) {
            idler_check_and_report_async_error(self);
        } else {
            /* Let's when input happens */
            idler_add_watch_kitten(self);
            /* Indicate we're idling now */
            self->is_in_idle_mode = true;
        }
    }
}

///////////////////////////////////////////

static void idler_dispatch_events(MooseIdleClient *self, enum mpd_idle events)
{
    g_assert(self);
    /* Clients can now use the connection.
     * We need to take care though, so when they get/put the connection,
     * we should not fire a "noidle"/"idle" now.
     * So we set this flag to indicate we're running.
     * It is resette by idler_enter().
     * */
    self->is_in_idle_mode = FALSE;
    self->is_running_extern = TRUE;
    {
        // moose_shelper_report_client_event((MooseClient *)self, events);
        moose_force_sync((MooseClient *)self, events);
    }
    self->is_running_extern = FALSE;
    /* reenter idle-mode (we did not leave by calling idler_leave though!) */
    idler_enter(self);
}

///////////////////////////////////////////

static bool idler_process_received(MooseIdleClient *self)
{
    g_assert(self);
    char *line = NULL;
    enum mpd_idle event_mask = 0;

    while ((line = mpd_async_recv_line(self->async_mpd_conn)) != NULL) {
        enum mpd_parser_result result = mpd_parser_feed(self->parser, line);

        switch (result) {
        case MPD_PARSER_MALFORMED:
            idler_report_error(self,
                               MPD_ERROR_MALFORMED,
                               "cannot parse malformed response"
            );
            return false;

        case MPD_PARSER_SUCCESS:
            idler_dispatch_events(self, event_mask);
            return true;

        case MPD_PARSER_ERROR:
            return !idler_check_and_report_async_error(self);

        case MPD_PARSER_PAIR:
            if (strcmp(mpd_parser_get_name(self->parser), "changed") == 0) {
                event_mask |= mpd_idle_name_parse(mpd_parser_get_value(self->parser));
            }

            break;
        }
    }

    return true;
}

///////////////////////////////////////////

static gboolean idler_socket_event(GIOChannel *source, GIOCondition condition, gpointer data)
/* Called once something happens to the socket.
 * The exact event is in @condition.
 *
 * This is basically where all the magic happens.
 * */
{
    g_assert(source);
    g_assert(data);

    MooseIdleClient *self = (MooseIdleClient *) data;
    enum mpd_async_event events = gio_to_mpd_async(condition);

    /* We need to lock here because in the meantime a get/put 
     * could happen from another thread. This could alter the state
     * of the client while we're processing
     */
    g_rec_mutex_lock(&self->logic._getput_mutex);

    if (mpd_async_io(self->async_mpd_conn, events) == FALSE) {
        /* Failure during IO */
        return !idler_check_and_report_async_error(self);
    }

    if (condition & G_IO_IN) {
        if (idler_process_received(self) == false) {
            /* Just continue as if nothing happened */
        }
    }

    events = mpd_async_events(self->async_mpd_conn);

    bool keep_notify = FALSE;
    if (events == 0) {
        /* No events need to be polled - so disable the watch
         * (I've never seen this happen though.
         * */
        self->watch_source_id = 0;
        self->last_io_events = 0;
        keep_notify = FALSE;
    } else if (events != self->last_io_events) {
        /* different event-mask, so we cannot reuse the current watch */
        idler_remove_watch_kitten(self);
        idler_add_watch_kitten(self);
        keep_notify = FALSE;
    } else {
        /* Keep the old watch */
        keep_notify = TRUE;
    }
    
    /* Unlock again - We're now ready to take other
     * get/puts again 
     */
    g_rec_mutex_unlock(&self->logic._getput_mutex);
    return keep_notify;
}

///////////////////////////////////////////

/* code that is shared for connect/disconnect */
static void idler_reset_struct(MooseIdleClient *self)
{
    /* Initially there is no watchkitteh, 0 tells us that */
    self->watch_source_id = 0;
    /* Currently no io events */
    self->last_io_events = 0;
    /* Set initial flag params */
    self->is_in_idle_mode = FALSE;
    /* Indicate we're not running in extern code */
    self->is_running_extern = FALSE;
}

///////////////////////////////////////////
/////////////////// API ///////////////////
///////////////////////////////////////////

static char *idler_do_connect(MooseClient *parent, GMainContext *context, const char *host, int port, float timeout)
{
    (void) context;
    g_assert(parent);

    MooseIdleClient *self = child(parent);

    g_mutex_lock(&self->one_thread_only_mtx);

    char *error = NULL;
    self->con = mpd_connect(parent, host, port, timeout, &error);

    if (error != NULL)
        goto failure;

    /* Get the underlying async connection */
    self->async_mpd_conn = mpd_connection_get_async(self->con);
    int async_fd = mpd_async_get_fd(self->async_mpd_conn);
    self->async_chan = g_io_channel_unix_new(async_fd);

    if (self->async_chan == NULL) {
        error = "Created GIOChannel is NULL (probably a bug!)";
        goto failure;
    }

    /* the parser object needs to be instantiated only once */
    self->parser = mpd_parser_new();

    /* Set all required flags */
    idler_reset_struct(self);

    /* Enter initial 'idle' loop */
    idler_enter(self);

failure:

    g_mutex_unlock(&self->one_thread_only_mtx);
    return g_strdup(error);
}

///////////////////////

static bool idler_do_is_connected(MooseClient * client)
{
    MooseIdleClient * self = child(client);

    g_mutex_lock(&self->one_thread_only_mtx);
    bool result = (self && self->con);
    g_mutex_unlock(&self->one_thread_only_mtx);
    return result;
}

///////////////////////

static mpd_connection *idler_do_get(MooseClient *self)
{
    g_assert(self);

    if (idler_do_is_connected(self) && idler_leave(child(self)))
        return child(self)->con;
    else
        return NULL;
}

///////////////////////

static void idler_do_put(MooseClient *self)
{
    g_assert(self);

    if (idler_do_is_connected(self)) {
        idler_enter(child(self));
    }
}

//////////////////////

static bool idler_do_disconnect(MooseClient *parent)
{
    if (idler_do_is_connected(parent)) {
        MooseIdleClient *self = child(parent);

        g_mutex_lock(&self->one_thread_only_mtx);

        /* Be nice and send a final noidle if needed */
        idler_leave(self);

        /* Underlying async connection is free too */
        mpd_connection_free(self->con);
        self->con = NULL;
        self->async_mpd_conn = NULL;

        if (self->watch_source_id != 0) {
            g_source_remove(self->watch_source_id);
        }

        g_io_channel_unref(self->async_chan);
        self->async_mpd_conn = NULL;

        if (self->parser != NULL) {
            mpd_parser_free(self->parser);
        }

        /* Make the struct reconnect-able */
        idler_reset_struct(self);
        g_mutex_unlock(&self->one_thread_only_mtx);

        return TRUE;
    } else {
        return FALSE;
    }
}

//////////////////////

static void idler_do_free(MooseClient *parent)
{
    g_assert(parent);

    /* Make sure wrong acces gets punished */
    MooseIdleClient *self = child(parent);
    g_mutex_clear(&self->one_thread_only_mtx);

    memset(self, 0, sizeof(MooseIdleClient));
    g_free(self);
}

//////////////////////
// Public Interface //
//////////////////////

MooseClient *moose_create_idler(void)
{
    MooseIdleClient *self = g_new0(MooseIdleClient, 1);

    /* Define the logic of this connector */
    self->logic.do_disconnect = idler_do_disconnect;
    self->logic.do_get = idler_do_get;
    self->logic.do_put = idler_do_put;
    self->logic.do_free = idler_do_free;
    self->logic.do_connect = idler_do_connect;
    self->logic.do_is_connected = idler_do_is_connected;

    g_mutex_init(&self->one_thread_only_mtx);
    return (MooseClient *) self;
}
