#ifndef DB_GUARD_H
#define DB_GUARD_H
/*
 * This header is not for you
 */
#include "db_private.h"

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
 * @brief Perform a song search on the database.
 *
 * Searches the songs table using the MATCH clause.
 * See http://www.sqlite.org/fts3.html#section_1_4 for the syntax
 *
 * @param self the store to operate on
 * @param match_clause search term
 * @param queue_only if true, only the queue contents is searched.
 * @param song_buffer a
 * @param buf_len the length of the buffer you passed. Also used as LIMIT.
 *
 * @return the number of actually found songs, or -1 on failure.
 */
int mc_store_search_out ( mc_StoreDB * self, const char * match_clause, bool queue_only, mpd_song ** song_buffer, int buf_len);

/**
 * @brief Tell routines to wait for db updates or return immediately.
 *
 * By default method return immediately.
 *
 * @param self the store to operate on
 * @param wait_for_db_finish if true, it waits for the lock.
 */
void mc_store_set_wait_mode (mc_StoreDB * self, bool wait_for_db_finish);

void mc_store_playlist_load (mc_StoreDB * self, const char * playlist_name);
int mc_store_playlist_select_to_stack (mc_StoreDB * self, mc_StoreStack * stack, const char * playlist_name, const char * match_clause);

int mc_store_dir_select_to_stack (mc_StoreDB * self, mc_StoreStack * stack, const char * directory, int depth);

#endif /* end of include guard: DB_GUARD_H */
