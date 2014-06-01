#include "../moose-api.h"
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    MooseClient * client = moose_client_new(MOOSE_PROTOCOL_IDLE);
    moose_client_connect(client, "localhost", 6666, 20);

    if (moose_client_is_connected(client)) {

        long job_id = 0;

        for (int i = 0; i < 1000; ++i) {
            g_print("SEND: BEGIN\n");
            moose_client_begin(client);
            g_print("SEND: NEXT\n");
            moose_client_send(client, "next");
            g_print("SEND: PREV\n");
            moose_client_send(client, "previous");
            g_print("SEND: PAUSE\n");
            moose_client_send(client, "pause");
            g_print("SEND: END\n");
            job_id = moose_client_commit(client);
            g_print("SEND: IS ACTIVE\n");
            g_printerr("is_active=%d\n", moose_client_command_list_is_active(client));
            g_print("SEND: RECV %ld\n", job_id);
            g_printerr("recv=%d\n", moose_client_recv(client, job_id));
            g_print("AFTER: RECV\n");
        }

        moose_client_wait(client);

    } else {
        puts("Not connected.");
    }

    puts("-- Freeing client.");

    moose_client_unref(client);
    return 0;
}
