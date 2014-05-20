#ifndef COMMON_H_GUARD
#define COMMON_H_GUARD

#include <glib.h>
#include <mpd/client.h>
#include <mpd/async.h>

#include "../moose-mpd-client.h"

typedef enum mpd_async_event mpd_async_event;
typedef struct mpd_connection mpd_connection;

/**
 * @brief Helper to create a mpd_connection with built-in error checks
 *
 * @param host a hostname or IP
 * @param port a port (6600)
 * @param timeout in seconds
 * @param err location to place a error description in. May be NULL.
 *
 * @return a newly allocated mpd_connection or NULL on error
 */
mpd_connection * mpd_connect(MooseClient * self, const char * host, int port, float timeout, char * * err);

#endif /* end of include guard: COMMON_H_GUARD */
