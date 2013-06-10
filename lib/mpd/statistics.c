#include "statistics.h"
#include "update.h"


#define STATISTIC_FUNC(NAME, RTYPE)                    \
    RTYPE mc_status_get_  ## NAME (mc_Client *self)    \
    {                                                  \
        RTYPE rc = 0;                                  \
        if(self && self->stats) {                      \
            mc_lock_stats(self);                       \
            rc = mpd_stats_get_ ## NAME (self->stats); \
            mc_unlock_stats(self);                     \
        }                                              \
                                                       \
        return rc;                                     \
    }                                                  \

STATISTIC_FUNC(number_of_artists, unsigned)
STATISTIC_FUNC(number_of_albums,  unsigned)
STATISTIC_FUNC(number_of_songs,   unsigned)
STATISTIC_FUNC(uptime,            unsigned long)
STATISTIC_FUNC(db_update_time,    unsigned long)
STATISTIC_FUNC(play_time,         unsigned long)
STATISTIC_FUNC(db_play_time,      unsigned long)
