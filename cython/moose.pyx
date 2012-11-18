from moose cimport mc_misc_bug_report

# libmpdclient Wrappers
include "song.pyx"
include "stats.pyx"
include "status.pyx"


print(str(mc_misc_bug_report(NULL)).replace('\\n', '\n'))
