#include "../protocol.h"

#ifndef CMND_CORE_H
#define CMND_CORE_H


/**
 * @brief Connector using two connections, one for sending, one for events.
 *
 * Sending a command:
 * - send it in the cmnd_con, read the response
 * - wait for incoming events asynchronously on idle_con
 */


/**
 * @brief Create an Proto_Connector
 *
 * See Proto_IdleConnector for more description.
 *
 * @return a newly allocated connector
 */
Proto_Connector * proto_create_cmnder (void);

#endif /* end of include guard: CMND_CORE_H */