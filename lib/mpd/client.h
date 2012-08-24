#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"

/**
 * @brief Skip to next song
 *
 * @param self the connection
 */
void mc_client_next (mc_Client * self);

void mc_client_volume (mc_Client * self, int volume);

#endif /* end of include guard: CLIENT_H */

