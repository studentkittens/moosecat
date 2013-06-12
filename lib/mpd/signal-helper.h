#ifndef MC_SIGNAL_HELPER_H
#define MC_SIGNAL_HELPER_H

#include <stdbool.h>
#include <mpd/client.h>

#include "signal.h"
#include "../compiler.h"

/* Only prototype needed */
struct mc_Client;

typedef enum mc_OpFinishedEnum {
    MC_OP_DB_UPDATED,
    MC_OP_QUEUE_UPDATED,
    MC_OP_SPL_UPDATED,
    MC_OP_SPL_LIST_UPDATED
} mc_OpFinishedEnum;

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
bool mc_shelper_report_error(
    struct mc_Client *self,
    struct mpd_connection *cconn);

/**
 * @brief Same as mc_shelper_report_error, but do no handle actual error (by disconnecting)
 *
 * @param self the client to operate on
 * @param cconn libmpdclient's mpd_connection
 */
bool mc_shelper_report_error_without_handling(
    struct mc_Client *self,
    struct mpd_connection *cconn);

/**
 * @brief Same as mc_shelper_report_error, but in a printf like fashion.
 *
 * Errors will be assumed to be non-fatal.
 *
 * @param self the client to operate on
 * @param format printf format
 * @param ... varargs
 */
void mc_shelper_report_error_printf(
    struct mc_Client *self,
    const char *format, ...);

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
mc_cc_printf(3,4) void mc_shelper_report_progress(
    struct mc_Client *self,
    bool print_newline,
    const char *format,
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
void mc_shelper_report_connectivity(
    struct mc_Client *self,
    const char *new_host,
    int new_port);

/**
 * @brief Disptatch the client-event signal
 *
 * @param self the client to operate on.
 * @param event the event to dispatch
 */
void mc_shelper_report_client_event(
    struct mc_Client *self,
    enum mpd_idle event);

/**
 * @brief Dispatch the op-finished signal
 *
 * @param self the client to operate on.
 * @param op a member of mc_OpFinishedEnum
 *           to indicate what kind of thing finished.
 */
void mc_shelper_report_operation_finished(
    struct mc_Client *self,
    mc_OpFinishedEnum op);

#endif /* end of include guard: MC_SIGNAL_HELPER_H */

