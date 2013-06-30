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
 * @return a new mc_Store , free with mc_store_close()
 */
mc_Store *mc_store_create(mc_Client *client, mc_StoreSettings *settings);

/**
 * @brief Close the mc_Store.
 *
 * This will write the data on disk, and close all connections,
 * held by libgda.
 *
 * @param self the store to close.
 */
void mc_store_close(mc_Store *self);

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
struct mpd_song *mc_store_song_at(mc_Store *self, int idx);

/**
 * @brief Returns the total number of songs stored in the database.
 *
 * This is basically the same as 'select count(*) from songs;' just faster.
 *
 * @param self the store to operate on.
 *
 * @return a number between 0 and fucktuple.
 */
int mc_store_total_songs(mc_Store *self);

///// PLAYLIST HANDLING ////

/**
 * @brief Load the playlist from the MPD server and store it in the database.
 *
 * Note: For every song in a playlist only an ID is stored. Therefore, this is quite efficient.
 *
 * @param self the store to operate on.
 * @param playlist_name a playlist name, as displayed by MPD.
 *
 * @return a job id
 */
long mc_store_playlist_load(mc_Store *self, const char *playlist_name);

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
 * @return a job id
 */
long mc_store_playlist_select_to_stack(mc_Store *self, mc_Stack *stack, const char *playlist_name, const char *match_clause);

/**
 * @brief List a directory in MPD's database.
 *
 * @param self the store to operate on.
 * @param stack a stack to fill.
 * @param directory the directory to list. (NULL == '/')
 * @param depth at wath depth to search (0 == '/', -1 to disable.)
 *
 * @return a job id
 */
long mc_store_dir_select_to_stack(mc_Store *self, mc_Stack *stack, const char *directory, int depth);

/**
 * @brief return a stack of the loaded playlists. Not the actually ones there.
 *
 * @param self the store to operate on.
 * @param stack a stack of mpd_playlists. DO NOT FREE CONTENT!
 *
 * @return a job id
 */
long mc_store_playlist_get_all_loaded(mc_Store *self, mc_Stack *stack);

/**
 * @brief Get a list (i.e. mc_Stack) of available playlist names on server-side.
 *
 * This is updated internally, and will change when the user changes it.
 * You will be notified with a stored-playlist signal.
 *
 * IMPORTANT NOTE: Call mc_store_release() when done using the list.
 *
 * Do not modify the returned value.
 *
 * @param self the store to operate on.
 *
 * @return A mc_Stack. DO NOT MODIFY IT!
 */
const mc_Stack *mc_store_playlist_get_all_names(mc_Store *self);

/**
 * @brief search the Queue or the whole Database.
 *
 * @param self the client to operate on
 * @param match_clause a fts match clause (NULL will return *all* songs)
 * @param queue_only restrict search to queue only?
 * @param stack the stack to select the songs too
 * @param limit_len limit the length of the return. negative numbers dont limit.
 *
 * Direclty after calling this function you will have no results yet in the stack.
 * You should call mc_store_wait_for_job() to wait for it to be filled.
 *
 * Example:
 *
 *      mc_Stack * stack = mc_stack_create(50, NULL);
 *      int job_id = mc_store_search_to_stack(store, "artist:Akrea", true, stack, -1);
 *      // You can other things than waiting here.
 *      mc_store_wait_for_job(store, job_id);
 *      mc_store_lock(store);
 *      for(int i = 0; i < mc_stack_length(stack); ++i) {
 *          printf("%s\n", mpd_song_tag(mc_stack_at(i), MPD_TAG_TITLE, 0));
 *      }
 *      mc_store_unlock(store);
 *      
 *
 * @return a Job id
 */
long mc_store_search_to_stack(mc_Store *self, const char *match_clause, bool queue_only, mc_Stack *stack, int limit_len);

/**
 * @brief Wait for the store to finish it's current operation.
 *
 * This is useful to wait for it to finish before shutdown.
 *
 * @param self the store to operate on.
 */
void mc_store_wait(mc_Store *self);

/**
 * @brief Wait for a certain job to complete.
 *
 * This can be useful at times when you want to fire an operation to
 * the database, but do not want for it to complete and do other things in 
 * the meanwhile. After some time you can get the result with
 * mc_store_get_result()
 *
 * @param self the store to operate on
 * @param job_id a job id to wait on (acquired by the above functions)
 */
void mc_store_wait_for_job(mc_Store *self, int job_id);

/**
 * @brief Get a result from a job.
 *
 * Note: This function does NOT wait. To be sure that the result is there
 *       you should call mc_store_wait_for_job() previously.
 *
 * @param self the store to operate on.
 * @param job_id the job id obtained from one of the above functions.
 *
 * @return a mc_Stack containing the results you wanted.
 */
mc_Stack *mc_store_get_result(mc_Store *self, int job_id);


/**
 * @brief Shortcur for mc_store_wait_for_job and mc_store_get_result
 *
 * @param self the store to operate on
 * @param job_id job_id to wait on and get the result from
 *
 * @return same as mc_store_get_result
 */
mc_Stack *mc_store_gw(mc_Store *self, int job_id);


/**
 * @brief Tells store that you done using a ressource.
 *
 *
 * Bad things will happen if you don't release the ressources.
 * libmoosecat will likely deadlock.
 *
 * @param self the store you had the ressources from
 */
void mc_store_release(mc_Store *self);

#endif /* end of include guard: DB_GUARD_H */
