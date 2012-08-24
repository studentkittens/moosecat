#include "mpd/protocol.h"
#include "mpd/pm/cmnd_core.h"
#include "mpd/client.h"

#include <stdlib.h>
#include <stdio.h>

static mc_Client * conn = NULL;

static void event (enum mpd_idle event, void * user_data)
{
    (void) user_data;
    g_print ("%p %p %p\n", conn->status, conn->song, conn->stats);
    g_print ("event = %d\n", event);
    g_print ("status = %d\n", mpd_status_get_song_id(conn->status));
    g_print ("ctsong = %s\n", mpd_song_get_tag(conn->song, MPD_TAG_ARTIST, 0));
    g_print ("statis = %d\n", mpd_stats_get_number_of_artists(conn->stats));
}

/////////////////////////////

gboolean next_song (gpointer user_data)
{
    GMainLoop * loop = (GMainLoop *) user_data;

    conn = mc_proto_create_cmnder();
    mc_proto_add_event_callback (conn, event, NULL);

    for (int i = 0; i < 100; i++)
    {
        g_print ("Connecting... ");
        char * err = mc_proto_connect (conn, NULL, "localhost", 6600, 2);
        if ( err != NULL )
        {
            g_print ("Err: %s\n", err);
            g_free (err);
            break;
        }
        g_print (" done.\n");


        for (int i = 0; i < 10; i++)
            mc_client_volume (conn, 100);
        
        gint wait = g_random_int_range(0, 1000 * 1000);
        g_print("Waiting for %f\n", wait / (1000. * 1000.));
        g_usleep(wait);

        while (g_main_context_iteration (NULL, TRUE) );

        g_print ("Disconnecting... ");
        mc_proto_disconnect (conn);
        g_print (" done.\n");
    }

    mc_proto_free (conn);

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
