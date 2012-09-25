#include "idle_core.h"
#include "common.h"
#include "../../util/gasyncqueue-watch.h"
#include "../../util/sleep_grained.h"
#include "../../compiler.h"
#include "../protocol.h"
#include "../signal_helper.h"

#include <glib.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>

/* define to cast a parent connector to the
 * concrete idle connector
 */
#define child(obj) ((mc_CmndClient *)obj)

/* time to check between sleep if pinger thread
 * needs to be closed down 
 */
#define PING_SLEEP_TIMEOUT 700 // ms

///////////////////////
// Private Interface //
///////////////////////

typedef struct {
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

    /* Indicates if the ping thread is supposed to run.
     * We need to ping the server because of the connection-timeout ,,feature'',
     * Otherwise we'll get disconnected after (by default) ~60 seconds.
     * Setting this flag to false will stop running threads. */
    gboolean run_pinger;

    /* Actual thread for the pinger thread.
     * It is started on first connect,
     * and shut down once the Client is freed.
     * (not on disconnect!) */
    GThread * pinger_thread;

    /* Timeout in milliseconds, after which we'll get disconnected,
     * without sending a command. Note that this only affects the cmnd_con,
     * since only here this span may be reached. The idle_con will wait
     * for responses from the server, and has therefore this timeout disabled
     * on server-side.
     *
     * The ping-thread only exists to work solely against this purpose. */
    glong connection_timeout_ms;

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

    /*
     * We may not use mc_shelper_report_error here.
     * Why? It may call disconnect on fatal errors.
     * This would lead to joining this thread with itself,
     * which is deadly. */
    while (self->run_listener) {
        if ( (events = mpd_run_idle (self->idle_con) ) == 0) {
            mc_shelper_report_error_without_handling ( (mc_Client *) self, self->idle_con);
            break;
        }

        g_async_queue_push (self->event_queue, GINT_TO_POINTER (events) );
        mc_shelper_report_error_without_handling ( (mc_Client *) self, self->idle_con);
    }

    mc_shelper_report_progress ( (mc_Client *) self, true, "cmnd-core: Listener Thread exited.");
    return NULL;
}

//////////////////////

static void cmnder_create_glib_adapter (
    mc_CmndClient * self,
    GMainContext * context)
{
    if (self->watch_source_id == -1) {
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
    /* Interrupt the idling, and tell the idle thread to die. */
    self->run_listener = FALSE;
    mpd_send_noidle (self->idle_con);

    if (self->watch_source_id != -1) {
        g_source_remove (self->watch_source_id);

        /* join the idle thread.
         * This is a very good argument, to not call disconnect
         * (especially implicitely!) in the idle thread.
         *
         * Ever seen a thread that joins itself?
         */
        g_thread_join (self->listener_thread);
    }

    self->listener_thread = NULL;
    self->watch_source_id = -1;
}

///////////////////////

static void cmnder_reset (mc_CmndClient * self)
{
    if (self != NULL) {
        cmnder_shutdown_listener (self);

        if (self->idle_con) {
            mpd_connection_free (self->idle_con);
            self->idle_con = NULL;
        }

        if (self->event_queue) {
            g_async_queue_unref (self->event_queue);
            self->event_queue = NULL;
        }

        if (self->cmnd_con) {
            mpd_connection_free (self->cmnd_con);
            self->cmnd_con = NULL;
        }
    }
}
//////////////////////////

/* Loop, and send pings to the server once every connection_timeout_ms milliseconds.
 *
 * Why another thread? Because if we'd using a timeout event
 * (in the mainloop) the whole thing:
 *   a) would only work with the mainloop attached. (not to serious, but still)
 *   b) may interfer with other threads using the connection
 *   c) mc_proto_get() may deadlock, if the connection hasn't been previously unlocked.
 *      (this might happen if an client operation invokes the mainloop)
 *
 *
 * The ping-thread will also run while being disconnected, but will not do anything,
 * but sleeping a lot.
 */
static gpointer cmnder_ping_server (mc_CmndClient * self)
{
    g_assert (self);

    while (self->run_pinger) {
        mc_sleep_grained (MAX (self->connection_timeout_ms, 100) / 2,
                PING_SLEEP_TIMEOUT, &self->run_pinger);

        if (mc_proto_is_connected ( (mc_Client *) self) ) {
            mpd_connection * con = mc_proto_get ( (mc_Client *) self);
            if (con != NULL) {
                if (mpd_send_command (con, "ping", NULL) == false)
                    mc_shelper_report_error ( (mc_Client *) self, con);

                if (mpd_response_finish (con) == false)
                    mc_shelper_report_error ( (mc_Client *) self, con);
            }
            mc_proto_put ( (mc_Client *) self);

        }

        if (self->run_pinger) {
            mc_sleep_grained (MAX (self->connection_timeout_ms, 100) / 2,
                    PING_SLEEP_TIMEOUT, &self->run_pinger);
        }
    }
    return NULL;
}

//////////////////////////

static void cmnder_shutdown_pinger (mc_CmndClient * self)
{
    g_assert (self);
    g_assert (self->pinger_thread);

    self->run_pinger = FALSE;
    g_thread_join (self->pinger_thread);
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

    /* listener */
    self->run_listener = TRUE;
    self->watch_source_id = -1;
    self->event_queue = g_async_queue_new();
    cmnder_create_glib_adapter (self, context);

    /* ping thread */
    if (self->pinger_thread == NULL) {
        self->run_pinger = TRUE;
        self->pinger_thread = g_thread_new ("cmnd-core-pinger",
                                            (GThreadFunc) cmnder_ping_server, self);
    }

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
    g_assert (parent);

    mc_CmndClient * self = child (parent);
    cmnder_shutdown_pinger (self);

    memset (self, 0, sizeof (mc_CmndClient) );
    g_free (self);
}

//////////////////////
// Public Interface //
//////////////////////

mc_Client * mc_proto_create_cmnder (long connection_timeout_ms)
{
    /* Only fill callbacks here, no
     * actual data relied to mc_CmndClient
     * should be placed here!
     */
    mc_CmndClient * self = g_new0 (mc_CmndClient, 1);
    self->logic.do_disconnect = cmnder_do_disconnect;
    self->logic.do_get = cmnder_do_get;
    self->logic.do_put = NULL;
    self->logic.do_free = cmnder_do_free;
    self->logic.do_connect = cmnder_do_connect;
    self->logic.do_is_connected = cmnder_do_is_connected;

    /* Fallback to default when neg. number given */
    if (connection_timeout_ms < 0)
        self->connection_timeout_ms = 5 * 1000;
    else
        self->connection_timeout_ms = connection_timeout_ms;

    return (mc_Client *) self;
}
