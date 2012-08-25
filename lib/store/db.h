#ifndef DB_GUARD_H
#define DB_GUARD_H

/*
 * API to serialize songs into
 * a persistent DB mc_Store
 */
#include "query.h"
#include "../mpd/protocol.h"

#include <glib.h>

/**
 * @brief Opaque Struct to manage a Songmc_Store
 */
struct mc_StoreDB;

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
 * @return a new mc_StoreDB struct, free with mc_store_close()
 */
struct mc_StoreDB * mc_store_create (mc_Client * client, const char * directory, const char * dbname);

/**
 * @brief Update the db to the latest Query of the MPD Server.
 *
 * @param self the mc_StoreDB to operate on.
 * @return the number of updated songs.
 */
int mc_store_update (struct mc_StoreDB * self);

/**
 * @brief Close the mc_Store.
 *
 * This will write the data on disk, and close all connections,
 * held by libgda.
 *
 * @param self the store to close.
 */
void mc_store_close (struct mc_StoreDB * self);

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
 *
 * @return An GArray of mpd_songs
 */
GArray * mc_store_search (struct mc_StoreDB * self, struct mc_StoreQuery * qry);

#endif /* end of include guard: DB_GUARD_H */
