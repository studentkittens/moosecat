#include "bug_report.h"

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
        
        struct mpd_connection * connection = mc_proto_get (client);
        if (connection != NULL)
        {
            server_version = mpd_connection_get_server_version (connection);
        }
        mc_proto_put (client);
    } 

    return g_strdup_printf(
            "Is Connected    : %s\n"
            "Server-Version  : %d.%d.%d\n",
            is_connected ? "Yes" : "No",
            server_version[0], server_version[1], server_version[2]
    );
}
