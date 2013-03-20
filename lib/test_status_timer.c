#include "api.h"

#include <stdlib.h>
#include <stdio.h>

static mc_Client *conn = NULL;

static void signal_event(
    mc_Client *u_conn,
    enum mpd_idle event,
    void *user_data)
{
    (void) u_conn;
    (void) user_data;
    g_print("%p %p %p\n", conn->status, conn->song, conn->stats);
    g_print("event = %d\n", event);
    g_print("status = %d\n", mpd_status_get_song_id(conn->status));
    g_print("statis = %d\n", mpd_stats_get_number_of_artists(conn->stats));

    if (conn->song)
        g_print("ctsong = %s\n", mpd_song_get_tag(conn->song, MPD_TAG_ARTIST, 0));
}

/////////////////////////////

int main(void)
{

    conn = mc_proto_create(MC_PM_COMMAND);
    mc_proto_signal_add(conn, "client-event", signal_event, NULL);
    mc_proto_status_timer_register(conn, 5000, true);
    mc_misc_register_posix_signal(conn);
    mc_proto_connect(conn, NULL, "localhost", 6600, 2.0);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);


    mc_proto_free(conn);
    g_main_loop_quit(loop);

    return EXIT_SUCCESS;
}
