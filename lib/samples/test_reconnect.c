#include "../api.h"

#include <stdlib.h>
#include <stdio.h>

GMainLoop * loop = NULL;

static void signal_connectivity(mc_Client *client, bool server_changed, bool was_connected)
{
    g_printerr("was_connected=%d is_connected=%d server_changed=%d host=%s, port=%d timeout=%2.2f\n", 
            was_connected,
            server_changed,
            mc_is_connected(client),
            mc_get_host(client),
            mc_get_port(client),
            mc_get_timeout(client)
    );
}

/////////////////////////////

static gboolean quit_loop(void * user_data) 
{
    g_main_loop_quit(loop);
    g_main_loop_unref(loop);
    loop = NULL;

    mc_free((mc_Client *) user_data);
    return FALSE;
}

/////////////////////////////

static gboolean idle_callback(void * user_data)
{
    mc_Client * self = user_data;

    for(int i = 0; i < 10; i++) {
        g_printerr("+ connect\n");
        mc_connect(self, NULL, "localhost", 6666, 2.0);
        g_printerr("- disconnect\n");
        mc_disconnect(self);
    }

    g_timeout_add(5000, quit_loop, self);
    return FALSE;
}

/////////////////////////////

static void reconnect_manytimes(mc_PmType type)
{
    mc_Client * client = mc_create(type);
    mc_signal_add(client, "connectivity", signal_connectivity, NULL);
    mc_misc_register_posix_signal(client);

    g_idle_add(idle_callback, client);

    loop = g_main_loop_new(NULL, FALSE);
    g_printerr("blockit\n");
    g_main_loop_run(loop);
    g_printerr("blockitdone\n");
}

/////////////////////////////

int main(void)
{
    g_printerr("-- MC_PM_IDLE\n");
    reconnect_manytimes(MC_PM_IDLE);
    g_printerr("-- MC_PM_COMMAND\n");
    reconnect_manytimes(MC_PM_COMMAND);
    return EXIT_SUCCESS;
 }
