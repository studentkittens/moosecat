#ifndef MOOSE_DEBUG_H
#define MOOSE_DEBUG_H

#include <glib.h>

/**
 * moose_debug_version:
 *
 * Returns: (transfer none): A human readable string, describing libmoosecat's version.
 */
const char *moose_debug_version(void);

/**
 * moose_debug_install_handler:
 *
 * Configures to print an backtrace on receiving any of the following signals:
 * SIGUSR1, SIGSEGV, SIGABRT, SIGFPE
 */
void moose_debug_install_handler(void);

/**
 * moose_debug_install_handler_full:
 * @signals: (element-type int): An array of signals to register.
 *
 * Configures all signals mentioned in signals to print a backtrace once
 * they are encountered.
 *
 * You might encounter an error like this:
 *
 *   ptrace: Operation not permitted
 *
 * If you get this, you can disable it via:
 *
 *   echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
 */
void moose_debug_install_handler_full(GArray *signals);

#endif /* end of include guard: MOOSE_DEBUG_H */
