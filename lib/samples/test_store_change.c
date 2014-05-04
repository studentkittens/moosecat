#include "../mpd/protocol.h"
#include "../store/db.h"
#include "../misc/posix-signal.h"

#include <string.h>
#include <stdio.h>

///////////////////////////////

static void print_logging(
    G_GNUC_UNUSED mc_Client *self,
    const char * message,
    mc_LogLevel level,
    G_GNUC_UNUSED gpointer user_data)
{
    g_printerr("Logging(%d): %s\n", level, message);
}

///////////////////////////////

static void print_event(mc_Client *self, enum mpd_idle event, void *user_data)
{
    g_print("event-signal: %p %d %p\n", self, event, user_data);
}

///////////////////////////////

static void print_connectivity(
        mc_Client *self,
        bool server_changed,
        bool was_connected,
        G_GNUC_UNUSED void *user_data)
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
    mc_Client * self = user_data;

    mc_disconnect(self);
    mc_connect(self, NULL, "localhost", 6600, 10.0);

    return false;
}

///////////////////////////////

int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char const *argv[])
{
    mc_Client *self = mc_create(MC_PM_IDLE);

    mc_connect(self, NULL, "localhost", 6666, 10.0);
    mc_signal_add(self, "logging", print_logging, NULL);
    mc_signal_add(self, "client-event", print_event, NULL);
    mc_signal_add(self, "connectivity", print_connectivity, NULL);
    mc_misc_register_posix_signal(self);

    mc_StoreSettings *settings = mc_store_settings_new();
    settings->use_memory_db = TRUE;
    settings->use_compression = FALSE;
    mc_Store *db = mc_store_create(self, settings);

    GMainLoop *loop = g_main_loop_new(NULL, true);
    g_timeout_add(2000, timeout_client_change, self);
    g_timeout_add(7000, timeout_quit, loop);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    mc_store_close(db);
    mc_store_settings_destroy(settings);
    mc_free(self);
}
