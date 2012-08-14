#include "idle_core.h"
#include "common.h"
#include "../../util/gasyncqueue-watch.h"

#include <glib.h>
#include <string.h>

/* define to cast a parent connector to the
 * concrete idle connector
 */
#define child(obj) ((Proto_CmndConnector *)obj)

char * fruits[] =
{
    "apple",
    "banana",
    "clementine",
    "dick",
    "estragon",
    "fruit",
    "ghana",
    "hindukush",
    "italie",
    "jemen",
    "kazakstan",
    "" /* Sign for termination */
};

///////////////////////
// Private Interface //
///////////////////////

typedef struct
{
    Proto_Connector logic;
    mpd_connection * cmnd_con;
    mpd_connection * idle_con;
    GThread * listener_thread;
    GAsyncQueue * event_queue;
    gboolean watch_created;
} Proto_CmndConnector;

///////////////////

static gpointer listener_thread (gpointer data)
{
    Proto_CmndConnector * self = child(data);
    enum mpd_idle events = 0;

    while(TRUE)
    {
        if(mpd_send_idle(self->idle_con) == FALSE)
        {
            g_print("Warning: Sending idle command failed.\n");
            break;
        }

        events = mpd_recv_idle(self->idle_con, TRUE);
        if(events == 0)
        {
            g_print("Warning: Received no events.\n");
            break;
        }
        
        g_async_queue_push(self->event_queue, GINT_TO_POINTER(events));
    }


    return NULL;
}

///////////////////

static const char * cmnder_do_connect (Proto_Connector * parent, const char * host, int port, int timeout)
{
    Proto_CmndConnector * self = child (parent);

    self->cmnd_con = mpd_connect (host, port, timeout);
    self->idle_con = mpd_connect (host, port, timeout);

    self->watch_created = FALSE;
    self->event_queue = g_async_queue_new();
    return NULL;
}

///////////////////////

static mpd_connection * cmnder_do_get (Proto_Connector * self)
{
    return child (self)->cmnd_con;
}

///////////////////////

static void cmnder_do_put (Proto_Connector * self)
{
    (void) self;
    /* NOOP */
}

///////////////////////

static void cmnder_free (Proto_CmndConnector * self)
{
    if (self != NULL)
    {
        if (self->watch_created)
            g_thread_join (self->listener_thread);

        if (self->event_queue)
            g_async_queue_unref (self->event_queue);

        if (self->cmnd_con)
            mpd_connection_free (self->cmnd_con);

        if (self->idle_con)
            mpd_connection_free (self->idle_con);

        memset (self, 0, sizeof (Proto_CmndConnector) );
        g_free (self);
    }
}

///////////////////////

static bool cmnder_do_disconnect (Proto_Connector * self)
{
    cmnder_free (child (self) );
    return true;
}

//////////////////////

static gboolean cmnder_event_callback (GAsyncQueue * queue, gpointer user_data)
{
    gpointer item = NULL;
    g_print ("<<-- -->>\n");

    while ( (item = g_async_queue_try_pop (queue) ) )
    {
        enum mpd_idle events = GPOINTER_TO_INT(item);
        g_print ("<=== %d\n", events);
    }
    g_print ("-->> <<--\n");

    return TRUE;
}

//////////////////////

static unsigned cmnder_do_create_glib_adapter (Proto_Connector * parent, GMainContext * context)
{
    Proto_CmndConnector * self = child (parent);
    if (self->watch_created == FALSE)
    {
        /* Start the listener thread and set the Queue Watcher on it */
        self->listener_thread = g_thread_new ("listener", listener_thread, self);
        return mc_async_queue_watch_new (child (self)->event_queue, G_PRIORITY_DEFAULT,
                                         cmnder_event_callback, self, context);
    }
    else
    {
        return -1;
    }
}

//////////////////////
// Public Interface //
//////////////////////

Proto_Connector * proto_create_cmnder (void)
{
    Proto_CmndConnector * self = g_new0 (Proto_CmndConnector, 1);
    self->logic.do_disconnect = cmnder_do_disconnect;
    self->logic.do_get = cmnder_do_get;
    self->logic.do_put = cmnder_do_put;
    self->logic.do_connect = cmnder_do_connect;
    self->logic.do_create_glib_adapter = cmnder_do_create_glib_adapter;

    return (Proto_Connector *) self;
}
