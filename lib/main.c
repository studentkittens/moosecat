#include "mpd/protocol.h"
#include "mpd/pm/cmnd_core.h"
#include "mpd/client.h"

#include <stdlib.h>
#include <stdio.h>

Proto_Connector * conn = NULL;

static void event (enum mpd_idle event, bool connection_change)
{
    g_print ("%d %d\n", event, connection_change);
}

/////////////////////////////

gboolean next_song (gpointer user_data)
{
    GMainLoop * loop = (GMainLoop *) user_data;
    g_print ("NEXT!\n");
    next (conn);

    while (g_main_context_iteration (NULL, TRUE) );
    g_usleep (1 * 1000 * 100);

    /* Disconnect all */
    g_print("Disconnecting\n");
    proto_disconnect (conn);
    conn = NULL;
    g_print("Disconnecting done\n");

    g_main_loop_quit (loop);

    return FALSE;
}

/////////////////////////////

int main (void)
{
    conn = proto_create_cmnder();
    const char * err = proto_connect (conn, NULL, "localhost", 6600, 2);
    if (err != NULL)
    {
        printf ("Cannot connect.\n");
    }
    else
    {
        GMainLoop * loop = g_main_loop_new (NULL, FALSE);
        proto_add_event_callback (conn, event, NULL);

        //next (conn);
        g_timeout_add (500, next_song, loop);

        g_main_loop_run (loop);
        g_main_loop_unref (loop);

    }
    return EXIT_SUCCESS;
}
