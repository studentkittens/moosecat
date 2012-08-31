#include "bug_report.h"

/* version */
#include "../config.h"

/* g_strdup_printf */
#include <glib.h>

char * mc_misc_bug_report (mc_Client * client)
{
    bool is_connected = false;
    const unsigned disconnected_version[] = {0, 0, 0};
    const unsigned * server_version = disconnected_version;

    if (mc_proto_is_connected(client))
    {
        is_connected = true;
        
        mpd_connection * connection = mc_proto_get (client);
        if (connection != NULL)
        {
            server_version = mpd_connection_get_server_version (connection);
        }
    } 

    mc_proto_put (client);

    return g_strdup_printf (
            "libmoosecat configuration:      \n"
            "==========================      \n"
            "                                \n"
            "Library Version  : %s           \n"
            "Is Connected     : %s           \n"
            "Server-Version   : %d.%d.%d     \n"
            "                                \n"
            "Hostname         : %s:%d        \n"
            "Timeout          : %d second(s) \n"
            "Protocol Machine : %s           \n"
            "                                \n"
            "Compiled with:                  \n"
            "--------------                  \n"
            "GLib (needed)    : %s           \n"
            "SQLite3 (needed) : %s           \n"
            "                                \n",
            ///////////////////////////////
            /* Library version: */ MC_VERSION,
            /* Is Connected:    */ is_connected ? "Yes" : "No",
            /* Server Version:  */ server_version[0], server_version[1], server_version[2],
            /* Hostname:Port:   */ client->_host, client->_port,
            /* Timeout:         */ client->_timeout,
            /* Protocol Machine:*/ client->_pm == MC_PM_IDLE ? "pm_idle" : "pm_command",
            /* Glib:            */ HAVE_GLIB ? "Yes" : "No",
            /* SQLite3:         */ HAVE_SQLITE3 ? "Yes" : "No"
    );
}
