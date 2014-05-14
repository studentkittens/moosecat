#ifndef MOOSE_SIGNAL_HELPER_H
#define MOOSE_SIGNAL_HELPER_H

#include <stdbool.h>
#include <mpd/client.h>

#include "moose-mpd-signal.h"

/* Only prototype needed */
struct MooseClient;

typedef enum MooseOpFinishedEnum {
    MOOSE_OP_DB_UPDATED,
    MOOSE_OP_QUEUE_UPDATED,
    MOOSE_OP_SPL_UPDATED,
    MOOSE_OP_SPL_LIST_UPDATED
} MooseOpFinishedEnum;

///////////////////////////////

/**
 * @brief Disptatch an client error
 *
 * Checks the connection for error, checks
 * if the error is fatal (if any), and also handles
 * it by calling mpd_connection_clear_error,
 * and, if it's fatal, moose_disconnect.
 *
 * The callback is called **after** cleaning up,
 * which allows the callee to reconnect if it desires too.
 *
 * Will do nothing when no error happened.
 *
 * @param self the client to operate on
 * @param cconn libmpdclient's mpd_connection
 */
bool moose_shelper_report_error(
    struct MooseClient * self,
    struct mpd_connection * cconn);

/**
 * @brief Same as moose_shelper_report_error, but do no handle actual error (by disconnecting)
 *
 * @param self the client to operate on
 * @param cconn libmpdclient's mpd_connection
 */
bool moose_shelper_report_error_without_handling(
    struct MooseClient * self,
    struct mpd_connection * cconn);

/**
 * @brief Same as moose_shelper_report_error, but in a printf like fashion.
 *
 * Errors will be assumed to be non-fatal.
 *
 * @param self the client to operate on
 * @param format printf format
 * @param ... varargs
 */
void moose_shelper_report_error_printf(
    struct MooseClient * self,
    const char * format, ...);

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
void moose_shelper_report_progress(
    struct MooseClient * self,
    bool print_newline,
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
 * @param new_timeout the new timeout passed to connect()
 */
void moose_shelper_report_connectivity(
    struct MooseClient * self,
    const char * new_host,
    int new_port,
    float new_timeout);

/**
 * @brief Dispatch the op-finished signal
 *
 * @param self the client to operate on.
 * @param op a member of MooseOpFinishedEnum
 *           to indicate what kind of thing finished.
 */
void moose_shelper_report_operation_finished(
    struct MooseClient * self,
    MooseOpFinishedEnum op);

#endif /* end of include guard: MOOSE_SIGNAL_HELPER_H */
