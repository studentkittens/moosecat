#include "bug-report.h"

/* g_strdup_printf */
#include <glib.h>

/* version */
#include "../config.h"

char *mc_misc_bug_report(mc_Client *client)
{
    bool is_connected = false;
    const unsigned disconnected_version[] = {0, 0, 0};
    const unsigned *server_version = disconnected_version;
    double timeout = -1.0;
    const char *host = "<not connected>";
    int port = -1;
    mc_PmType pm = 0;

    if (client != NULL) {
        if (mc_is_connected(client)) {
            is_connected = true;
            struct mpd_connection *connection = mc_get(client);

            if (connection != NULL) {
                server_version = mpd_connection_get_server_version(connection);
            }
        }

        g_rec_mutex_lock(&client->_client_attr_mutex);
        timeout = client->_timeout;
        host = client->_host;
        port = client->_port;
        pm = client->_pm;
        g_rec_mutex_unlock(&client->_client_attr_mutex);
        mc_put(client);
    }

    return g_strdup_printf(
               "libmoosecat configuration:        \n"
               "==========================        \n"
               "                                  \n"
               "Library Version  : %s (rev: %s)   \n"
               "Is Connected     : %s             \n"
               "Server-Version   : %d.%d.%d       \n"
               "                                  \n"
               "Hostname         : %s:%d          \n"
               "Timeout          : %.3f second(s) \n"
               "Protocol Machine : %s             \n"
               "                                  \n"
               "Compiled with:                    \n"
               "--------------                    \n"
               "GLib (needed)    : %s (%d.%d.%d)  \n"
               "                                  \n",
               ///////////////////////////////
               /* Library version: */ MC_VERSION, MC_VERSION_GIT_REVISION,
               /* Is Connected:    */ is_connected ? "Yes" : "No",
               /* Server Version:  */ server_version[0], server_version[1], server_version[2],
               /* Hostname:Port:   */ host, port,
               /* Timeout:         */ timeout,
               /* Protocol Machine:*/ pm == MC_PM_IDLE ? "pm_idle" : "pm_command",
               /* Glib (X.Y.Z):    */ HAVE_GLIB ? "Yes" : "No", GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION
           );
}
