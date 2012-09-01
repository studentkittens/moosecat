#include "idle_core.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>

/* define to cast a parent connector to the
 * concrete idle connector
 */
#define child(obj) ((mc_IdleClient *)obj)

///////////////////////
// Private Interface //
///////////////////////

typedef struct
{
    mc_Client logic;

    mpd_connection * con;
} mc_IdleClient;

///////////////////

static char * idler_do_connect (mc_Client * self, GMainContext * context, const char * host, int port, float timeout)
{
    (void) context;
    // Create one mpd connection, and
    // everything that needs to be initialized once here
    g_print ("%p %s %d %f\n", self, host, port, timeout);
    return NULL;
}

///////////////////////

static mpd_connection * idler_do_get (mc_Client * self)
{
    // leave event listening, make connection sendable
    return child (self)->con;
}

///////////////////////

static void idler_do_put (mc_Client * self)
{
    // enter event listening state
    g_print ("Putting %p back\n", child (self) );
}

///////////////////////

static void idler_free (mc_IdleClient * ctor)
{
    if (ctor != NULL)
    {
        memset (ctor, 0, sizeof (mc_IdleClient) );
        g_free (ctor);
    }
}

///////////////////////

static bool idler_do_disconnect (mc_Client * self)
{
    // cleanup
    idler_free (child (self) );
    return true;
}

//////////////////////
// Public Interface //
//////////////////////

mc_Client * mc_proto_create_idler (void)
{
    mc_IdleClient * ctor = g_new0 (mc_IdleClient, 1);

    /* Set all required functions */
    ctor->logic.do_disconnect = idler_do_disconnect;
    ctor->logic.do_get = idler_do_get;
    ctor->logic.do_put = idler_do_put;
    ctor->logic.do_connect = idler_do_connect;

    ctor->con = NULL; // <-- write connect code here

    // ... for now just abort here.
    g_print("NOTE: MC_PM_IDLE pm is not yet implemented!\n");
    g_print("      Kittens didn't walk over the keybaord yet.\n");
    g_print("      => Aborting!\n\n");
    raise(SIGTERM);

    return (mc_Client *) ctor;
}
