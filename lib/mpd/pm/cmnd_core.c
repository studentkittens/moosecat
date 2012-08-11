#include "idle_core.h"

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
    mpd_connection * cmnd_con;
    mpd_connection * idle_con;
} Proto_CmndConnector;

///////////////////

static const char * cmnder_do_connect (Proto_Connector * self, const char * host, int port, int timeout)
{
    // Create one mpd connection, and
    // everything that needs to be initialized once here
    g_print ("%p %s %d %d\n", self, host, port, timeout);
    return NULL;
}

///////////////////////

static mpd_connection * cmnder_do_get (Proto_Connector * self)
{
    // leave event listening, make connection sendable
    return child (self)->cmnd_con;
}

///////////////////////

static void cmnder_do_put (Proto_Connector * self)
{
    // enter event listening state
    g_print ("Putting %p back\n", child (self) );
}

///////////////////////

static void cmnder_free (Proto_CmndConnector * ctor)
{
    if (ctor != NULL)
    {
        memset (ctor, 0, sizeof (Proto_CmndConnector) );
        g_free (ctor);
    }
}

///////////////////////

static bool cmnder_do_disconnect (Proto_Connector * self)
{
    // cleanup
    cmnder_free (child (self) );
    return true;
}
//////////////////////
// Public Interface //
//////////////////////

Proto_Connector * proto_create_cmnder (void)
{
    Proto_CmndConnector * ctor = g_new0 (Proto_CmndConnector, 1);
    ctor->logic.do_disconnect = cmnder_do_disconnect;
    ctor->logic.do_get = cmnder_do_get;
    ctor->logic.do_put = cmnder_do_put;
    ctor->logic.do_connect = cmnder_do_connect;

    // ... connect code ....
    return (Proto_Connector *) ctor;
}
