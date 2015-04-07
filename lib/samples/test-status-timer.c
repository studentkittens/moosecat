#include "../moose-api.h"

static int COUNTER = 0;
static GMainLoop *LOOP = NULL;
static GTimer *TIMER = NULL;

#define TIMEOUT 0.1

static void event_cb(G_GNUC_UNUSED MooseClient *client,
                     MooseIdle event,
                     G_GNUC_UNUSED void *user_data) {
    if(event & MOOSE_IDLE_STATUS_TIMER_FLAG) {
        COUNTER++;
    }

    g_printerr("#%02d event = %d\n", COUNTER, event);

    if(COUNTER == 20) {
        g_main_loop_quit(LOOP);
        g_printerr("%f\n", g_timer_elapsed(TIMER, NULL));
    }
}

static void test_status_timer_real(MooseProtocolType pm) {
    MooseClient *client = moose_client_new(pm);
    moose_client_connect_to(client, "localhost", 6666, 20);
    g_signal_connect(client, "client-event", G_CALLBACK(event_cb), NULL);

    if(moose_client_is_connected(client)) {
        g_object_set(client, "timer-interval", TIMEOUT, NULL);
        g_object_set(client, "timer-only-when-playing", FALSE, NULL);
        g_timer_start(TIMER);

        moose_client_timer_set_active(client, TRUE);
        moose_client_send_single(client, "play");
        moose_client_send_single(client, "pause");

        LOOP = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(LOOP);
        g_main_loop_unref(LOOP);
        LOOP = NULL;

        g_printerr("Disconnecting...\n");
        moose_client_disconnect(client);
    } else {
        moose_warning("Test server running?");
        g_assert_not_reached();
    }
    moose_client_unref(client);
}

static void test_status_timer_cmd(void) {
    COUNTER = 0;
    TIMER = g_timer_new();
    test_status_timer_real(MOOSE_PROTOCOL_COMMAND);
    g_timer_destroy(TIMER);
    g_printerr("---\n");
}

/*
static void test_status_timer_idle(void) {
    COUNTER = 0;
    TIMER = g_timer_new();
    test_status_timer_real(MOOSE_PROTOCOL_IDLE);
    g_timer_destroy(TIMER);
    g_printerr("---\n");
}
*/

int main(int argc, char **argv) {
    // moose_debug_install_handler();

    g_test_init(&argc, &argv, NULL);
    // g_test_add_func("/mpd/client/status-timer-cmd", test_status_timer_cmd);
    g_test_add_func("/mpd/client/status-timer-idle", test_status_timer_cmd);
    g_test_add_func("/mpd/client/status-timer-idle", test_status_timer_cmd);
    return g_test_run();
}
