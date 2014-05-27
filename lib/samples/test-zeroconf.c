#include "../moose-api.h"

static void zeroconf_callback(MooseZeroconfBrowser * self, void * user_data) {
    switch (moose_zeroconf_browser_get_state(self)) {
    case MOOSE_ZEROCONF_STATE_CHANGED: {
        g_printerr("SERVER_LIST:\n");
        GList * server_list = moose_zeroconf_browser_get_server_list(self);
        for (GList * i = server_list; i; i = i->next) {
            MooseZeroconfServer * server = i->data;

#define PRINT(THING) \
{ \
char * string = moose_zeroconf_server_get_ ## THING(server); \
g_printerr("%15s: %s\n", # THING, string); \
g_free(string); \
} \
 
            PRINT(name);
            PRINT(protocol);
            PRINT(domain);
            PRINT(addr);
            PRINT(host);
            g_printerr("%15s: %d\n", "port", moose_zeroconf_server_get_port(server));
            g_printerr("\n");
        }
        g_list_free_full(server_list, (GDestroyNotify)g_object_unref);
        break;
    }
    case MOOSE_ZEROCONF_STATE_ERROR: {
        g_printerr("-> Uh oh, some error happended: %s\n",
                   moose_zeroconf_browser_get_error(self)
                  );
        break;
    }
    case MOOSE_ZEROCONF_STATE_ALL_FOR_NOW: {
        g_printerr("-> These should be all for now, quitting n 10 seconds\n");
        g_timeout_add(10000, (GSourceFunc)g_main_loop_quit, user_data);
        break;
    }
    case MOOSE_ZEROCONF_STATE_UNCONNECTED:
    default:
        g_printerr("Should not happen...\n");
        break;
    }
}

int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char const * argv[]) {
    MooseZeroconfBrowser * browser = moose_zeroconf_browser_new();
    if (browser != NULL) {
        if (moose_zeroconf_browser_get_state(browser) != MOOSE_ZEROCONF_STATE_UNCONNECTED) {
            GMainLoop * loop = g_main_loop_new(NULL, FALSE);
            g_signal_connect(browser, "state-changed", G_CALLBACK(zeroconf_callback), loop);
            g_main_loop_run(loop);
            g_main_loop_unref(loop);
        } else {
            g_printerr("No avahi daemon running.\n");
        }
        moose_zeroconf_browser_destroy(browser);
    }

    return 0;
}
