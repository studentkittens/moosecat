/*
 * client.h defines actual commmands, like next, prev
 * or listplaylist. It tries to get the connection
 * from the protocol-machine and sends a command
 * synchronously over it, the reponse is parsed,
 * and maybe returned or stored in the database.
 */

#include "client.h"
#include "client_private.h"

///////////////////

// Implemted as example
void mc_client_next (mc_Client * self)
{
    COMMAND (mpd_run_next (conn) )
}

///////////////////

void mc_client_volume (mc_Client * self, int volume)
{
    COMMAND (mpd_run_set_volume (conn, volume) )
}

///////////////////

bool mc_client_password (mc_Client * self, const char * pwd)
{
    bool rc = false;
    COMMAND (rc = mpd_run_password (conn, pwd) );
    return rc;
}
