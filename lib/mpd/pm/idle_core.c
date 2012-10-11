#include "idle_core.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "../signal_helper.h"

#include <mpd/parser.h>

/* define to cast a parent conneself to the
 * concrete idle conneself
 */
#define child(obj) ((mc_IdleClient *)obj)

///////////////////////
// Private Interface //
///////////////////////

typedef struct {
    mc_Client logic;
    mpd_connection * con;

    struct mpd_async * async;
    struct mpd_parser * parser;
    enum mpd_async_event io_eventmask;
    enum mpd_idle idle_events;
    GIOChannel * async_chan;
    guint async_chan_id;

    gboolean is_idling;
    GMainContext * main_context;

} mc_IdleClient;

///////////////////

static gboolean idler_check_error (mc_IdleClient * self)
{
    g_assert (self);

    if (mpd_async_get_error (self->async) != MPD_ERROR_SUCCESS) {
        g_print ("Async Error: %s\n", mpd_async_get_error_message (self->async));
        self->io_eventmask = 0;
        return true;
    } else {
        return false;
    }
}

///////////////////

static gboolean idler_parse_response (mc_IdleClient * self, char * netline)
{
    switch(mpd_parser_feed(self->parser, netline))
    {
        case MPD_PARSER_ERROR:
            {
                self->io_eventmask = 0;
                idler_check_error(self);
                g_print("ParserError: %d - %s",
                        mpd_parser_get_server_error(self->parser),
                        mpd_parser_get_message(self->parser));

                return false;
            }
        case MPD_PARSER_MALFORMED:
            {
                self->io_eventmask = 0;
                idler_check_error(self);
                return false;
            }
        case MPD_PARSER_PAIR:
            {
                const char * name = NULL;
                if(g_strcmp0((name = mpd_parser_get_name(self->parser)), "changed") == 0)
                {
                    const char * value = mpd_parser_get_value(self->parser);
                    self->idle_events |= mpd_idle_name_parse(value);
                }
                return true;
            }
        case MPD_PARSER_SUCCESS:
            {
                self->io_eventmask = 0;
                idler_check_error(self);
                return true;
            }
    }

    /* not reached */
    return true;
}

///////////////////

static gboolean idler_recv_parseable(mc_IdleClient * self)
{
    char * line = NULL;
    gboolean retval = true;

    while((line = mpd_async_recv_line(self->async)) != NULL)
    {
        if(idler_parse_response(self, line) == false)
        {
            retval = false;
            break;
        }
    }

    idler_check_error(self);
    return retval;
}

///////////////////

static void idler_create_socket_poll (mc_IdleClient * self, enum mpd_async_event events);

static gboolean idler_gio_socket_func (GIOChannel * source, GIOCondition condition, gpointer data)
{
    (void) source;
    g_assert (data);

    mc_IdleClient * self = data;
    mpd_async_event actual_event = gio_to_mpd_async(condition);

    if(mpd_async_io(self->async, actual_event) == false)
    {
        idler_check_error(self);
        return false;
    }

    if(condition & G_IO_IN)
    {
        if(idler_recv_parseable(self) == false)
        {
            g_print("Could not parse response.\n");
            return false;
        }
    }

    enum mpd_async_event events = mpd_async_events(self->async);
    if(events == 0)
    {
        g_print("no events -> removing watch\n");
        /* no events - disable watch */
        self->io_eventmask = 0;
        return false;
    }
    else if(events != self->io_eventmask)
    {
        idler_create_socket_poll(self, events);
        self->io_eventmask = events;
        return false;
    }

    return true;
}

///////////////////

static void idler_remove_socket_poll (mc_IdleClient * self)
{
    if (self->async_chan_id != 0) {
        if (g_source_remove (self->async_chan_id) == FALSE)
            g_print ("Warning: Cannot remove source from mainloop\n");

        self->async_chan_id = -1;
    }
}

///////////////////

static void idler_create_socket_poll (mc_IdleClient * self, enum mpd_async_event events)
{
    idler_remove_socket_poll (self);
    self->async_chan_id = g_io_add_watch (
            self->async_chan,
            mpd_async_to_gio (events),
            idler_gio_socket_func,
            NULL);
}

///////////////////

static gboolean idler_is_idling (mc_IdleClient * self)
{
    return self->is_idling;
}


///////////////////

static gboolean idler_enter (mc_IdleClient * self)
{
    if(idler_is_idling(self) == false)
    {
        //if(is_leaving || is_working)
        //    return false;

        g_print("Putting connection into 'idle' state.");
        if(mpd_async_send_command(self->async, "idle", NULL) == false)
        {
            idler_check_error(self);
            return false;
        }

        /* Indicate we get into idlemode */
        self->is_idling = true;

        /* Add a watch on the socket */
        idler_create_socket_poll(self, mpd_async_events(self->async));
        return true;
    }
    else
    {
        g_print("Cannot enter idling mode: Already idling.\n");
        return false;
    }
}

///////////////////

static gboolean idler_leave (mc_IdleClient * self)
{
    if(idler_is_idling(self)) {
        self->is_idling = false;
        self->io_eventmask = 0;

        enum mpd_idle events = 0;

        if(self->idle_events == 0) {
            events = mpd_run_noidle(self->con);
        }

        /* Check for errors that may happened shortly */
        if (mc_shelper_report_error ((mc_Client *)self, self->con) == false)
        {
            self->idle_events |= events;
            //invoke_user_callback();

            idler_remove_socket_poll (self);
        }
    }

    return true;
}

///////////////////

gboolean idler_enter_on_idle_wrapper (gpointer data)
{
    idler_enter ((mc_IdleClient *) data);
    return false; /* execute only once */
}

///////////////////////////////////////////
/////////////////// API ///////////////////
///////////////////////////////////////////

static char * idler_do_connect (mc_Client * parent, GMainContext * context, const char * host, int port, float timeout)
{
    g_assert (context);
    g_assert (parent);

    mc_IdleClient * self = child(parent);

    char * err = NULL;
    self->con = mpd_connect (parent, host, port, timeout, &err);
    if (err != NULL)
        return err;

    self->async = mpd_connection_get_async (self->con);
    int async_fd = mpd_async_get_fd (self->async);
    if (async_fd >= 0) {
        self->async_chan = g_io_channel_unix_new (async_fd);
    } else {
        return g_strdup("Unable to create an GIOChannel on the unix socket.");
    }

    self->main_context = context;
    self->parser = mpd_parser_new ();
    self->io_eventmask = 0;
    self->idle_events = 0;
    self->async_chan_id = 0;

    /* This is called exactly once, 
     * when the mainloop starts working.
     * Otherwise no work at all is done.
     */
    g_idle_add (idler_enter_on_idle_wrapper, self);

    return NULL;
}

///////////////////////

static mpd_connection * idler_do_get (mc_Client * self)
{
    return (idler_leave (child(self))) ? child (self)->con : NULL;
}

///////////////////////

static void idler_do_put (mc_Client * self)
{
    // enter event listening state
    g_print ("Putting %p back\n", child (self) );
}

///////////////////////

static bool idler_do_disconnect (mc_Client * self)
{
    (void) self;
    return true;
}

//////////////////////

static bool idler_do_is_connected (mc_Client * self)
{
    return (self && child(self)->con);
}

//////////////////////

static void idler_do_free (mc_Client * self)
{
    g_assert (self);

    if (self != NULL) {
        memset (self, 0, sizeof (mc_IdleClient) );
        g_free (self);
    }
}

//////////////////////
// Public Interface //
//////////////////////

mc_Client * mc_proto_create_idler (void)
{
    mc_IdleClient * self = g_new0 (mc_IdleClient, 1);
    self->logic.do_disconnect = idler_do_disconnect;
    self->logic.do_get = idler_do_get;
    self->logic.do_put = idler_do_put;
    self->logic.do_free = idler_do_free;
    self->logic.do_connect = idler_do_connect;
    self->logic.do_is_connected = idler_do_is_connected;

    // ... for now just abort here.
    g_print ("NOTE: MC_PM_IDLE pm is not yet implemented!\n");
    g_print ("      Kittens didn't walk over the keybaord yet.\n");
    g_print ("      => Aborting!\n\n");
    raise (SIGTERM);

    return (mc_Client *) self;
}
