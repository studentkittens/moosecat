#include "../moose-api.h"


static void zeroconf_callback(struct MooseZeroconfBrowser * self, void * user_data) 
{
    switch(moose_zeroconf_get_state(self)) {
        case MC_ZEROCONF_STATE_CHANGED: {
            g_printerr("SERVER_LIST:\n");
            struct MooseZeroconfServer ** server = moose_zeroconf_get_server(self);
            for(int i = 0; server[i]; ++i) {
                struct MooseZeroconfServer * s = server[i];
                g_printerr("   %s %s %s %s %s %u\n",
                        moose_zeroconf_server_get_host(s),
                        moose_zeroconf_server_get_name(s),
                        moose_zeroconf_server_get_type(s),
                        moose_zeroconf_server_get_domain(s),
                        moose_zeroconf_server_get_addr(s),
                        moose_zeroconf_server_get_port(s)
                );
            }
            g_free(server);
            break;
        }
        case MC_ZEROCONF_STATE_ERROR: {
            g_printerr("-> Uh oh, some error happended: %s\n", 
                    moose_zeroconf_get_error(self)
            );
            break;
        }
        case MC_ZEROCONF_STATE_ALL_FOR_NOW: {
            g_printerr("-> These should be all for now, quitting n 10 seconds\n");
            g_timeout_add(10000, (GSourceFunc)g_main_loop_quit, user_data);
            break;
        }
        case MC_ZEROCONF_STATE_UNCONNECTED:
        default:
            g_printerr("Should not happen...\n");
            break;
    }
}


int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char const *argv[])
{
    struct MooseZeroconfBrowser * browser = moose_zeroconf_new(NULL);
    if(browser != NULL) {
        if(moose_zeroconf_get_state(browser) != MC_ZEROCONF_STATE_UNCONNECTED) {
            GMainLoop * loop = g_main_loop_new(NULL, FALSE);
            moose_zeroconf_register(browser, zeroconf_callback, loop);
            g_main_loop_run(loop);
            g_main_loop_unref(loop);
        } else {
            g_printerr("No avahi daemon running.\n");
        }
        moose_zeroconf_destroy(browser);
    }

    return 0;
}
