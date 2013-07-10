#include "../api.h"


static void zeroconf_callback(struct mc_ZeroconfBrowser * self, void * user_data) 
{
    switch(mc_zeroconf_get_state(self)) {
        case MC_ZEROCONF_STATE_CHANGED: {
            g_printerr("SERVER_LIST:\n");
            struct mc_ZeroconfServer ** server = mc_zeroconf_get_server(self);
            for(int i = 0; server[i]; ++i) {
                struct mc_ZeroconfServer * s = server[i];
                g_printerr("   %s %s %s %s %s %u\n",
                        mc_zeroconf_server_get_host(s),
                        mc_zeroconf_server_get_name(s),
                        mc_zeroconf_server_get_type(s),
                        mc_zeroconf_server_get_domain(s),
                        mc_zeroconf_server_get_addr(s),
                        mc_zeroconf_server_get_port(s)
                );
            }
            g_free(server);
            break;
        }
        case MC_ZEROCONF_STATE_ERROR: {
            g_printerr("-> Uh oh, some error happended: %s\n", 
                    mc_zeroconf_get_error(self)
            );
            break;
        }
        case MC_ZEROCONF_STATE_ALL_FOR_NOW: {
            g_printerr("-> These should be all for now, quitting in 10 seconds\n");
            g_timeout_add(10000, (GSourceFunc)g_main_loop_quit, user_data);
            break;
        }
        case MC_ZEROCONF_STATE_UNCONNECTED:
        default:
            g_printerr("Should not happen...\n");
            break;
    }
}


int main(int argc, char const *argv[])
{
    struct mc_ZeroconfBrowser * browser = mc_zeroconf_new(NULL);
    if(browser != NULL) {
        if(mc_zeroconf_get_state(browser) != MC_ZEROCONF_STATE_UNCONNECTED) {
            GMainLoop * loop = g_main_loop_new(NULL, FALSE);
            mc_zeroconf_register(browser, zeroconf_callback, loop);
            g_main_loop_run(loop);
            g_main_loop_unref(loop);
        } else {
            g_printerr("No avahi daemon running.\n");
        }
        mc_zeroconf_destroy(browser);
    }

    return 0;
}
