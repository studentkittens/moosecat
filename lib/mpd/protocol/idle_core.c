#include "idle_core.h"

#include <glib.h>
#include <string.h>

/* define to cast a parent connector to the
 * concrete idle connector
 */
#define child(obj) ((Proto_IdleConnector *)obj)

///////////////////////
// Private Interface //
///////////////////////

typedef struct
{
    Proto_Connector logic;

    mpd_connection * con;
} Proto_IdleConnector;

///////////////////

static const char * idler_do_connect (Proto_Connector * self, const char * host, int port, int timeout)
{
    // Create one mpd connection, and
    // everything that needs to be initialized once here
    g_print ("%p %s %d %d\n", self, host, port, timeout);
    return NULL;
}

///////////////////////

static mpd_connection * idler_do_get (Proto_Connector * self)
{
    // leave event listening, make connection sendable
    return child (self)->con;
}

///////////////////////

static void idler_do_put (Proto_Connector * self)
{
    // enter event listening state
    g_print ("Putting %p back\n", child (self) );
}

///////////////////////

static void idler_free (Proto_IdleConnector * ctor)
{
    if (ctor != NULL)
    {
        memset (ctor, 0, sizeof (Proto_IdleConnector) );
        g_free (ctor);
    }
}

///////////////////////

static bool idler_do_disconnect (Proto_Connector * self)
{
    // cleanup
    idler_free (child (self) );
    return true;
}
//////////////////////
// Public Interface //
//////////////////////

Proto_Connector * proto_create_idler (void)
{
    Proto_IdleConnector * ctor = g_new0 (Proto_IdleConnector, 1);
    ctor->logic.do_disconnect = idler_do_disconnect;
    ctor->logic.do_get = idler_do_get;
    ctor->logic.do_put = idler_do_put;
    ctor->logic.do_connect = idler_do_connect;
    ctor->con = NULL; // <-- write connect code here
    return (Proto_Connector *) ctor;
}
