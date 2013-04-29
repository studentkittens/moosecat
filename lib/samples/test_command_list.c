#include "../api.h"

#include <stdlib.h>
#include <stdio.h>

int main(void)
{
    mc_Client *client = mc_proto_create(MC_PM_IDLE);
    mc_proto_connect(client, NULL, "localhost", 6600, 2);

    if (mc_proto_is_connected(client)) {
        mc_client_pause(client);
        mc_client_pause(client);


        mc_client_command_list_begin(client);
        /* nothing */
        mc_client_command_list_commit(client);


        g_print("%d\n", mc_client_command_list_is_active(client));

        mc_client_command_list_begin(client);
        g_print("%d\n", mc_client_command_list_is_active(client));
        mc_client_next(client);
        mc_client_previous(client);
        mc_client_command_list_commit(client);
        g_print("%d\n", mc_client_command_list_is_active(client));


        mc_client_command_list_begin(client);
        mc_client_command_list_begin(client);
        /* nothing */
        mc_client_command_list_commit(client);
        mc_client_command_list_commit(client);

    } else {
        puts("Not connected.");
    }

    mc_proto_free(client);
    return 0;
}
