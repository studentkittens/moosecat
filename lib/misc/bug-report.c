#include "bug-report.h"

/* version */
#include "../config.h"

/* g_strdup_printf */
#include <glib.h>
#include <sqlite3.h>

char * mc_misc_bug_report (mc_Client * client)
{
    bool is_connected = false;
    const unsigned disconnected_version[] = {0, 0, 0};
    const unsigned * server_version = disconnected_version;

    if (mc_proto_is_connected (client) ) {
        is_connected = true;

        mpd_connection * connection = mc_proto_get (client);
        if (connection != NULL) {
            server_version = mpd_connection_get_server_version (connection);
        }
    }

    mc_proto_put (client);

    return g_strdup_printf (
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
               "SQLite3 (needed) : %s (%s)        \n"
               "                                  \n",
               ///////////////////////////////
               /* Library version: */ MC_VERSION, MC_VERSION_GIT_REVISION,
               /* Is Connected:    */ is_connected ? "Yes" : "No",
               /* Server Version:  */ server_version[0], server_version[1], server_version[2],
               /* Hostname:Port:   */ client->_host, client->_port,
               /* Timeout:         */ client->_timeout,
               /* Protocol Machine:*/ client->_pm == MC_PM_IDLE ? "pm_idle" : "pm_command",
               /* Glib (X.Y.Z):    */ HAVE_GLIB ? "Yes" : "No", GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION,
               /* SQLite3:         */ HAVE_SQLITE3 ? "Yes" : "No", SQLITE_VERSION
           );
}
