#include "../api.h"

#include <stdlib.h>
#include <stdio.h>

int main(void)
{
    mc_Client *client = mc_create(MC_PM_IDLE);
    mc_connect(client, NULL, "localhost", 6600, 2);

    if(mc_is_connected(client)) {

        g_usleep(1000000);

        mc_lock_outputs(client);
        const char **op_list = mc_outputs_get_names(client);

        for (int i = 0; op_list[i]; ++i) {
            g_printerr("NAME %s\n", op_list[i]);
            g_printerr("%20s: %3s\n",
                    op_list[i],
                    mc_outputs_get_state(client, op_list[i]) ? "Yes" : " No"
            );
        }
        mc_unlock_outputs(client);
        
        g_strfreev((char **) op_list);
        mc_client_wait(client);
        mc_disconnect(client);
    }
    mc_free(client);
    return 0;
}
