#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"

/**
 * @brief Skip to next song
 *
 * Events: player
 *
 * @param self the connection
 */
void mc_client_next (mc_Client * self);

/**
 * @brief Change the volume (0 - 100)
 *
 * Events: mixer
 *
 * @param self the connection
 * @param volume integer, range 0-100
 */
void mc_client_volume (mc_Client * self, int volume);

/**
 * @brief Send a password to gain more privileges.
 *
 * Events: none
 *
 * @param self the (connected) client to operate on.
 * @param pwd a plain text password (mpd does not offer encryption sadly)
 *
 * @return true if auth was a success.
 */
bool mc_client_password (mc_Client * self, const char * pwd);

#endif /* end of include guard: CLIENT_H */

