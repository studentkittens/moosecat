#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"

/**
 * @brief Skip to next song
 *
 * @param self the connection
 */
void mc_next (Proto_Connector * self);
void mc_volume (Proto_Connector * self, int volume);

#endif /* end of include guard: CLIENT_H */

