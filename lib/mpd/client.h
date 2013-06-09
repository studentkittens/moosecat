#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"

/**
 * client.h implements the actual commands, which are
 * sendable to the server.
 *
 * This excludes database commands, which are only accessible
 * via the store/ module.
 */

void mc_client_init(mc_Client *self);
void mc_client_destroy(mc_Client *self);
long mc_client_send(mc_Client *self, const char *command);
bool mc_client_recv(mc_Client *self, long job_id);
bool mc_client_run(mc_Client *self, const char *command);
bool mc_client_command_list_is_active(mc_Client *self);
void mc_client_wait(mc_Client *self);


void mc_client_begin(mc_Client *self);
void mc_client_commit(mc_Client *self);


#endif /* end of include guard: CLIENT_H */

