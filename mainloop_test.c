#include "lib/mpd/client.h"

int vol = 0;
GTimer * timer;

gboolean volume_change (mc_Client * client)
{
    g_print ("%2.3f\n", g_timer_elapsed (timer, NULL) );
    //mc_client_next (client);
    return true;
}

int main (void)
{
    timer = g_timer_new();
    mc_Client * client = mc_proto_create (MC_PM_COMMAND);
    mc_proto_connect (client, NULL, "localhost", 6600, 10.0);
    GMainLoop * loop = g_main_loop_new (NULL, true);
    g_timeout_add (100, (GSourceFunc) volume_change, client);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
    mc_proto_free (client);
    return 0;
}

/*
 * total    after
 * 150      60
 * 61       60
 */
