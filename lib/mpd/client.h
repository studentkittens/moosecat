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



/**
 * @brief Initialize client.
 *
 * This is called for you by mc_proto_create()
 *
 * @param self the client to operate on
 */
void mc_client_init(mc_Client *self);

/**
 * @brief Destroy client internal structures.
 *
 * This is called for you by mc_proto_free()
 *
 * @param self the client to operate on
 */
void mc_client_destroy(mc_Client *self);

/**
 * @brief Send a command to the server
 *
 * See HandlerTable
 *
 * @param self the client to operate on
 * @param command the command to send 
 *
 * @return an ID you can pass to mc_client_recv
 */
long mc_client_send(mc_Client *self, const char *command);

/**
 * @brief Receive the response of a command
 *
 * There is no way to get the exact response,
 * you can only check for errors.
 *
 * @param self the client to operate on
 * @param job_id the job id to wait for a response
 *
 * @return true on success, false on error
 */
bool mc_client_recv(mc_Client *self, long job_id);

/**
 * @brief Shortcut for send/recv
 *
 * @param self the client to operate on 
 * @param command same as with mc_client_send
 *
 * @return same as mc_client_recv
 */
bool mc_client_run(mc_Client *self, const char *command);

/**
 * @brief Check if mc_client_begin() was called, but not mc_client_commit()
 *
 * @param self the client to operate on
 *
 * @return true if active
 */
bool mc_client_command_list_is_active(mc_Client *self);


/**
 * @brief Wait for all operations on this client to finish.
 *
 * This does not include Store Operations.
 *
 * @param self the Client to operate on
 */
void mc_client_wait(mc_Client *self);


/**
 * @brief Begin a new Command List.
 *
 * All following commands are hold back and send at once.
 *
 * Use the ID returned by mc_client_commit() to
 * wait for it, if needed.
 *
 *
 * @param self the client to operate on
 */
void mc_client_begin(mc_Client *self);



/**
 * @brief Commit all previously holdback commands.
 *
 * @param self the Client to operate on 
 *
 * @return 
 */
long mc_client_commit(mc_Client *self);


#endif /* end of include guard: CLIENT_H */
