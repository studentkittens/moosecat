#ifndef MC_STATISTICS_H
#define MC_STATISTICS_H

#include "protocol.h"

unsigned mc_stats_get_number_of_artists(mc_Client *self);
unsigned mc_stats_get_number_of_albums(mc_Client *self);
unsigned mc_stats_get_number_of_songs(mc_Client *self);
unsigned long mc_stats_get_uptime(mc_Client *self);
unsigned long mc_stats_get_db_update_time(mc_Client *self);
unsigned long mc_stats_get_play_time(mc_Client *self);
unsigned long mc_stats_get_db_play_time(mc_Client *self);


#endif /* end of include guard: MC_STATISTICS_H */

