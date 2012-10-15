#ifndef DB_GUARD_H
#define DB_GUARD_H

#include "db-store.h"

/*
 * API to serialize songs into
 * a persistent DB mc_Store
 */

/**
 * @brief Create a new mc_Store.
 *
 * @param directory the path to the containing dir
 * @param client a connection to operate on.
 *
 * If no db exist yet at this place (assuming it is a valid path),
 * then a new empy is created with a Query-Version of 0.
 * A call to mc_store_update will therefore insert the whole queue to the list.
 *
 * If the db is already created, a call to mc_store_update() will only
 * insert the latest differences (which might be enourmous too,
 * since mpd returns all changes behind a certain ID)
 *
 * The name of the db will me moosecat_$host:$port
 *
 * @return a new mc_StoreDB , free with mc_store_close()
 */
mc_StoreDB * mc_store_create (mc_Client * client, mc_StoreSettings * settings);

/**
 * @brief Close the mc_Store.
 *
 * This will write the data on disk, and close all connections,
 * held by libgda.
 *
 * @param self the store to close.
 */
void mc_store_close ( mc_StoreDB * self);

/**
 * @brief Tell routines to wait for db updates or return immediately.
 *
 * By default method return immediately.
 *
 * @param self the store to operate on
 * @param wait_for_db_finish if true, it waits for the lock.
 */
void mc_store_set_wait_mode (mc_StoreDB * self, bool wait_for_db_finish);

/**
 * @brief Returns a mpd_song at some index
 *
 * This is effectively only useful on mc_store_dir_select_to_stack
 * (which returns a list of files, prefixed with IDs or -1 in case of a directory)
 *
 * @param self the store to operate on.
 * @param idx the index (0 - mc_store_total_songs())
 *
 * @return an mpd_song. DO NOT FREE!
 */
struct mpd_song * mc_store_song_at (mc_StoreDB * self, int idx);

/**
 * @brief Returns the total number of songs stored in the database.
 *
 * This is basically the same as 'select count(*) from songs;' just faster.
 *
 * @param self the store to operate on.
 *
 * @return a number between 0 and fucktuple.
 */
int mc_store_total_songs (mc_StoreDB * self);

///// PLAYLIST HANDLING ////

/**
 * @brief Load the playlist from the MPD server and store it in the database.
 *
 * Note: For every song in a playlist only an ID is stored. Therefore, this is quite efficient.
 *
 * @param self the store to operate on.
 * @param playlist_name a playlist name, as displayed by MPD.
 */
void mc_store_playlist_load (mc_StoreDB * self, const char * playlist_name);

/**
 * @brief Queries the contents of a playlist in a similar fashion as mc_store_search_out does.
 *
 * On succes, stack is filled with mpd_songs up to $return_value songs.
 *
 * @param self the store to operate on.
 * @param stack a stack to fill.
 * @param playlist_name a playlist name, as displayed by MPD.
 * @param match_clause an fts match clause. Empty ("") or NULL returns *all* songs in the playlist.
 *
 * @return number of found songs, or negative number on failure.
 */
int mc_store_playlist_select_to_stack (mc_StoreDB * self, mc_Stack * stack, const char * playlist_name, const char * match_clause);

/**
 * @brief List a directory in MPD's database.
 *
 * @param self the store to operate on.
 * @param stack a stack to fill.
 * @param directory the directory to list. (NULL == '/')
 * @param depth at wath depth to search (0 == '/', -1 to disable.)
 *
 * @return number of selected songs.
 */
int mc_store_dir_select_to_stack (mc_StoreDB * self, mc_Stack * stack, const char * directory, int depth);

/**
 * @brief return a stack of the loaded playlists. Not the actually ones there.
 *
 * @param self the store to operate on.
 * @param stack a stack of mpd_playlists. DO NOT FREE CONTENT!
 *
 * @return number of loaded playlists.
 */
int mc_store_playlist_get_all_loaded (mc_StoreDB * self, mc_Stack * stack);

/**
 * @brief 
 *
 * @param self
 * @param match_clause
 * @param queue_only
 * @param stack
 *
 * @return 
 */
int mc_store_search_to_stack (mc_StoreDB * self, const char * match_clause, bool queue_only, mc_Stack * stack, int limit_len);

#endif /* end of include guard: DB_GUARD_H */
