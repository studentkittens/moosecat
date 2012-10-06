#ifndef MC_DB_STORED_PLAYLIST_HH
#define MC_DB_STORED_PLAYLIST_HH

#include "db_private.h"

void mc_stprv_spl_init (mc_StoreDB * self);
void mc_stprv_spl_destroy (mc_StoreDB * self);

void mc_stprv_spl_update (mc_StoreDB * self);
void mc_stprv_spl_load (mc_StoreDB * store, struct mpd_playlist * playlist);
void mc_stprv_spl_load_by_playlist_name (mc_StoreDB * store, const char * playlist_name);

#endif /* end of include guard: MC_DB_STORED_PLAYLIST_HH */

