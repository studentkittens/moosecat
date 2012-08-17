#include "mpd/protocol.h"
#include "mpd/pm/cmnd_core.h"
#include "mpd/client.h"

#include <stdlib.h>
#include <stdio.h>

static Proto_Connector * conn = NULL;

static void event (enum mpd_idle event, void * user_data)
{
    (void) user_data;
    g_print ("event = %d\n", event);
    g_print ("status = %d\n", mpd_status_get_song_id(conn->status));
}

/////////////////////////////

gboolean next_song (gpointer user_data)
{
    GMainLoop * loop = (GMainLoop *) user_data;

    conn = proto_create_cmnder();
    proto_add_event_callback (conn, event, NULL);

    for (int i = 0; i < 100; i++)
    {
        char * err = proto_connect (conn, NULL, "localhost", 6600, 2);
        if ( err != NULL )
        {
            g_print ("Err: %s\n", err);
            g_free (err);
            break;
        }

        mc_volume (conn, 100);
        while (g_main_context_iteration (NULL, TRUE) );
        proto_disconnect (conn);
    }

    proto_free (conn);

    g_main_loop_quit (loop);
    return FALSE;
}

/////////////////////////////

int main (void)
{
    GMainLoop * loop = g_main_loop_new (NULL, FALSE);
    g_timeout_add (500, next_song, loop);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);

    return EXIT_SUCCESS;
}
