/*
 * client.h defines actual commmands, like next, prev
 * or listplaylist. It tries to get the connection
 * from the protocol-machine and sends a command
 * synchronously over it, the reponse is parsed,
 * and maybe returned or stored in the database.
 */

#include "client.h"

///////////////////

/* Template to define new commands */
#define COMMAND(_code_)  {                   \
        mpd_connection * conn = proto_get(self); \
        if(conn != NULL)                         \
        {                                        \
            _code_;                               \
        }                                        \
        proto_put(self);                         \
    }                                            \
     
///////////////////

// Implemted as example
void next (Proto_Connector * self)
{
    COMMAND (mpd_run_next (conn) )
}

///////////////////
