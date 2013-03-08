cimport binds as c

# Utils are needed before everything else
include "util.pyx"

# Wrapper around libmpdclient
include "song.pyx"
include "status.pyx"
include "statistics.pyx"
include "output.pyx"

# Actual libmoosecat functionality
include "playlist.pyx"
include "queue.pyx"
include "stored_playlist.pyx"
include "store.pyx"
include "client.pyx"
include "misc.pyx"


# Try to register SIGSEGV catching.
# c.mc_misc_register_posix_signal(NULL)
