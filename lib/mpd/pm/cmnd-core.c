#include "cmnd-core.h"
#include "common.h"
#include "../../util/gasyncqueue-watch.h"
#include "../../util/sleep-grained.h"
#include "../../compiler.h"
#include "../protocol.h"
#include "../signal-helper.h"

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
#define PING_SLEEP_TIMEOUT 500 // ms

///////////////////////
// Private Interface //
///////////////////////

typedef struct {
    /* Parent struct */
    mc_Client logic;

    /* Connection to send commands */
    mpd_connection *cmnd_con;

    /* Connection to recv. events */
    mpd_connection *idle_con;

    /* Thread that polls idle_con */
    GThread *listener_thread;

    /* Indicates that the listener runs,
     * if false it may still run, but will
     * terminate on next iteration */
    volatile gboolean run_listener;

    /* Indicates if the ping thread is supposed to run.
     * We need to ping the server because of the connection-timeout ,,feature'',
     * Otherwise we'll get disconnected after (by default) ~60 seconds.
     * Setting this flag to false will stop running threads. */
    volatile gboolean run_pinger;

    /* Actual thread for the pinger thread.
     * It is started on first connect,
     * and shut down once the Client is freed.
     * (not on disconnect!) */
    GThread *pinger_thread;

    /* Timeout in milliseconds, after which we'll get disconnected,
     * without sending a command. Note that this only affects the cmnd_con,
     * since only here this span may be reached. The idle_con will wait
     * for responses from the server, and has therefore this timeout disabled
     * on server-side.
     *
     * The ping-thread only exists to work solely against this purpose. */
    glong connection_timeout_ms;

    /* Protect write read access 
     * to the run_* flags
     */ 
    GMutex flagmtx_run_pinger;
    GMutex flagmtx_run_listener;

} mc_CmndClient;

//////////////////////

/* We want to make helgrind happy. */

static bool cmnder_get_run_pinger(mc_CmndClient * self)
{
    volatile bool result = false;
    g_mutex_lock(&self->flagmtx_run_pinger);
    result = self->run_pinger;    
    g_mutex_unlock(&self->flagmtx_run_pinger);

    return result;
}

static void cmnder_set_run_pinger(mc_CmndClient * self, volatile bool state)
{
    g_mutex_lock(&self->flagmtx_run_pinger);
    self->run_pinger = state;
    g_mutex_unlock(&self->flagmtx_run_pinger);
}

static bool cmnder_get_run_listener(mc_CmndClient * self)
{
    volatile bool result = false;
    g_mutex_lock(&self->flagmtx_run_listener);
    result = self->run_listener;    
    g_mutex_unlock(&self->flagmtx_run_listener);

    return result;
}

static void cmnder_set_run_listener(mc_CmndClient * self, volatile bool state)
{
    g_mutex_lock(&self->flagmtx_run_listener);
    self->run_listener = state;
    g_mutex_unlock(&self->flagmtx_run_listener);
}

///////////////////

static mc_cc_hot gpointer cmnder_listener_thread(gpointer data)
{
    mc_CmndClient *self = child(data);
    enum mpd_idle events = 0;

    while (cmnder_get_run_listener(self)) {
        /* Well, let's just be honest. This is retarted. 
         * 
         * This works fine when most of the time, 
         * but when disconnection we have to wake up this cinderella.
         * We currently do this in a bit of a retarted way. 
         *
         * We send this over cmnd_con:
         *
         *      command_list_begin
         *      repeat !$(current_state)
         *      repeat $(current_state)
         *      command_list_end
         *
         * This triggers an event that is waking up mpd_run_idle().
         * Yes, really. I wanted to be honest with you, dear reader.
         */

        if ((events = mpd_run_idle(self->idle_con)) == 0) {
            mc_shelper_report_error((mc_Client *) self, self->idle_con);
            break;
        }

        if(cmnder_get_run_listener(self)) {
            mc_shelper_report_client_event((mc_Client *) self, events);
        }
    }

    return NULL;
}

//////////////////////

static void cmnder_create_glib_adapter(
    mc_CmndClient *self,
    GMainContext *context)
{
    if (self->listener_thread == NULL) {
        /* Start the listener thread and set the Queue Watcher on it */
        self->listener_thread = g_thread_new("listener", cmnder_listener_thread, self);
    }
}

//////////////////////////

static void cmnder_shutdown_pinger(mc_CmndClient *self)
{
    g_assert(self);

    if (self->pinger_thread != NULL) {
        cmnder_set_run_pinger(self, FALSE);
        g_thread_join(self->pinger_thread);
        self->pinger_thread = NULL;
    }
}

///////////////////////

static void cmnder_shutdown_listener(mc_CmndClient *self)
{
    /* Interrupt the idling, and tell the idle thread to die. */
    cmnder_set_run_listener(self, FALSE);

    /* Ugly hack, see cmnder_listener_thread() */
    struct mpd_status * status =  mpd_run_status(self->cmnd_con);
    if(status != NULL) {
        mpd_command_list_begin(self->cmnd_con, false);
        mpd_send_repeat(self->cmnd_con, !mpd_status_get_repeat(status));
        mpd_send_repeat(self->cmnd_con, mpd_status_get_repeat(status));
        mpd_command_list_end(self->cmnd_con);
        mpd_status_free(status);
    } 

    /* This hack takes a bit time to have a effect. 
     * in the meantime we can wait for the ping thread 
     * to close */
    cmnder_shutdown_pinger(self);

    /* join the idle thread.
     * This is a very good argument, to not call disconnect
     * (especially implicitely!) in the idle thread.
     *
     * Ever seen a thread that joins itself?
     */
    if (self->listener_thread != NULL) {
        g_thread_join(self->listener_thread);
    }

    self->listener_thread = NULL;
}

///////////////////////

static void cmnder_reset(mc_CmndClient *self)
{
    if (self != NULL) {
        cmnder_shutdown_listener(self);

        if (self->idle_con) {
            mpd_connection_free(self->idle_con);
            self->idle_con = NULL;
        }

        if (self->cmnd_con) {
            mpd_connection_free(self->cmnd_con);
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
 *   c) mc_get() may deadlock, if the connection hasn't been previously unlocked.
 *      (this might happen if an client operation invokes the mainloop)
 *
 *
 * The ping-thread will also run while being disconnected, but will not do anything,
 * but sleeping a lot.
 */
static gpointer cmnder_ping_server(mc_CmndClient *self)
{
    g_assert(self);

    while (cmnder_get_run_pinger(self)) {
        mc_sleep_grained(MAX(self->connection_timeout_ms, 100) / 2,
                         PING_SLEEP_TIMEOUT, &self->run_pinger);

        if (cmnder_get_run_pinger(self) == false) {
            break;
        }

        if (mc_is_connected((mc_Client *) self)) {
            mpd_connection *con = mc_get((mc_Client *) self);

            if (con != NULL) {
                if (mpd_send_command(con, "ping", NULL) == false)
                    mc_shelper_report_error((mc_Client *) self, con);

                if (mpd_response_finish(con) == false)
                    mc_shelper_report_error((mc_Client *) self, con);
            }

            mc_put((mc_Client *) self);
        }

        if (cmnder_get_run_pinger(self)) {
            mc_sleep_grained(MAX(self->connection_timeout_ms, 100) / 2,
                             PING_SLEEP_TIMEOUT, &self->run_pinger);
        }
    }


    return NULL;
}

//////////////////////////
//// Public Callbacks ////
//////////////////////////

static char *cmnder_do_connect(
    mc_Client *parent,
    GMainContext *context,
    const char *host,
    int port,
    float timeout)
{
    char *error_message = NULL;
    mc_CmndClient *self = child(parent);
    self->cmnd_con = mpd_connect((mc_Client *) self, host, port, timeout, &error_message);

    if (error_message != NULL) {
        goto failure;
    }

    self->idle_con = mpd_connect((mc_Client *) self, host, port, timeout, &error_message);

    if (error_message != NULL) {
        if (self->cmnd_con) {
            mpd_connection_free(self->cmnd_con);
            self->cmnd_con = NULL;
        }

        goto failure;
    }

    /* listener */
    cmnder_set_run_listener(self, TRUE);
    cmnder_create_glib_adapter(self, context);

    /* start ping thread */
    if (self->pinger_thread == NULL) {
        cmnder_set_run_pinger(self, TRUE);
        self->pinger_thread = g_thread_new(
                "cmnd-core-pinger",
                (GThreadFunc) cmnder_ping_server,
                self
        );
    }

failure:
    return error_message;
}

//////////////////////

static bool cmnder_do_is_connected(mc_Client *parent)
{
    mc_CmndClient *self = child(parent);
    return (self->idle_con && self->cmnd_con);
}

///////////////////////

static bool cmnder_do_disconnect(mc_Client *parent)
{
    if (cmnder_do_is_connected(parent)) {
        mc_CmndClient *self = child(parent);
        cmnder_reset(self);
        return true;
    } else {
        return false;
    }
}

///////////////////////

static mpd_connection *cmnder_do_get(mc_Client *self)
{
    return child(self)->cmnd_con;
}

///////////////////////

static void cmnder_do_put(mc_Client *self)
{
    /* NOOP */
    (void) self;
}

//////////////////////

static void cmnder_do_free(mc_Client *parent)
{
    g_assert(parent);
    mc_CmndClient *self = child(parent);
    cmnder_do_disconnect(parent);
    g_mutex_clear(&self->flagmtx_run_pinger);
    g_mutex_clear(&self->flagmtx_run_listener);
    memset(self, 0, sizeof(mc_CmndClient));
    g_free(self);
}

//////////////////////
// Public Interface //
//////////////////////

mc_Client *mc_create_cmnder(long connection_timeout_ms)
{
    /* Only fill callbacks here, no
     * actual data relied to mc_CmndClient
     * should be placed here!
     */
    mc_CmndClient *self = g_new0(mc_CmndClient, 1);
    self->logic.do_disconnect = cmnder_do_disconnect;
    self->logic.do_get = cmnder_do_get;
    self->logic.do_put = cmnder_do_put;
    self->logic.do_free = cmnder_do_free;
    self->logic.do_connect = cmnder_do_connect;
    self->logic.do_is_connected = cmnder_do_is_connected;

    g_mutex_init(&self->flagmtx_run_pinger);
    g_mutex_init(&self->flagmtx_run_listener);

    /* Fallback to default when neg. number given */
    if (connection_timeout_ms < 0)
        self->connection_timeout_ms = 5 * 1000;
    else
        self->connection_timeout_ms = connection_timeout_ms;

    return (mc_Client *) self;
}
