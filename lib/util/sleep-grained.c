#include "sleep-grained.h"

void mc_sleep_grained(unsigned ms, unsigned interval_ms, gboolean *check)
{
    g_assert(check);

    if (interval_ms == 0) {
        if (*check) g_usleep((ms) * 1000);
    } else {
        unsigned n = ms / interval_ms;

        for (unsigned i = 0; i < n && *check; ++i) {
            g_usleep(interval_ms * 1000);
        }

        if (*check) g_usleep((ms % interval_ms) * 1000);
    }
}
