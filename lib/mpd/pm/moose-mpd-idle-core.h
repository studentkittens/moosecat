#include "../moose-mpd-protocol.h"

#ifndef IDLE_CORE_H
#define IDLE_CORE_H

/**
 * @brief Create an MooseClient
 *
 * @brief Connector using only one connection and the idle command.
 *
 * Sending a command:
 * - leave idle state, by sending 'noidle' async.
 * - send command and read response
 * - while idle_respone != NULL: issue 'idle'.
 *
 * Used by ncmpc and friends.
 *
 * @return a newly allocated connector
 */
MooseClient * moose_create_idler(void);

#endif /* end of include guard: IDLE_CORE_H */
