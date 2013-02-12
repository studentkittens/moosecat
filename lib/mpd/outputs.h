#ifndef MC_OUTPUTS_HH
#define MC_OUTPUTS_HH

#include "protocol.h"

/**
 * @brief Initialize outputs support
 *
 * @param self Client to initialize outputs support on.
 */
void mc_proto_outputs_init(mc_Client *self);

/**
 * @brief Get a list of outputs from MPD
 *
 * @param self the client to operate on
 * @param event an idle event. Will do only stmth. with MPD_IDLE_OUTPUT
 * @param ununsed pass NULL
 */
void mc_proto_outputs_update(mc_Client *self, enum mpd_idle event, void *ununsed);

/**
 * @brief Convert a name of an output (e.g. "AlsaOutput") to MPD's output id
 *
 * This is used internally, normal API User usually don't get in contact with
 * the Output ID.
 *
 * @param self the client to operate on
 * @param output_name a user defined string of the output (like in mpd.conf)
 *
 * @return the id or -1 on resolve-error
 */
int mc_proto_outputs_name_to_id(mc_Client *self, const char *output_name);


/**
 * @brief Check if an output is enabled.
 *
 * @param self the client to operate on
 * @param output_name a user defined string of the output (like in mpd.conf)
 *
 * @return -1 on "no such name", 0 if disabled, 1 if enabled
 */
int mc_proto_outputs_is_enabled(mc_Client *self, const char *output_name);

/**
 * @brief Free the list of outputs
 *
 * @param self the client to operate on
 */
void mc_proto_outputs_free(mc_Client *self);

#endif /* end of include guard: MC_OUTPUTS_HH */

