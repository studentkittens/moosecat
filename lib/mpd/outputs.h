#ifndef MC_OUTPUTS_HH
#define MC_OUTPUTS_HH

#include "protocol.h"


typedef struct mc_OutputsData {
    mc_Client *client;
    GHashTable *outputs;
    GMutex output_lock;
} mc_OutputsData;

/**
 * @brief Initialize outputs support
 *
 * @param data Client to initialize outputs support on.
 */
mc_OutputsData *mc_priv_outputs_new(mc_Client *self);

/**
 * @brief Get a list of outputs from MPD
 *
 * @param data the client to operate on
 * @param event an idle event. Will do only stmth. with MPD_IDLE_OUTPUT
 */
void mc_priv_outputs_update(mc_OutputsData *data, enum mpd_idle event);

/**
 * @brief Convert a name of an output (e.g. "AlsaOutput") to MPD's output id
 *
 * This is used internally, normal API User usually don't get in contact with
 * the Output ID.
 *
 * @param data the client to operate on
 * @param output_name a user defined string of the output (like in mpd.conf)
 *
 * @return the id or -1 on resolve-error
 */
int mc_priv_outputs_name_to_id(mc_OutputsData *data, const char *output_name);


/**
 * @brief Check if an output is enabled.
 *
 * @param data the client to operate on
 * @param output_name a user defined string of the output (like in mpd.conf)
 *
 * @return -1 on "no such name", 0 if disabled, 1 if enabled
 */
bool mc_priv_outputs_get_state(mc_OutputsData *data, const char *output_name);

/**
 * @brief Get a list of known output names
 *
 * @param data the client to operate on 
 *
 * @return a NULL terminated list of names,
 *         free the list (not the contents) with free() when done
 */
const char ** mc_priv_outputs_get_names(mc_OutputsData *data);

/**
 * @brief Set the state of a Output (enabled or disabled)
 *
 * This can also be accomplished with:
 *
 *    mc_client_send("output_switch horst 1"); 
 *
 *  to enable the output horst. This is just a convinience command.
 *
 * @param data the client to operate on 
 * @param output_name The name of the output to alter
 * @param state the state, True means enabled. 
 *              If nothing would change no command is send.
 *
 * @return True if an output with this name could be found.
 */
bool mc_priv_outputs_set_state(mc_OutputsData *data, const char *output_name, bool state);

/**
 * @brief Free the list of outputs
 *
 * @param data the client to operate on
 */
void mc_priv_outputs_destroy(mc_OutputsData *data);

#endif /* end of include guard: MC_OUTPUTS_HH */

