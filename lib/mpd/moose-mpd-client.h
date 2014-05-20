#ifndef CLIENT_H
#define CLIENT_H

#include "moose-mpd-protocol.h"

/**
 * moose-mpd-client.h implements the actual commands, which are
 * sendable to the server.
 *
 * This excludes database commands, which are only accessible
 * via the store/ module.
 */


/**
 * @brief Initialize client.
 *
 * This is called for you by moose_create()
 *
 * @param self the client to operate on
 */
void moose_client_init(MooseClient * self);

/**
 * @brief Destroy client internal structures.
 *
 * This is called for you by moose_free()
 *
 * @param self the client to operate on
 */
void moose_client_destroy(MooseClient * self);

/**
 * @brief Send a command to the server
 *
 * See HandlerTable
 *
 * @param self the client to operate on
 * @param command the command to send
 *
 * @return an ID you can pass to moose_client_recv
 */
long moose_client_send(MooseClient * self, const char * command);

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
bool moose_client_recv(MooseClient * self, long job_id);

/**
 * @brief Shortcut for send/recv
 *
 * @param self the client to operate on
 * @param command same as with moose_client_send
 *
 * @return same as moose_client_recv
 */
bool moose_client_run(MooseClient * self, const char * command);

/**
 * @brief Check if moose_client_begin() was called, but not moose_client_commit()
 *
 * @param self the client to operate on
 *
 * @return true if active
 */
bool moose_client_command_list_is_active(MooseClient * self);


/**
 * @brief Wait for all operations on this client to finish.
 *
 * This does not include Store Operations.
 *
 * @param self the Client to operate on
 */
void moose_client_wait(MooseClient * self);


/**
 * @brief Begin a new Command List.
 *
 * All following commands are hold back and send at once.
 *
 * Use the ID returned by moose_client_commit() to
 * wait for it, if needed.
 *
 *
 * @param self the client to operate on
 */
void moose_client_begin(MooseClient * self);



/**
 * @brief Commit all previously holdback commands.
 *
 * @param self the Client to operate on
 *
 * @return
 */
long moose_client_commit(MooseClient * self);


#endif /* end of include guard: CLIENT_H */
