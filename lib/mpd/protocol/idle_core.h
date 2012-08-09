#include "../protocol.h"

#ifndef IDLE_CORE_H
#define IDLE_CORE_H

/**
 * @brief Connector using only one connection and the idle command.
 *
 * Sending a command:
 * - leave idle state, by sending 'noidle' async.
 * - send command and read response
 * - while idle_respone != NULL: issue 'idle'.
 *
 * Used by ncmpc and friends.
 */


/**
 * @brief Create an Proto_IdleConnector
 *
 * See Proto_IdleConnector for more description.
 *
 * @return a newly allocated connector
 */
Proto_Connector * proto_create_idler (void);

#endif /* end of include guard: IDLE_CORE_H */
