#ifndef MOOSE_DEBUG_H
#define MOOSE_DEBUG_H

#include <glib.h>

/**
 * moose_debug_version:
 *
 * Returns: (transfer none): A human readable string, describing libmoosecat's version.
 */
const char * moose_debug_version(void);

/**
 * moose_debug_install_handler:
 *
 * Configures to print an backtrace on receiving any of the following signals:
 * SIGUSR1, SIGSEGV, SIGABRT, SIGFPE
 */
void moose_debug_install_handler(void);

/**
 * moose_debug_install_handler_full:
 * @signals: An array of signals to register.
 *
 */
void moose_debug_install_handler_full(GArray *signals);

#endif /* end of include guard: MOOSE_DEBUG_H */

