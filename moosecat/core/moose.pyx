cimport binds as c

# Utils are needed before everything else
include "util.pyx"

# Wrapper around libmpdclient
include "song.pyx"
include "status.pyx"
include "statistics.pyx"

# Actual libmoosecat functionality
include "client.pyx"
include "store.pyx"

# Try to register SIGSEGV catching.
c.mc_misc_register_posix_signal(NULL)

