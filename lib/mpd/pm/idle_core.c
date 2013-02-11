#include "idle_core.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "../signal-helper.h"

#include <mpd/parser.h>

/*
 * This code is based/inspired but not copied from ncmpc, See this for reference:
 *
 *   http://git.musicpd.org/cgit/master/ncmpc.git/tree/src/gidle.c
 */


/* define to cast a parent conneself to the
 * concrete idle conneself
 */
#define child(obj) ((mc_IdleClient *)obj)

///////////////////////
// Private Interface //
///////////////////////


typedef struct {
    /* Parent "class" */
    mc_Client logic;

    /* Normal connection to mpd, on top of async_mpd_conn */
    mpd_connection * con;

    /* the async connection being watched */
    struct mpd_async * async_mpd_conn;

    /* A Glib IO Channel used as a watch on async_mpd_conn */
    GIOChannel * async_chan;

    /* the id of a watch if active, or 0 */
    int watch_source_id;

    /* libmpdclient's helper for parsing async. recv'd lines */
    struct mpd_parser * parser;

    /* the io events we are currently polling for */
    enum mpd_async_event last_io_events;

    /* true if currently polling for events */
    bool is_in_idle_mode;

    /* true if being in the outside world */
    bool is_running_extern;

} mc_IdleClient;

static void idler_add_watch_kitten(mc_IdleClient * self);
static void idler_remove_watch_kitten(mc_IdleClient * self);
static void idler_enter(mc_IdleClient * self);

///////////////////////////////////////////

static void idler_dispatch_events (mc_IdleClient * self, enum mpd_idle events) 
{
    g_print("Dispatching Event: %d\n", events);

    /* Clients can now use the connection.
     * We need to take care though, so when they get/put the connection,
     * we should not fire a "noidle"/"idle" now.
     * So we set this flag to indicate we're running.
     * It is resette by idler_enter().
     * */

    self->is_in_idle_mode = FALSE;
    self->is_running_extern = TRUE;
    mc_shelper_report_client_event ((mc_Client *)self, events);
    self->is_running_extern = FALSE;

    /* reenter idle-mode (we did not leave by calling idler_leave though!) */
    idler_enter(self);
}

///////////////////////////////////////////

static bool idler_process_received (mc_IdleClient * self)
{
    char *line = NULL;
    enum mpd_idle event_mask = 0;

    g_print("Starting to process line..\n");

    while ((line = mpd_async_recv_line(self->async_mpd_conn)) != NULL) {
        enum mpd_parser_result result = mpd_parser_feed(self->parser, line);

        switch (result) {
            case MPD_PARSER_MALFORMED:
                return false;
            case MPD_PARSER_SUCCESS:
                idler_dispatch_events (self, event_mask);
                return true;
            case MPD_PARSER_ERROR:
                return false;
            case MPD_PARSER_PAIR:
                if (strcmp(mpd_parser_get_name(self->parser), "changed") == 0) {
                    g_print("%s : %s\n", mpd_parser_get_name(self->parser), mpd_parser_get_value(self->parser));
                    event_mask |= mpd_idle_name_parse(mpd_parser_get_value(self->parser));
                }
                break;
        }
    }
    return true;
}

///////////////////////////////////////////

static gboolean idler_socket_event (GIOChannel * source, GIOCondition condition, gpointer data)
    /* Called once something happens to the socket. 
     * The exact event is in @condition.
     *
     * This is basically where all the magic happens.
     * */
{
    g_assert(source);
    g_assert(data);

    mc_IdleClient * self = (mc_IdleClient *) data;
    enum mpd_async_event events = gio_to_mpd_async(condition);

    if (mpd_async_io (self->async_mpd_conn, events) == FALSE) {
        /* Failure during IO */
        puts("IO, but an Error.");
        return FALSE;
    }
        
    if (condition & G_IO_IN) {
        idler_process_received (self);
    }

    events = mpd_async_events(self->async_mpd_conn);
    if (events == 0) {
        /* No events need to be polled - so disable the watch */
        self->watch_source_id = 0;
        self->last_io_events = 0;
        return FALSE;
    } else if (events != self->last_io_events) {
        /* different event-mask, so we cannot reuse the current watch */
        idler_remove_watch_kitten (self);
        idler_add_watch_kitten (self);
        return FALSE;
    } else {
        /* Keep the old watch */
        return TRUE;
    }
}

///////////////////////////////////////////

static void idler_add_watch_kitten(mc_IdleClient * self) 
{
    if (self->watch_source_id == 0) {
        /* Add a GIOSource that watches the socket for IO */
        enum mpd_async_event events = mpd_async_events (self->async_mpd_conn);
        self->watch_source_id = g_io_add_watch (
                self->async_chan,
                mpd_async_to_gio (events),
                idler_socket_event,
                self
        );

        /* remember the events we're polling for */
        self->last_io_events = events;
    }
}

///////////////////////////////////////////

static void idler_remove_watch_kitten(mc_IdleClient * self) 
{
    if (self->watch_source_id != 0) {
        g_source_remove (self->watch_source_id);
        self->watch_source_id = 0;
        self->last_io_events = 0;
    }
}

///////////////////////////////////////////

static void idler_leave(mc_IdleClient * self) 
{
    if (self->is_in_idle_mode == true &&
        self->is_running_extern == false) {
        /* Since we're sending actively, we do not want
         * to be informed about it. */
        idler_remove_watch_kitten (self);
        mpd_run_noidle(self->con);

        /* Make calling twice not dangerous */
        self->is_in_idle_mode = false;
    }
}

///////////////////////////////////////////

static void idler_enter(mc_IdleClient * self) 
{
    if (self->is_in_idle_mode == false &&
        self->is_running_extern == false) {
        /* Do not wait for a reply */
        mpd_async_send_command(self->async_mpd_conn, "idle", NULL);

        /* Let's when input happens */
        idler_add_watch_kitten (self);

        /* Indicate we're idling now */
        self->is_in_idle_mode = true;
    }
}
///////////////////////////////////////////

/* code that is shared for connect/disconnect */
static void idler_reset_struct (mc_IdleClient * self)
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

static char * idler_do_connect (mc_Client * parent, GMainContext * context, const char * host, int port, float timeout)
{
    (void) context;
    g_assert (parent);

    mc_IdleClient * self = child (parent);

    char * error = NULL;
    self->con = mpd_connect (parent, host, port, timeout, &error);
    if (error != NULL)
        goto failure;

    /* Get the underlying async connection */
    self->async_mpd_conn = mpd_connection_get_async (self->con);
    int async_fd = mpd_async_get_fd (self->async_mpd_conn);

    self->async_chan = g_io_channel_unix_new (async_fd);
    if (self->async_chan == NULL) {
        error = "Created GIOChannel is NULL (probably a bug!)";
        goto failure;
    }

    /* the parser object needs to be instantiated only once */
    self->parser = mpd_parser_new ();

    /* Set all required flags */
    idler_reset_struct (self);

    /* Enter initial 'idle' loop */
    idler_enter(self);

failure:
    return g_strdup(error);
}

///////////////////////

static mpd_connection * idler_do_get (mc_Client * self)
{
    puts("Getting connection");
    idler_leave(child (self));
    puts("Getting connection done");
    return child (self)->con;
}

///////////////////////

static void idler_do_put (mc_Client * self)
{
    puts("Putting connection");
    idler_enter(child (self));
    puts("Putting connection done");
}

///////////////////////

static bool idler_do_is_connected (mc_Client * self)
{
    g_print("Checking if connected...\n");
    return (self && child (self)->con);
}

//////////////////////

static bool idler_do_disconnect (mc_Client * parent)
{
    if (idler_do_is_connected (parent)) {
        g_print("Disconnecting.\n");

        mc_IdleClient * self = child (parent);

        /* Be nice and send a final noidle if needed */
        idler_leave (self);

        /* Underlying async connection is free too */
        mpd_connection_free (self->con);
        self->async_mpd_conn = NULL;

        if (self->watch_source_id != 0) {
            g_source_remove (self->watch_source_id);
        }

        g_io_channel_unref (self->async_chan);
        self->async_mpd_conn = NULL;

        if (self->parser != NULL) {
            mpd_parser_free (self->parser);
        }

        /* Make the struct reconnect-able */
        idler_reset_struct (self);

        return TRUE;
    } else {
        return FALSE;
    }
}

//////////////////////


static void idler_do_free (mc_Client * self)
{
    g_assert (self);

    idler_do_disconnect (self);

    memset (self, 0, sizeof (mc_IdleClient) );
    g_free (self);
}

//////////////////////
// Public Interface //
//////////////////////

mc_Client * mc_proto_create_idler (void)
{
    mc_IdleClient * self = g_new0 (mc_IdleClient, 1);

    /* Define the logic of this connector */
    self->logic.do_disconnect = idler_do_disconnect;
    self->logic.do_get = idler_do_get;
    self->logic.do_put = idler_do_put;
    self->logic.do_free = idler_do_free;
    self->logic.do_connect = idler_do_connect;
    self->logic.do_is_connected = idler_do_is_connected;

    return (mc_Client *) self;
}
