#ifndef MC_current_song_H
#define MC_current_song_H

#include "protocol.h"

const char * mc_current_song_get_uri(mc_Client *self);
unsigned mc_current_song_get_duration(mc_Client *self);
unsigned mc_current_song_get_start(mc_Client *self);
unsigned mc_current_song_get_end(mc_Client *self);
time_t mc_current_song_get_last_modified(mc_Client *self);
unsigned mc_current_song_get_pos(mc_Client *self);
unsigned mc_current_song_get_id(mc_Client *self);

const char * mc_current_song_get_tag(
        mc_Client *self,
        enum mpd_tag_type type,
        unsigned tag_pos
);


/*
void mc_current_song_set_pos(mc_Client *self);
*/

#endif /* end of include guard: MC_current_song_H */

