#include "idle_core.h"

#include <glib.h>


///////////////////////
// Private Interface //
///////////////////////
    
static const char * idler_do_connect(const char * host, int port, int timeout)
{
    return NULL;
}

///////////////////////

static mpd_connection * idler_do_put(void)
{
    return NULL;
}

///////////////////////

static bool idler_do_disconnect(void)
{
    return true;
}

//////////////////////
// Public Interface //
//////////////////////

Proto_IdleConnector * proto_create_idler(void)
{
    Proto_IdleConnector * ctor = g_new0(Proto_IdleConnector, 1);
    ctor->logic.do_disconnect = idler_do_disconnect;
    ctor->logic.do_put = idler_do_put;
    ctor->logic.do_connect = idler_do_connect;
    ctor->con = NULL; // <-- write connect code
    return ctor;
}
