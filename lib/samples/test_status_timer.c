#include "../api.h"

#include <stdlib.h>
#include <stdio.h>


static void signal_event(
    mc_Client *u_conn,
    enum mpd_idle event,
    void *user_data)
{
    (void) u_conn;
    static int counter = 0;
    GMainLoop *loop = user_data;


    g_print("event = %d\n", event);

    if (++counter == 15)
        g_main_loop_quit(loop);
}

/////////////////////////////

int main(void)
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    mc_Client * client = mc_create(MC_PM_COMMAND);
    mc_signal_add(client, "client-event", signal_event, loop);
    mc_status_timer_register(client, 50, true);
    mc_misc_register_posix_signal(client);
    mc_connect(client, NULL, "localhost", 6600, 2.0);

    /* BLOCKS */
    {
        g_main_loop_run(loop);
    }

    mc_free(client);
    return EXIT_SUCCESS;
}
