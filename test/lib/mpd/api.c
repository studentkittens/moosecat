/* First include the fct framework. */
#include "../fct.h"

#include "../../../lib/mpd/protocol.h"


#define test_api(client)                                           \
    {                                                                  \
        /* nothing to check, just shouldn't segfault */                \
        fct_chk(mc_proto_get(client) == NULL);                         \
        mc_proto_put(client);                                          \
        mc_proto_disconnect(client);                                   \
        \
        /* Should yield NULL */                                        \
        fct_chk(mc_proto_create(42) == NULL);                          \
        \
        fct_chk(mc_proto_is_connected(NULL) == false);                 \
        \
        /* Signals */                                                  \
        mc_proto_signal_add (client, "error", NULL, NULL);             \
        mc_proto_signal_rm (client, "error", NULL);                    \
        mc_proto_signal_dispatch (client, "error", 1, 2, 3, 4);        \
        if (client)                                                    \
            fct_chk(mc_proto_signal_length(client, "error") == 0);     \
        else                                                           \
            fct_chk(mc_proto_signal_length(client, "error") == -1);    \
        mc_proto_free(client);                                         \
    }                                                                  \
     
/* Now lets define our testing scope. */
FCT_BGN()
{
    FCT_SUITE_BGN (rape_api) {
        FCT_TEST_BGN (fill_in_nul) {
            mc_Client * client = NULL;
            char * err = mc_proto_connect (client, NULL, NULL, 0, 0);
            fct_chk (err != NULL);
            g_free (err);

            /* the rest */
            test_api (client);
        }
        FCT_TEST_END();

        /////////////////////

        FCT_TEST_BGN (fill_in_invalid) {
            mc_Client * client = mc_proto_create (MC_PM_COMMAND);
            test_api (client);
        }
        FCT_TEST_END();
    }
    FCT_SUITE_END();

}
FCT_END();
