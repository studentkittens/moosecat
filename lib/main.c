#include "mpd/protocol.h"
#include "mpd/pm/cmnd_core.h"
#include "mpd/client.h"

#include <stdlib.h>
#include <stdio.h>

Proto_Connector * conn = NULL;

static void event (enum mpd_idle event, void * user_data)
{
    (void) user_data;
    g_print ("event = %d\n", event);
}

/////////////////////////////

gboolean next_song (gpointer user_data)
{
    GMainLoop * loop = (GMainLoop *) user_data;
    g_print ("NEXT!\n");

    proto_update(conn, INT_MAX);

    for(int i = 0; i < 1; i++)
    {
        next (conn);
    }

    while (g_main_context_iteration (NULL, TRUE) );
    g_usleep (1 * 1000 * 100);

    /* Disconnect all */
    g_print("Disconnecting\n");
    proto_disconnect (conn);
    g_print("Disconnecting done\n");
    proto_connect (conn, NULL, "localhost", 6600, 2);

    next (conn);
    while (g_main_context_iteration (NULL, TRUE) );
    proto_disconnect (conn);

    g_main_loop_quit (loop);

    return FALSE;
}

/////////////////////////////

int main (void)
{
    //g_mem_set_vtable (glib_mem_profiler_table);

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

        g_timeout_add (500, next_song, loop);

        g_main_loop_run (loop);
        g_main_loop_unref (loop);
    }

    g_print("\n--------------------------------\n");
    //g_mem_profile ();


    return EXIT_SUCCESS;
}
