#ifndef MOOSE_MPD_CLIENT_PRIVATE_H
#define MOOSE_MPD_CLIENT_PRIVATE_H

#include "moose-mpd-client.h"

G_BEGIN_DECLS

/**
 * moose_client_check_error: skip:
 * @self: a #MooseClient
 * @conn: a connection to check for errors.
 *
 * This function will check if an error happened.
 * If yes, the error will be logged.
 * On fatal errors, disconnect will be called to prevent
 * more errors.
 *
 * Returns: True if an error happened.
 */
gboolean moose_client_check_error(MooseClient * self, struct mpd_connection *conn);

/**
 * moose_client_check_error_without_handling: skip:
 * @self: a #MooseClient
 * @conn: a connection to check for errors.
 *
 * This function will check if an error happened.
 * If yes, the error will be logged.
 * Fatal errors will not be further handled.
 *
 * Returns: True if an error happened.
 */
gboolean moose_client_check_error_without_handling(MooseClient * self, struct mpd_connection *conn);

/**
 * moose_base_connect: (skip)
 * @self: a #MooseClient.
 * @host: the hostname to connect to.
 * @port: The port  to connect to
 * @timeout: How many seconds to wait before cancelling a network op
 * @err: (out): (nullable): Out-param for the error-location.
 *
 * Returns: (transfer none): A readily connected mpd_connection.
 */
struct mpd_connection * moose_base_connect(MooseClient * self, const char * host, int port, float timeout, char ** err);

G_END_DECLS

#endif /* end of include guard: MOOSE_MPD_CLIENT_PRIVATE_H */

