#include "cmnd-core.h"
#include "common.h"
#include "../../util/gasyncqueue-watch.h"
#include "../../util/sleep-grained.h"
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


/* WARNING AND TODO:
 * 
 * Currently this code does not work for obscure reasons.
 * Perhaps I changed something and braimdumped during that.
 *
 * After a while the connections timeouts and both close.
 *
 * This needs to be debugged.
 *
 */


typedef struct {
    /* Parent struct */
    mc_Client logic;

    /* Connection to send commands */
    mpd_connection *cmnd_con;

    /* Protexct get/set of self->cmnd_con */
    GMutex cmnd_con_mtx;

    /* Thread that polls idle_con */
    GThread *listener_thread;

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

static bool cmnder_get_run_listener(mc_CmndClient * self, GThread * thread)
{
    volatile bool result = false;
    g_mutex_lock(&self->flagmtx_run_listener);
    result = GPOINTER_TO_INT(g_hash_table_lookup(self->run_listener_table, thread));
    g_mutex_unlock(&self->flagmtx_run_listener);

    return result;
}

static void cmnder_set_run_listener(mc_CmndClient * self, GThread *thread, volatile bool state)
{
    g_mutex_lock(&self->flagmtx_run_listener);
    g_hash_table_insert(self->run_listener_table, thread, GINT_TO_POINTER(state));
    g_mutex_unlock(&self->flagmtx_run_listener);
}

///////////////////

static gpointer cmnder_listener_thread(gpointer data)
{
    mc_CmndClient *self = child(data);
    enum mpd_idle events = 0;

    char *error_message = NULL;
    struct mpd_connection * idle_con = mpd_connect((mc_Client *) self,
            mc_get_host((mc_Client*)self),
            mc_get_port((mc_Client*)self),
            mc_get_timeout((mc_Client*)self),
            &error_message
    );
    
    cmnder_set_run_listener(self, g_thread_self(), TRUE);

    if(idle_con && error_message == NULL) {
        while(cmnder_get_run_listener(self, g_thread_self())) {
            if(mpd_send_idle(idle_con) == false) {
                if(mpd_connection_get_error(idle_con) == MPD_ERROR_TIMEOUT) {
                    mpd_connection_clear_error(idle_con);
                } else {
                }

                if ((events = mpd_recv_idle(idle_con, false)) == 0) {
                    mc_shelper_report_error((mc_Client *) self, idle_con);
                    break;
                }

                if(cmnder_get_run_listener(self, g_thread_self())) {
                     mc_force_sync((mc_Client *) self, events);
                }
            }
        }

        mpd_connection_free(idle_con);
        idle_con = NULL;

    } else {
        mc_shelper_report_error_printf((mc_Client *)self,
                "listener_thread: cannot connect: %s",
                error_message
        );
    }

    g_thread_unref(g_thread_self());
    g_thread_exit(NULL);
    return NULL;
}

//////////////////////

static void cmnder_create_glib_adapter(
    mc_CmndClient *self,
    G_GNUC_UNUSED GMainContext *context)
{
    if (self->listener_thread == NULL) {
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
    cmnder_set_run_listener(self, self->listener_thread, FALSE);

    /* Note: Idle thread is not joined.
     *       It frees itself when it exits,
     *       and we told it to exit by calling
     *       cmnder_set_run_listener() avbove
     */
    self->listener_thread = NULL;
}

///////////////////////

static void cmnder_reset(mc_CmndClient *self)
{
    if (self != NULL) {
        cmnder_shutdown_listener(self);

        g_mutex_lock(&self->cmnd_con_mtx);
        if (self->cmnd_con) {
            mpd_connection_free(self->cmnd_con);
            self->cmnd_con = NULL;
        }
        g_mutex_unlock(&self->cmnd_con_mtx);
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
    g_mutex_lock(&self->cmnd_con_mtx);
    self->cmnd_con = mpd_connect((mc_Client *) self, host, port, timeout, &error_message);
    g_mutex_unlock(&self->cmnd_con_mtx);

    if (error_message != NULL) {
        goto failure;
    }

    if (error_message != NULL) {
        g_mutex_lock(&self->cmnd_con_mtx);
        if (self->cmnd_con) {
            mpd_connection_free(self->cmnd_con);
            self->cmnd_con = NULL;
        }
        g_mutex_unlock(&self->cmnd_con_mtx);

        goto failure;
    }

    /* listener */
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

    g_mutex_lock(&self->cmnd_con_mtx);
    struct mpd_connection * conn = child(self)->cmnd_con;
    g_mutex_unlock(&self->cmnd_con_mtx);

    return (conn);
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

static mpd_connection *cmnder_do_get(mc_Client *client)
{
    return child(client)->cmnd_con;
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

    /* Close the ping thread */
    cmnder_shutdown_pinger(self);

    g_hash_table_destroy(self->run_listener_table);
    g_mutex_clear(&self->cmnd_con_mtx);
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

    self->run_listener_table = g_hash_table_new(
            g_direct_hash, g_int_equal
    );

    g_mutex_init(&self->cmnd_con_mtx);
    g_mutex_init(&self->flagmtx_run_pinger);
    g_mutex_init(&self->flagmtx_run_listener);

    /* Fallback to default when neg. number given */
    if (connection_timeout_ms < 0)
        self->connection_timeout_ms = 5 * 1000;
    else
        self->connection_timeout_ms = connection_timeout_ms;

    return (mc_Client *) self;
}
