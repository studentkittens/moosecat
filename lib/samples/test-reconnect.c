#include "../moose-api.h"

#include <stdlib.h>
#include <stdio.h>

GMainLoop *loop = NULL;

static void signal_connectivity(MooseClient *client,
                                bool server_changed,
                                void *user_data) {
    (void)user_data;
    g_printerr("is_connected=%d server_changed=%d host=%s, port=%d timeout=%2.2f\n",
               server_changed,
               moose_client_is_connected(client),
               moose_client_get_host(client),
               moose_client_get_port(client),
               moose_client_get_timeout(client));
}

static gboolean quit_loop(void *user_data) {
    g_main_loop_quit(loop);
    g_main_loop_unref(loop);
    loop = NULL;

    moose_client_unref((MooseClient *)user_data);
    return FALSE;
}

static gboolean idle_callback(void *user_data) {
    MooseClient *self = user_data;

    for(int i = 0; i < 10; i++) {
        g_printerr("+ connect\n");
        moose_client_connect_to(self, "localhost", 6666, 2.0);
        g_printerr("- disconnect\n");
        moose_client_disconnect(self);
    }

        moose_client_connect_to(self, "localhost", 6666, 2.0);
    g_timeout_add(50000, quit_loop, self);
    return FALSE;
}

static void reconnect_manytimes(void) {
    MooseClient *client = moose_client_new(MOOSE_PROTOCOL_IDLE);
    g_signal_connect(client, "connectivity", G_CALLBACK(signal_connectivity), NULL);

    g_idle_add(idle_callback, client);

    loop = g_main_loop_new(NULL, FALSE);
    g_printerr("blockit\n");
    g_main_loop_run(loop);
    g_printerr("blockitdone\n");
}

int main(void) {
    g_printerr("-- MOOSE_PM_IDLE\n");
    reconnect_manytimes();
    g_printerr("-- MOOSE_PM_COMMAND\n");
    reconnect_manytimes();
    return EXIT_SUCCESS;
}
