#include "../api.h"

#include <stdlib.h>
#include <stdio.h>

int main(void)
{
    MooseClient *client = moose_create(MC_PM_IDLE);
    moose_connect(client, NULL, "localhost", 6600, 2);

    if(moose_is_connected(client)) {

        /* Wait till a client update is done. 
         * This is fired in another thread.
         * We want to get sure we already have an output available here.
         */
        moose_block_till_sync(client);

        moose_lock_outputs(client);
        const char **op_list = moose_outputs_get_names(client);

        for (int i = 0; op_list[i]; ++i) {
            g_printerr("%20s: %3s\n",
                    op_list[i],
                    moose_outputs_get_state(client, op_list[i]) ? "Yes" : " No"
            );
        }
        moose_unlock_outputs(client);
        g_free(op_list);
        
        moose_client_wait(client);
        moose_disconnect(client);
    }
    moose_free(client);
    return 0;
}
