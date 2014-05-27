#include "../mpd/moose-mpd-client.h"
#include "../store/moose-store.h"

#include <string.h>
#include <stdio.h>

static void print_event(MooseClient * self, MooseIdle event, void * user_data) {
    g_print("event-signal: %p %d %p\n", self, event, user_data);
}

static void print_connectivity(
    MooseClient * self,
    bool server_changed,
    G_GNUC_UNUSED void * user_data) {
    g_print("connectivity-signal: changed=%d is_connected=%d\n",
            server_changed, moose_client_is_connected(self)
           );
}

static gboolean timeout_quit(gpointer user_data) {
    g_main_loop_quit((GMainLoop *)user_data);

    return false;
}

static gboolean timeout_client_change(gpointer user_data) {
    MooseClient * self = user_data;

    moose_client_disconnect(self);
    moose_client_connect(self, NULL, "localhost", 6600, 10.0);

    return false;
}

int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char const * argv[]) {
    MooseClient * self = moose_client_new(MOOSE_PROTOCOL_IDLE);

    moose_client_connect(self, NULL, "localhost", 6601, 10.0);
    g_signal_connect(self, "client-event", G_CALLBACK(print_event), NULL);
    g_signal_connect(self, "connectivity", G_CALLBACK(print_connectivity), NULL);

    MooseStore * db = moose_store_new_full(self, NULL, NULL, TRUE, FALSE);

    GMainLoop * loop = g_main_loop_new(NULL, true);
    g_timeout_add(2000, timeout_client_change, self);
    g_timeout_add(7000, timeout_quit, loop);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    moose_store_unref(db);
    moose_client_unref(self);
}
