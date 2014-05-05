#ifndef MC_BUG_REPORT_H
#define MC_BUG_REPORT_H

#include "../mpd/moose-mpd-protocol.h"

/**
 * @brief Returns a message containing information about the library/system.
 *
 * Let's just hope this never gets executed.
 *
 * @param client the client (reads some information from it)
 *
 * @return a newly allocated string
 */
char *moose_misc_bug_report(MooseClient *client);

#endif /* end of include guard: MC_BUG_REPORT_H */

