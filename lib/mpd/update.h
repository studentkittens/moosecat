#include <mpd/client.h>

struct mc_Client;

/**
 * @brief Update status/stats/song.
 * 
 * The function looks like that, in order to be registered as event callback.
 *
 * @param events the happenene event. Use INT_MAX to update all events.
 * @param user_data not used. pass NULL.
 */
void mc_proto_update_context_info_cb (struct mc_Client * self, enum mpd_idle events, void * user_data);
