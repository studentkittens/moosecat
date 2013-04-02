#ifndef MC_CLIENT_COMMAND_LIST_H
#define MC_CLIENT_COMMAND_LIST_H

#include "protocol.h"
#include <stdbool.h>

bool mc_client_command_list_begin(mc_Client *self);
bool mc_client_command_list_commit(mc_Client *self);
bool mc_client_command_list_is_active(mc_Client *self);

#endif /* end of include guard: MC_CLIENT_COMMAND_LIST_H */

