#include "../api.h"

#include <stdlib.h>
#include <stdio.h>

int main(void)
{
    mc_Client *client = mc_proto_create(MC_PM_IDLE);
    mc_proto_connect(client, NULL, "localhost", 6600, 2);

    if (mc_proto_is_connected(client)) {

        long job_id = 0;

        for(int i = 0; i < 10; ++i) {
            job_id = mc_client_send(client, "command_list_begin");
            mc_client_send(client, "next");
            mc_client_send(client, "previous");
            mc_client_send(client, "pause");
            mc_client_send(client, "command_list_end");
            g_printerr("is_active=%d\n", mc_client_command_list_is_active(client));
            g_printerr("recv=%d\n", mc_client_recv(client, job_id));
        }


        mc_client_wait(client);

    } else {
        puts("Not connected.");
    }

    puts("-- Freeing client.");

    mc_proto_free(client);
    return 0;
}
