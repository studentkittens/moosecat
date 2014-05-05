#ifndef MC_SLEEP_GRAINED_H
#define MC_SLEEP_GRAINED_H

#include <glib.h>

/**
 * @brief Sleep as long as a condition is true.
 *
 * Sleeps for ms milliseconds, and checks the condition
 * check every interval_ms. If the condition evaluates
 * false sleeping is stopped.
 *
 * If interval_ms sleep for ms milliseconds, and check only
 * on entry.
 *
 * check may not be NULL.
 *
 * @param ms suspension time in milliseconds
 * @param interval_ms interval to check in ms
 * @param check a pointer to a condition
 */
void moose_sleep_grained(unsigned ms, unsigned interval_ms, volatile gboolean *check);

#endif /* end of include guard: MC_SLEEP_GRAINED_H */
