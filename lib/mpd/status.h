#ifndef MC_STATUS_H
#define MC_STATUS_H

#include "protocol.h"

/* Wrapper around mpd_status.
 * Im not a fan of wrappers myself, but libmoosecat can't get you a pointer 
 * to a mpd_status sicne it may silenty delete it behind your back.
 */ 


int mc_status_get_volume(mc_Client *self);
bool mc_status_get_repeat(mc_Client *self);
bool mc_status_get_random(mc_Client *self);
bool mc_status_get_single(mc_Client *self);
bool mc_status_get_consume(mc_Client *self);

unsigned mc_status_get_queue_length(mc_Client *self);
unsigned mc_status_get_queue_version(mc_Client *self);

enum mpd_state mc_status_get_state(mc_Client *self);
unsigned mc_status_get_crossfade(mc_Client *self);
float mc_status_get_mixrampdb(mc_Client *self);
float mc_status_get_mixrampdelay(mc_Client *self);

int mc_status_get_song_pos(mc_Client *self);
int mc_status_get_song_id(mc_Client *self);
int mc_status_get_next_song_pos(mc_Client *self);
int mc_status_get_next_song_id(mc_Client *self);

unsigned mc_status_get_elapsed_time(mc_Client *self);
unsigned mc_status_get_elapsed_ms(mc_Client *self);
unsigned mc_status_get_total_time(mc_Client *self);
unsigned mc_status_get_kbit_rate(mc_Client *self);

unsigned mc_status_get_status_audio_get_sample_rate(mc_Client *self);
unsigned mc_status_get_status_audio_get_bits(mc_Client *self);
unsigned mc_status_get_status_audio_get_channels(mc_Client *self);

bool mc_status_get_is_updating(mc_Client *self);

const char *mc_status_get_replay_gain_status(mc_Client *self);


#endif /* end of include guard: MC_STATUS_H */
