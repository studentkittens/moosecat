#include "../api.h"

#include <stdlib.h>
#include <stdio.h>


static void signal_event(
    MooseClient *u_conn,
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

    MooseClient * client = moose_create(MC_PM_COMMAND);
    moose_signal_add(client, "client-event", signal_event, loop);
    moose_status_timer_register(client, 50, true);
    moose_misc_register_posix_signal(client);
    moose_connect(client, NULL, "localhost", 6600, 2.0);

    /* BLOCKS */
    {
        g_main_loop_run(loop);
    }
    g_main_loop_unref(loop);

    moose_free(client);
    return EXIT_SUCCESS;
}
