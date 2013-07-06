#include "../api.h"

#include <stdlib.h>
#include <stdio.h>

static int counter = 0;
static mc_Client *conn = NULL;

static void signal_event(
    mc_Client *u_conn,
    enum mpd_idle event,
    void *user_data)
{
    (void) u_conn;
    GMainLoop *loop = user_data;


    g_print("event = %d\n", event);

    if (counter++ == 15)
        g_main_loop_quit(loop);
}

/////////////////////////////

int main(void)
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    conn = mc_create(MC_PM_COMMAND);
    mc_signal_add(conn, "client-event", signal_event, loop);
    mc_status_timer_register(conn, 50, true);
    mc_misc_register_posix_signal(conn);
    mc_connect(conn, NULL, "localhost", 6600, 2.0);

    g_main_loop_run(loop);


    mc_free(conn);
    g_main_loop_quit(loop);

    return EXIT_SUCCESS;
}
