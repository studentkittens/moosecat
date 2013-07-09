#include "../api.h"

#include <stdlib.h>
#include <stdio.h>

int main(void)
{
    mc_Client *client = mc_create(MC_PM_IDLE);
    mc_connect(client, NULL, "localhost", 6600, 2);

    if(mc_is_connected(client)) {

        /* Wait till a client update is done. 
         * This is fired in another thread.
         * We want to get sure we already have an output available here.
         */
        mc_block_till_sync(client);

        mc_lock_outputs(client);
        const char **op_list = mc_outputs_get_names(client);

        for (int i = 0; op_list[i]; ++i) {
            g_printerr("%20s: %3s\n",
                    op_list[i],
                    mc_outputs_get_state(client, op_list[i]) ? "Yes" : " No"
            );
        }
        mc_unlock_outputs(client);
        g_free(op_list);
        
        mc_client_wait(client);
        mc_disconnect(client);
    }
    mc_free(client);
    return 0;
}
