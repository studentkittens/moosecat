#include "../mpd/moose-mpd-protocol.h"
#include "../store/moose-store.h"
#include "../store/moose-store-completion.h"

#include <string.h>
#include <stdio.h>

///////////////////////////////

static void print_logging(
    G_GNUC_UNUSED MooseClient * self,
    const char * message,
    MooseLogLevel level,
    G_GNUC_UNUSED gpointer user_data)
{
    g_printerr("Logging(%d): %s\n", level, message);
}

///////////////////////////////

static void print_event(
    MooseClient * self,
    enum mpd_idle event,
    void * user_data)
{
    g_print("event-signal: %p %d %p\n", self, event, user_data);
}

///////////////////////////////

static void print_connectivity(
    MooseClient * self,
    bool server_changed,
    bool was_connected,
    G_GNUC_UNUSED void * user_data)
{
    g_print("connectivity-signal: changed=%d was_connected=%d is_connected=%d\n",
            server_changed, was_connected, moose_client_is_connected(self)
            );
}

///////////////////////////////

static gboolean timeout_quit(gpointer user_data)
{
    g_main_loop_quit((GMainLoop *)user_data);

    return false;
}

///////////////////////////////

static gboolean timeout_client_change(gpointer user_data)
{
    MooseStore * store = user_data;
    MooseStoreCompletion * cmpl = moose_store_get_completion(store);
    g_printerr("C %p\n", cmpl);
    g_printerr("P %s\n", moose_store_cmpl_lookup(cmpl, MPD_TAG_ARTIST, "Akrea"));
    g_printerr("P %s\n", moose_store_cmpl_lookup(cmpl, MPD_TAG_ARTIST, "Knork"));
    return false;
}

///////////////////////////////

int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char const * argv[])
{
    MooseClient * self = moose_client_create(MOOSE_PM_IDLE);

    moose_client_connect(self, NULL, "localhost", 6600, 10.0);
    moose_client_signal_add(self, "logging", print_logging, NULL);
    moose_client_signal_add(self, "client-event", print_event, NULL);
    moose_client_signal_add(self, "connectivity", print_connectivity, NULL);

    MooseStoreSettings * settings = moose_store_settings_new();
    settings->use_memory_db = TRUE;
    settings->use_compression = FALSE;
    MooseStore * db = moose_store_create(self, settings);

    GMainLoop * loop = g_main_loop_new(NULL, true);
    g_timeout_add(3000, timeout_client_change, db);
    g_timeout_add(5000, timeout_quit, loop);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    moose_store_close(db);
    moose_store_settings_destroy(settings);
    moose_client_unref(self);
}
