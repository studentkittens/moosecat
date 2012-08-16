#include "idle_core.h"
#include "common.h"
#include "../../util/gasyncqueue-watch.h"
#include "../protocol.h"

#include <glib.h>
#include <string.h>

/* define to cast a parent connector to the
 * concrete idle connector
 */
#define child(obj) ((Proto_CmndConnector *)obj)

///////////////////////
// Private Interface //
///////////////////////

typedef struct
{
    Proto_Connector logic;

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
} Proto_CmndConnector;

//////////////////////

static gboolean cmnder_event_callback (GAsyncQueue * queue, gpointer user_data)
{
    Proto_Connector * self = user_data;
    gpointer item = NULL;

    /* Pop all items from the queue that are in,
     * enqueueue items into a list and remove dupes
     */
    GList * event_list = NULL;
    enum mpd_idle prev_events = 0;
    while ( (item = g_async_queue_try_pop (queue) ) )
    {
        enum mpd_idle events = GPOINTER_TO_INT (item);
        if (events != prev_events)
        {
            event_list = g_list_prepend (event_list, GINT_TO_POINTER (events) );
            prev_events = events;
        }
    }

    /* 
     * Now iterate over the happened events,
     * and execute any registered event callbacks
     */
    for (GList * iter = event_list; iter; iter = iter->next)
    {
        g_print ("<=== %d\n", GPOINTER_TO_INT (iter->data) );
        proto_update(self, GPOINTER_TO_INT (iter->data));
    }

    g_list_free (event_list);
    return TRUE;
}

///////////////////

static gpointer cmnder_listener_thread (gpointer data)
{
    Proto_CmndConnector * self = child (data);
    enum mpd_idle events = 0;

    while (self->run_listener)
    {
        if( (events = mpd_run_idle(self->idle_con)) == 0)
        {
            g_print("Info: No events received at all.");
            break;
        }
        
        g_async_queue_push (self->event_queue, GINT_TO_POINTER (events) );
    }

    g_print ("Listener thread exited.\n");
    return NULL;
}

//////////////////////

static void cmnder_create_glib_adapter (Proto_CmndConnector * self, GMainContext * context)
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

static void cmnder_shutdown_listener(Proto_CmndConnector * self)
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
    mpd_run_crossfade(self->cmnd_con, 0);
        
    if (self->watch_source_id != -1) 
    {
       g_source_remove(self->watch_source_id);
       g_thread_join (self->listener_thread);
    }

    self->watch_source_id = -1;
}

///////////////////////

static void cmnder_free (Proto_CmndConnector * self)
{
    if (self != NULL)
    {
        if (self->idle_con) 
        {
            mpd_connection_free (self->idle_con);
            self->idle_con = NULL;
        }

        cmnder_shutdown_listener (self);

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

static const char * cmnder_do_connect (Proto_Connector * parent, GMainContext * context, const char * host, int port, int timeout)
{
    Proto_CmndConnector * self = child (parent);

    self->cmnd_con = mpd_connect (host, port, timeout);
    self->idle_con = mpd_connect (host, port, timeout);

    self->run_listener = TRUE;
    self->watch_source_id = -1;
    self->event_queue = g_async_queue_new();

    cmnder_create_glib_adapter(self, context);
    return NULL;
}

///////////////////////

static bool cmnder_do_disconnect (Proto_Connector * self)
{
    cmnder_free (child (self) );
    return true;
}

//////////////////////

static bool cmnder_do_is_connected (Proto_Connector * parent)
{
    Proto_CmndConnector * self = child (parent);
    return (self->idle_con && self->cmnd_con);
}

///////////////////////

static mpd_connection * cmnder_do_get (Proto_Connector * self)
{
    return child (self)->cmnd_con;
}

//////////////////////

static void cmnder_do_free (Proto_Connector * parent)
{
    Proto_CmndConnector * self = child(parent);
    memset(self, 0, sizeof(Proto_CmndConnector));
    g_free(self);
}

//////////////////////
// Public Interface //
//////////////////////

Proto_Connector * proto_create_cmnder (void)
{
    /* Only fill callbacks here, no 
     * actual data relied to Proto_CmndConnector!
     */
    Proto_CmndConnector * self = g_new0 (Proto_CmndConnector, 1);
    self->logic.do_disconnect = cmnder_do_disconnect;
    self->logic.do_get = cmnder_do_get;
    self->logic.do_put = NULL;
    self->logic.do_free = cmnder_do_free;
    self->logic.do_connect = cmnder_do_connect;
    self->logic.do_is_connected = cmnder_do_is_connected;

    return (Proto_Connector *) self;
}
