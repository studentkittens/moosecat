#ifndef MC_SIGNAL_HELPER_H
#define MC_SIGNAL_HELPER_H

/* Only prototype needed */
struct mc_Client;

///////////////////////////////

/**
 * @brief Disptatch an client error
 *
 * Checks the connection for error, checks
 * if the error is fatal (if any), and also handles
 * it by calling mpd_connection_clear_error,
 * and, if it's fatal, mc_proto_disconnect.
 *
 * The callback is called **after** cleaning up,
 * which allows the callee to reconnect if it desires too.
 *
 * Will do nothing when no error happened.
 *
 * @param self the client to operate on
 * @param cconn libmpdclient's mpd_connection
 */
void mc_shelper_report_error (
    struct mc_Client * self,
    mpd_connection * cconn);

/**
 * @brief Dispatch the progress signal
 *
 * This is a helper function that works in printf()
 * like fashion, by writing all varargs into format,
 * and dispatch the string to the progress signal.
 *
 * @param self the client to operate on.
 * @param format a printf format
 * @param ... varargs as with printf
 */
void mc_shelper_report_progress (
    struct mc_Client * self,
    const char * format,
    ...);

/**
 * @brief Dispatch the connectivity signal
 *
 * Note: signal is disptatched always,
 *       caller should assure to not report
 *       connectivity twice.
 *
 * @param self the client to operate on.
 * @param new_host the new host passed to connect()
 * @param new_port the new port passed to connect()
 */
void mc_shelper_report_connectivity (
    struct mc_Client * self,
    const char * new_host,
    int new_port);

/**
 * @brief Disptatch the client-event signal
 *
 * @param self the client to operate on.
 * @param event the event to dispatch
 */
void mc_shelper_report_client_event (
    struct mc_Client * self,
    enum mpd_idle event);

#endif /* end of include guard: MC_SIGNAL_HELPER_H */
