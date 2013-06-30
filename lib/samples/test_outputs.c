#include "../api.h"

#include <stdlib.h>
#include <stdio.h>

int main(void)
{
    mc_Client *client = mc_create(MC_PM_IDLE);
    mc_connect(client, NULL, "localhost", 6600, 2);
    int op_size = 0;
    struct mpd_output **op_list = mc_get_outputs(client, &op_size);

    for (int i = 0; i < op_size; ++i) {
        struct mpd_output *output = op_list[i];
        printf("%d\t%d\t%s\n",
               mpd_output_get_id(output),
               mpd_output_get_enabled(output),
               mpd_output_get_name(output));
    }

    mc_disconnect(client);
    mc_free(client);
    return 0;
}
