#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"

/**
 * client.h implements the actual commands, which are
 * sendable to the server.
 *
 * This excludes database commands, which are only accessible
 * via the store/ module.
 */

bool mc_client_output_switch(mc_Client *self, const char *output_name, bool mode);
bool mc_client_password(mc_Client *self, const char *pwd);
void mc_client_consume(mc_Client *self, bool mode);
void mc_client_crossfade(mc_Client *self, bool mode);
void mc_client_database_rescan(mc_Client *self, const char *path);
void mc_client_database_update(mc_Client *self, const char *path);
void mc_client_mixramdb(mc_Client *self, int decibel);
void mc_client_mixramdelay(mc_Client *self, int seconds);
void mc_client_next(mc_Client *self);
void mc_client_pause(mc_Client *self);
void mc_client_play_id(mc_Client *self, unsigned id);
void mc_client_playlist_add(mc_Client *self, const char *name, const char *file);
void mc_client_playlist_clear(mc_Client *self, const char *name);
void mc_client_playlist_delete(mc_Client *self, const char *name, unsigned pos);
void mc_client_playlist_load(mc_Client *self, const char *playlist);
void mc_client_playlist_move(mc_Client *self, const char *name, unsigned old_pos, unsigned new_pos);
void mc_client_playlist_rename(mc_Client *self, const char *old_name, const char *new_name);
void mc_client_playlist_rm(mc_Client *self, const char *playlist_name);
void mc_client_playlist_save(mc_Client *self, const char *as_name);
void mc_client_play(mc_Client *self);
void mc_client_previous(mc_Client *self);
void mc_client_prio_id(mc_Client *self, unsigned prio, unsigned id);
void mc_client_prio(mc_Client *self, unsigned prio, unsigned position);
void mc_client_prio_range(mc_Client *self, unsigned prio, unsigned start_pos, unsigned end_pos);
void mc_client_queue_add(mc_Client *self, const char *uri);
void mc_client_queue_clear(mc_Client *self);
void mc_client_queue_delete_id(mc_Client *self, int id);
void mc_client_queue_delete(mc_Client *self, int pos);
void mc_client_queue_delete_range(mc_Client *self, int start, int end);
void mc_client_queue_move(mc_Client *self, unsigned old_pos, unsigned new_pos);
void mc_client_queue_move_range(mc_Client *self, unsigned start_pos, unsigned end_pos, unsigned new_pos);
void mc_client_queue_shuffle(mc_Client *self);
void mc_client_queue_swap_id(mc_Client *self, int id_a, int id_b);
void mc_client_queue_swap(mc_Client *self, int pos_a, int pos_b);
void mc_client_random(mc_Client *self, bool mode);
void mc_client_repeat(mc_Client *self, bool mode);
void mc_client_replay_gain_mode(mc_Client *self, const char *replay_gain_mode);
void mc_client_seekcur(mc_Client *self, int seconds);
void mc_client_seekid(mc_Client *self, int id, int seconds);
void mc_client_seek(mc_Client *self, int pos, int seconds);
void mc_client_setvol(mc_Client *self, int volume);
void mc_client_single(mc_Client *self, bool mode);
void mc_client_stop(mc_Client *self);

#endif /* end of include guard: CLIENT_H */

