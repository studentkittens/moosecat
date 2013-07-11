#include "../api.h"

#include <stdlib.h>
#include <stdio.h>

int main(void)
{
    mc_Client *client = mc_create(MC_PM_IDLE);
    mc_connect(client, NULL, "localhost", 6666, 2);

    if (mc_is_connected(client)) {

        long job_id = 0;

        for(int i = 0; i < 1000; ++i) {
            g_print("SEND: BEGIN\n");
            mc_client_begin(client);
            g_print("SEND: NEXT\n");
            mc_client_send(client, "next");
            g_print("SEND: PREV\n");
            mc_client_send(client, "previous");
            g_print("SEND: PAUSE\n");
            mc_client_send(client, "pause");
            g_print("SEND: END\n");
            job_id = mc_client_commit(client);
            g_print("SEND: IS ACTIVE\n");
            g_printerr("is_active=%d\n", mc_client_command_list_is_active(client));
            g_print("SEND: RECV %ld\n", job_id);
            g_printerr("recv=%d\n", mc_client_recv(client, job_id));
            g_print("AFTER: RECV\n");
        }


        mc_client_wait(client);

    } else {
        puts("Not connected.");
    }

    puts("-- Freeing client.");

    mc_free(client);
    return 0;
}
