#include "../moose-api.h"
#include "../misc/moose-misc-job-manager.h"
#include <glib.h>


static GThread *MAIN_THREAD;


static void event_cb(
    G_GNUC_UNUSED MooseClient * client,
    MooseIdle events,
    G_GNUC_UNUSED gpointer user_data) {

    /* Signals shall always be emitted on the mainthread */
    g_assert(g_thread_self() == MAIN_THREAD);

    moose_client_run_single(client, "['volume', 50]");

    moose_message("Client Event = %u", events);
    MooseStatus * status = moose_client_ref_status(client); {
        moose_message("%d %d %d",
                moose_status_get_state(status),
                moose_status_get_song_id(status),
                moose_status_get_song_pos(status)
        );
    } 
    moose_status_unref(status);
}

static gboolean timeout_cb(gpointer user_data) {
    GMainLoop * loop = user_data;
    moose_message("LOOP STOP");
    g_main_loop_quit(loop);
    return FALSE;
}

int main(int argc, char const * argv[]) {
    moose_debug_install_handler();

    MAIN_THREAD = g_thread_self();

    MooseClient * client = NULL;
    if (argc > 1 && g_strcmp0(argv[1], "-c") == 0) {
        moose_message("Using CMND ProtocolMachine.");
        client = moose_client_new(MOOSE_PROTOCOL_IDLE);
    } else {
        moose_message("Using IDLE ProtocolMachine.");
        client = moose_client_new(MOOSE_PROTOCOL_IDLE);
    }

    g_signal_connect(client, "client-event", G_CALLBACK(event_cb), NULL);
    if(moose_client_connect(client, "localhost", 6666, 20)) {
        GMainLoop * loop = g_main_loop_new(NULL, TRUE);
        g_timeout_add(10000, timeout_cb, loop);
        moose_client_run_single(client, "play");
        g_main_loop_run(loop);
        moose_message("MAIN LOOP RUN OVER");

        moose_client_disconnect(client);
        moose_client_unref(client);
        g_main_loop_unref(loop);
    } else {
        moose_warning("Could not connect. Test-Server running?");
    }
    return 0;
}
