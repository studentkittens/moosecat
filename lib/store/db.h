#ifndef DB_GUARD_H
#define DB_GUARD_H

/*
 * API to serialize songs into
 * a persistent DB mc_Store
 */
#include "db_private.h"

#include <glib.h>

/**
 * @brief Create a new mc_Store.
 *
 * @param directory the path to the containing dir
 * @param dbname the name of the db file
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
 * @return a new mc_StoreDB , free with mc_store_close()
 */
 mc_StoreDB * mc_store_create (mc_Client * client, const char * directory, const char * dbname);

/**
 * @brief Update the db to the latest Query of the MPD Server.
 *
 * @param self the mc_StoreDB to operate on.
 * @return the number of updated songs.
 */
int mc_store_update ( mc_StoreDB * self);

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
 * @brief Search songs in the store, accoring to a mc_StoreQuery.
 *
 * @param self mc_Store to operate on.
 * @param qry
 *
 * The mc_Store will look through the metadata, and return pointers
 * to the internally cached mpd_songs, encapsulated in a GArray.
 *
 * Here is a humble warning.
 * DO NOT FUCKING FREE THE RETURNED MPD_SONGS!
 */
mpd_song ** mc_store_search ( mc_StoreDB * self, struct mc_StoreQuery * qry);

int mc_store_search_out ( mc_StoreDB * self, const char * match_clause, mpd_song ** song_buffer, int buf_len);

/**
 * @brief try to optimize the internal database ure.
 *
 * @param self the store to operate on.
 */
void mc_store_optimize ( mc_StoreDB * self);

#endif /* end of include guard: DB_GUARD_H */
