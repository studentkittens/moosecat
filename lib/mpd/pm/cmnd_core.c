#include "idle_core.h"
#include "common.h"
#include "../../util/gasyncqueue-watch.h"
#include "../../compiler.h"
#include "../protocol.h"
#include "../signal_helper.h"

#include <glib.h>
#include <string.h>

/* define to cast a parent connector to the
 * concrete idle connector
 */
#define child(obj) ((mc_CmndClient *)obj)

///////////////////////
// Private Interface //
///////////////////////

typedef struct
{
    /* Parent struct */
    mc_Client logic;

    /* Connection to send commands */
    mpd_connection * cmnd_con;

    /* Connection to recv. events */
    mpd_connection * idle_con;

    /* Thread that polls idle_con */
    GThread * listener_thread;

    /* Queue used to communicte between
     * listener_thread and mainloop-thread */
    GAsyncQueue * event_queue;

    /* ID of the GAsyncQueueWatch */
    glong watch_source_id;

    /* Indicates that the listener runs,
     * if false it may still run, but will
     * terminate on next iteration */
    gboolean run_listener;
} mc_CmndClient;

//////////////////////

static mc_cc_hot gboolean cmnder_event_callback (
    GAsyncQueue * queue,
    gpointer user_data)
{
    mc_Client * self = user_data;
    enum mpd_idle events = 0;
    gpointer item = NULL;

    /* Pop all items from the queue that are in,
     * and b'or them into one single event.
     */
    while ( (item = g_async_queue_try_pop (queue) ) )
        events |= GPOINTER_TO_INT (item);

    if (events != 0)
        mc_shelper_report_client_event (self, events);

    return TRUE;

}

///////////////////

static mc_cc_hot gpointer cmnder_listener_thread (gpointer data)
{
    mc_CmndClient * self = child (data);
    enum mpd_idle events = 0;

    while (self->run_listener)
    {
        if ( (events = mpd_run_idle (self->idle_con) ) == 0)
        {
            mc_shelper_report_error ( (mc_Client *) self, self->idle_con);
            break;
        }

        g_async_queue_push (self->event_queue, GINT_TO_POINTER (events) );
        mc_shelper_report_error ( (mc_Client *) self, self->idle_con);
    }

    mc_shelper_report_progress ( (mc_Client *) self, "Cmnd: Listener Thread exited.");
    return NULL;
}

//////////////////////

static void cmnder_create_glib_adapter (
    mc_CmndClient * self,
    GMainContext * context)
{
    if (self->watch_source_id == -1)
    {
        /* Start the listener thread and set the Queue Watcher on it */
        self->listener_thread = g_thread_new ("listener", cmnder_listener_thread, self);
        self->watch_source_id = mc_async_queue_watch_new (
                                    self->event_queue,
                                    -1,
                                    cmnder_event_callback,
                                    self,
                                    context);
    }
}

///////////////////////

static void cmnder_shutdown_listener (mc_CmndClient * self)
{
    /*
    * This is a bit hacky,
    * send a random command to the mpd,
    * so the listener gets up and finds out
    * it shall stop
    *
    * TODO!
    */
    self->run_listener = FALSE;

    mpd_run_crossfade (self->cmnd_con, 0);

    if (self->watch_source_id != -1)
    {
        g_source_remove (self->watch_source_id);
        g_thread_join (self->listener_thread);
    }

    self->listener_thread = NULL;
    self->watch_source_id = -1;
}

///////////////////////

static void cmnder_reset (mc_CmndClient * self)
{
    if (self != NULL)
    {
        cmnder_shutdown_listener (self);

        if (self->idle_con)
        {
            mpd_connection_free (self->idle_con);
            self->idle_con = NULL;
        }

        if (self->event_queue)
        {
            g_async_queue_unref (self->event_queue);
            self->event_queue = NULL;
        }

        if (self->cmnd_con)
        {
            mpd_connection_free (self->cmnd_con);
            self->cmnd_con = NULL;
        }
    }
}

//////////////////////////
//// Public Callbacks ////
//////////////////////////

static char * cmnder_do_connect (
    mc_Client * parent,
    GMainContext * context,
    const char * host,
    int port,
    float timeout)
{
    char * error_message = NULL;
    mc_CmndClient * self = child (parent);

    self->cmnd_con = mpd_connect ( (mc_Client *) self, host, port, timeout, &error_message);
    if (error_message != NULL)
        return error_message;

    self->idle_con = mpd_connect ( (mc_Client *) self, host, port, timeout, &error_message);
    if (error_message != NULL)
        return error_message;

    self->run_listener = TRUE;
    self->watch_source_id = -1;
    self->event_queue = g_async_queue_new();
    cmnder_create_glib_adapter (self, context);

    return error_message;
}

///////////////////////

static bool cmnder_do_disconnect (mc_Client * self)
{
    cmnder_reset (child (self) );
    return true;
}

//////////////////////

static bool cmnder_do_is_connected (mc_Client * parent)
{
    mc_CmndClient * self = child (parent);
    return (self->idle_con && self->cmnd_con);
}

///////////////////////

static mpd_connection * cmnder_do_get (mc_Client * self)
{
    return child (self)->cmnd_con;
}

//////////////////////

static void cmnder_do_free (mc_Client * parent)
{
    mc_CmndClient * self = child (parent);
    memset (self, 0, sizeof (mc_CmndClient) );
    g_free (self);
}

//////////////////////
// Public Interface //
//////////////////////

mc_Client * mc_proto_create_cmnder (void)
{
    /* Only fill callbacks here, no
     * actual data relied to mc_CmndClient
     * may be placed here!
     */
    mc_CmndClient * self = g_new0 (mc_CmndClient, 1);
    self->logic.do_disconnect = cmnder_do_disconnect;
    self->logic.do_get = cmnder_do_get;
    self->logic.do_put = NULL;
    self->logic.do_free = cmnder_do_free;
    self->logic.do_connect = cmnder_do_connect;
    self->logic.do_is_connected = cmnder_do_is_connected;

    return (mc_Client *) self;
}
