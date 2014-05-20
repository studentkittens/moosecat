#include "../moose-api.h"

static void signal_event(
    MooseClient * client,
    enum mpd_idle event,
    void * user_data)
{
    g_printerr("event = %d\n", event);
    MooseStatus * status = moose_ref_status(client);
    g_printerr("State: %d\n", moose_status_get_state(status));
    moose_status_unref(status);
    g_printerr("Unlocked.\n");

    static int counter = 0;
    if (++counter == 15) {
        g_main_loop_quit((GMainLoop *)user_data);
    }
}

/////////////////////////////

int main(void)
{
    GMainLoop * loop = g_main_loop_new(NULL, FALSE);

    MooseClient * client = moose_create(MOOSE_PM_IDLE);
    moose_signal_add(client, "client-event", signal_event, loop);
    moose_status_timer_register(client, 50, true);
    moose_connect(client, NULL, "localhost", 6601, 2.0);

    g_printerr("Is %d\n", moose_is_connected(client));

    /* BLOCKS */
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    moose_free(client);
    return 0;
}
