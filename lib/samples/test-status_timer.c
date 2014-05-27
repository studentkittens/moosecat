#include "../moose-api.h"

static void signal_event(
    MooseClient * client,
    MooseIdle event,
    void * user_data) {
    g_printerr("event = %d\n", event);
    MooseStatus * status = moose_client_ref_status(client);
    g_printerr("State: %d\n", moose_status_get_state(status));
    moose_status_unref(status);
    g_printerr("Unlocked.\n");

    static int counter = 0;
    if (++counter == 15) {
        g_main_loop_quit((GMainLoop *)user_data);
    }
}

int main(void) {
    GMainLoop * loop = g_main_loop_new(NULL, FALSE);

    MooseClient * client = moose_client_new(MOOSE_PROTOCOL_IDLE);
    g_signal_connect(client, "client-event", G_CALLBACK(signal_event), loop);
    moose_client_timer_set_active(client, TRUE);
    moose_client_connect(client, NULL, "localhost", 6601, 2.0);

    g_printerr("Is %d\n", moose_client_is_connected(client));

    /* BLOCKS */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    moose_client_unref(client);
    return 0;
}
