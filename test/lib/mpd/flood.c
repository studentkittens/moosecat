#include "clar.h"

#include "../../../lib/mpd/protocol.h"
#include "../../../lib/mpd/pm/cmnd_core.h"
#include "../../../lib/mpd/client.h"


static mc_Client * conn = NULL;

void test_flood__initialize (void)
{
    conn = mc_proto_create_cmnder();
    mc_proto_connect (conn, NULL, "localhost", 6600, 2);
}

void test_flood__cleanup (void)
{
    mc_proto_disconnect (conn);
    mc_proto_free (conn);
}

void test_flood__do_flooding (void)
{
    puts ("Starting volume flood");
    for (int i = 0; i < 1000; i++)
    {
        mc_volume (conn, i % 100);
    }
    puts ("End of volume flood");
}
