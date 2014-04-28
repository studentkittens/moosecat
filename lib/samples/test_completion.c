#include "../mpd/protocol.h"
#include "../store/db.h"
#include "../store/db-completion.h"
#include "../misc/posix-signal.h"

#include <string.h>
#include <stdio.h>

///////////////////////////////

static void print_logging(mc_Client *self, const char * message, mc_LogLevel level, gpointer user_data)
{
    g_printerr("Logging(%d): %s\n", level, message);
}

///////////////////////////////

static void print_event(mc_Client *self, enum mpd_idle event, void *user_data)
{
    g_print("event-signal: %p %d %p\n", self, event, user_data);
}

///////////////////////////////

static void print_connectivity(mc_Client *self, bool server_changed, bool was_connected, void *user_data)
{
    g_print("connectivity-signal: changed=%d was_connected=%d is_connected=%d\n",
            server_changed, was_connected, mc_is_connected(self)
    );
}

///////////////////////////////

static gboolean timeout_quit(gpointer user_data)
{
    g_main_loop_quit((GMainLoop *) user_data);

    return false;
}

///////////////////////////////

static gboolean timeout_client_change(gpointer user_data)
{
    mc_Client * store = user_data;
    mc_StoreCompletion *cmpl = mc_store_cmpl_new((struct mc_Store *) store);
    g_printerr("P %s\n", mc_store_cmpl_lookup(cmpl, MPD_TAG_ARTIST, "Akrea"));
    g_printerr("P %s\n", mc_store_cmpl_lookup(cmpl, MPD_TAG_ARTIST, "Knork"));
    mc_store_cmpl_free(cmpl);
    return false;
}

///////////////////////////////

int main(int argc, char *argv[])
{
    mc_Client *self = mc_create(MC_PM_IDLE);

    mc_connect(self, NULL, "localhost", 6600, 10.0);
    mc_signal_add(self, "logging", print_logging, NULL);
    mc_signal_add(self, "client-event", print_event, NULL);
    mc_signal_add(self, "connectivity", print_connectivity, NULL);
    mc_misc_register_posix_signal(self);

    mc_StoreSettings *settings = mc_store_settings_new();
    settings->use_memory_db = TRUE;
    settings->use_compression = FALSE;
    mc_Store *db = mc_store_create(self, settings);

    GMainLoop *loop = g_main_loop_new(NULL, true);
    g_timeout_add(1000, timeout_client_change, db);
    g_timeout_add(2000, timeout_quit, loop);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    mc_store_close(db);
    mc_store_settings_destroy(settings);
    mc_free(self);
}
