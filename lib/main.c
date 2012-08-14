#include "mpd/protocol.h"
#include "mpd/pm/cmnd_core.h"
#include "mpd/client.h"

#include <stdlib.h>
#include <stdio.h>

static void event (int event, bool connection_change)
{
    g_print ("%d %d\n", event, connection_change);
}

/////////////////////////////

int main (void)
{
    Proto_Connector * conn = proto_create_cmnder();
    const char * err = proto_connect (conn, "localhost", 6600, 2);
    if (err != NULL)
    {
        printf ("Cannot connect.\n");
    }
    else
    {
        GMainLoop * loop = g_main_loop_new (NULL, FALSE);
        proto_create_glib_adapter (conn, NULL);
        proto_add_event_callback (conn, event);

        //next (conn);

        g_main_loop_run (loop);
        g_main_loop_unref (loop);

        /* Disconnect all */
        proto_disconnect (conn);
        conn = NULL;
    }
    return EXIT_SUCCESS;
}
