from binds cimport mc_misc_bug_report

include "song.pyx"
include "status.pyx"
include "statistics.pyx"

report = mc_misc_bug_report(NULL)
print(str(report).replace('\\n', '\n'))
