#ifndef MC_DB_STORED_PLAYLIST_HH
#define MC_DB_STORED_PLAYLIST_HH

#include "moose-store-store.h"

/**
 * @brief Init self->spl.*
 *
 * @param self the store to initialzie stored playlist support
 */
void moose_stprv_spl_init(MooseStore *self);

/**
 * @brief Destroy all ressources allocated by moose_stprv_spl_init()
 *
 * @param self the store you called moose_stprv_spl_init on
 */
void moose_stprv_spl_destroy(MooseStore *self);

/**
 * @brief Update stored playlists from MPD
 *
 * - This will only fetch metadata about the playlist. 
 *   use moose_stprv_spl_load to actually load them
 * - This will drop orphanded playlists.
 *
 * @param self the store to update.
 */
void moose_stprv_spl_update(MooseStore *self);

/**
 * @brief Load a certain playlist. 
 *
 * This actually fetches the
 *  
 * @param store the store to load them to.
 * @param playlist a mpd_playlist struct 
 */
bool moose_stprv_spl_load(MooseStore *store, struct mpd_playlist *playlist);

/**
 * @brief Load a playlist by it's name.
 *
 * @param store the store to load them to
 * @param playlist_name  a playlist name
 */
bool moose_stprv_spl_load_by_playlist_name(MooseStore *store, const char *playlist_name);

/**
 * @brief Search in a Playlist
 *
 * @param store the store that knows about the Playlist
 * @param out_stack stack to write the results to
 * @param playlist_name name of the playlist
 * @param match_clause a match clause as understood by FTS
 *
 * @return Number of selected songs.
 */
int moose_stprv_spl_select_playlist(MooseStore *store, MoosePlaylist *out_stack, const char *playlist_name, const char *match_clause);

/**
 * @brief Select all loaded mpd_playlists object to a stack.
 *
 * @param store the store that knows about the playlists
 * @param stack the stack to write the mpd_playlist objects to
 *
 * @return Number of laoded playlists
 */
int moose_stprv_spl_get_loaded_playlists(MooseStore *store, MoosePlaylist *stack);

/**
 * @brief Return a list of a all known mpd_playlists structs
 *
 * @param store the store which knows about them
 * @param stack the stack where to write them to
 *
 * @return number of appended playlisyts
 */
int moose_stprv_spl_get_known_playlists(MooseStore *store, MoosePlaylist *stack);

#endif /* end of include guard: MC_DB_STORED_PLAYLIST_HH */

