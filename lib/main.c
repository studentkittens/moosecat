#include "mpd/protocol.h"
#include "mpd/protocol/idle_core.h"
#include <glib.h>

int main (void)
{
    Proto_Connector * conn = proto_create_idler();
    const char * err = proto_connect (conn, "localhost", 6600, 2);
    if (err != NULL)
    {
        g_print ("Cannot connect.\n");
    }
    else
    {
        mpd_connection * c = proto_get (conn);
        g_print ("Got connection: %p\n", c);
        proto_put (conn);

        proto_disconnect (conn);
        conn = NULL;
    }
    return 0;
}
