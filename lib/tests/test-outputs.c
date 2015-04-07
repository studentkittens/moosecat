#include "../moose-api.h"
#include <stdlib.h>
#include <stdio.h>

static GMainLoop *LOOP = NULL;
static int COUNTER = 0;
static gboolean STATE;

static void event_cb(MooseClient *client, G_GNUC_UNUSED MooseIdle events,
                     G_GNUC_UNUSED gpointer user_data) {
    g_assert(client);

    MooseStatus *status = moose_client_ref_status(client);
    g_assert(status);

    GHashTable *outputs = moose_status_outputs_get(status);
    g_assert(outputs);

    GHashTableIter iter;
    gchar *name = NULL;
    GVariant *output = NULL;

    g_hash_table_iter_init(&iter, outputs);
    while(g_hash_table_iter_next(&iter, (gpointer *)&name, (gpointer *)&output)) {
        int output_id;
        char *same_as_name;
        gboolean new_state;

        g_variant_get(output, "(sib)", &same_as_name, &output_id, &new_state);
        moose_message("(#%d) %s: %s", output_id, name, new_state ? "Yes" : "No");

        g_assert_cmpstr(name, ==, same_as_name);
        if(COUNTER != 0) {
            g_assert(new_state != STATE);
        }

        g_assert(output_id == 0);
        g_assert_cmpstr(name, ==, "YouHopefullyDontUseThisDoYou");

        STATE = new_state;
        g_free(same_as_name);
    }

    g_hash_table_unref(outputs);
    moose_status_unref(status);

    if(COUNTER == 0) {
        g_assert(events == (MooseIdle)INT_MAX);

        moose_message("Switching output %s.", !STATE ? "On" : "Off");
        GVariant *variant = g_variant_new("(ssb)", "output-switch",
                                          "YouHopefullyDontUseThisDoYou", !STATE);
        moose_client_send_variant(client, variant);
        g_variant_unref(variant);
    } else {
        g_assert(events & MOOSE_IDLE_OUTPUT);
        g_main_loop_quit(LOOP);
    }

    ++COUNTER;
}

static void test_outputs_real(MooseProtocolType pm) {
    MooseClient *client = moose_client_new(pm);
    moose_client_connect_to(client, "localhost", 6666, 20);
    g_signal_connect(client, "client-event", G_CALLBACK(event_cb), NULL);

    if(moose_client_is_connected(client)) {
        LOOP = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(LOOP);
        g_main_loop_unref(LOOP);

        moose_client_disconnect(client);
    } else {
        moose_warning("Test server running?");
        g_assert_not_reached();
    }
    moose_client_unref(client);
}

static void test_outputs_cmd(void) {
    STATE = COUNTER = 0;
    test_outputs_real(MOOSE_PROTOCOL_COMMAND);
}

static void test_outputs_idle(void) {
    STATE = COUNTER = 0;
    test_outputs_real(MOOSE_PROTOCOL_IDLE);
}

int main(int argc, char **argv) {
    moose_debug_install_handler();
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/mpd/client/outputs", test_outputs_idle);
    g_test_add_func("/mpd/client/outputs", test_outputs_cmd);
    return g_test_run();
}
