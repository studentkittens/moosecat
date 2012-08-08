#include "../protocol.h"

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
typedef struct {
  Proto_Connector logic;
  
  mpd_connection * con;
} Proto_IdleConnector;

///////////////////

Proto_IdleConnector * proto_create_idler(void);
