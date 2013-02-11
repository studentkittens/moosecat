#include "api.h"
#include <stdlib.h>

static mc_Client * conn = NULL;

static void signal_event (
        mc_Client * u_conn,
        enum mpd_idle event,
        void * user_data)
{
    (void) u_conn;
    (void) user_data;

    g_print ("%p %p %p\n", conn->status, conn->song, conn->stats);

    g_print ("event = %d\n", event);
    g_print ("status = %d\n", mpd_status_get_song_id (conn->status) );
    g_print ("statis = %d\n", mpd_stats_get_number_of_artists (conn->stats) );

    if (conn->song)
        g_print ("ctsong = %s\n", mpd_song_get_tag (conn->song, MPD_TAG_ARTIST, 0) );
}

/////////////////////////////

static void signal_error (
        mc_Client * u_conn,
        enum mpd_error error,
        const char * err_msg,
        bool is_fatal,
        void * user_data)
{
    (void) u_conn;
    (void) is_fatal;
    (void) user_data;
    g_print ("ERROR [Fatal: %s]: #%d: %s\n", is_fatal ? "Yes" : "No", error, err_msg);
}

/////////////////////////////

static void signal_progress (
        mc_Client * client,
        bool print_newline,
        const char * progress,
        void * user_data
        )
{
    (void) user_data;
    (void) client;
    g_print ("PROGRESS: %s%c", progress, print_newline ? '\n' : '\r');
}

/////////////////////////////

static void signal_connectivity (
        mc_Client * client,
        bool server_changed,
        void * user_data
        )
{
    (void) user_data;

    g_print ("CONNECTION: is %s and server %s.\n",
            mc_proto_is_connected (client) ? "connected" : "disconnected",
            server_changed ? "changed" : "is still the same");
}

/////////////////////////////

static gboolean register_all_the_things(gpointer user_data)
{
    //GMainLoop * loop = (GMainLoop *) user_data;

    conn = mc_proto_create (MC_PM_IDLE);
    mc_proto_signal_add (conn, "client-event", signal_event, NULL);
    mc_proto_signal_add (conn, "error", signal_error, NULL);
    mc_proto_signal_add (conn, "progress", signal_progress, NULL);
    mc_proto_signal_add (conn, "connectivity", signal_connectivity, NULL);

    mc_misc_register_posix_signal (conn);

    char * err = mc_proto_connect (conn, NULL, "localhost", 6600, 2);
    if ( err != NULL ) {
        g_print ("Error: %s\n", err);
        g_free (err);
        return EXIT_FAILURE;
    }

    g_print("Main loop running\n");
    return FALSE;
}

/////////////////////////////

static gboolean unregister_all_the_things(gpointer user_data)
{
    GMainLoop * loop = (GMainLoop *) user_data;
    g_print("Making the disconnect thing.\n");
    g_main_loop_quit(loop);
    mc_proto_disconnect (conn);

    return FALSE;
}

/////////////////////////////

int main (void)
{
    GMainLoop * loop = g_main_loop_new (NULL, FALSE);

    /* Init with already running mainloop */
    g_timeout_add (0, register_all_the_things, loop);

    /* disconnect after 5 seconds */
    g_timeout_add (5 * 1000, unregister_all_the_things, loop);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
    return EXIT_SUCCESS;
}
