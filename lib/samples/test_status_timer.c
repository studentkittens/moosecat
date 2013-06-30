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
    g_print("status = %d\n", mc_status_get_song_id(conn));
    g_print("statis = %d\n", mc_stats_get_number_of_artists(conn));
    g_print("ctsong = %s\n", mc_current_song_get_tag(conn, MPD_TAG_ARTIST, 0));

    if (counter++ == 10)
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
