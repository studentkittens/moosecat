#include "../mpd/moose-mpd-client.h"
#include "../store/moose-store.h"
#include "../store/moose-store-completion.h"

#include <string.h>
#include <stdio.h>

static void print_event(MooseClient *self, MooseIdle event, void *user_data) {
    g_print("event-signal: %p %d %p\n", self, event, user_data);
}

static void print_connectivity(MooseClient *self,
                               bool server_changed,
                               G_GNUC_UNUSED void *user_data) {
    g_print("connectivity-signal: changed=%d is_connected=%d\n", server_changed,
            moose_client_is_connected(self));
}

static gboolean timeout_quit(gpointer user_data) {
    g_main_loop_quit((GMainLoop *)user_data);

    return false;
}

static gboolean timeout_client_change(gpointer user_data) {
    MooseStore *store = user_data;
    MooseStoreCompletion *cmpl = moose_store_get_completion(store);
    g_printerr("C %p\n", cmpl);
    g_printerr("P %s\n", moose_store_completion_lookup(cmpl, MOOSE_TAG_ARTIST, "Akrea"));
    g_printerr("P %s\n", moose_store_completion_lookup(cmpl, MOOSE_TAG_ARTIST, "Knork"));
    return false;
}

int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char const *argv[]) {
    MooseClient *self = moose_client_new(MOOSE_PROTOCOL_IDLE);

    moose_client_connect_to(self, "localhost", 6600, 10.0);
    g_signal_connect(self, "client-event", G_CALLBACK(print_event), NULL);
    g_signal_connect(self, "connectivity", G_CALLBACK(print_connectivity), NULL);

    MooseStore *db = moose_store_new_full(self, NULL, NULL, TRUE, FALSE);

    GMainLoop *loop = g_main_loop_new(NULL, true);
    g_timeout_add(3000, timeout_client_change, db);
    g_timeout_add(5000, timeout_quit, loop);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    moose_store_unref(db);
    moose_client_unref(self);
}
