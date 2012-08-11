#ifndef DB_GUARD_H
#define DB_GUARD_H

/*
 * API to serialize songs into
 * a persistent DB Store
 */
#include "../mpd/protocol.h"

/**
 * @brief Struct to manage a SongStore
 */
typedef struct {
    const char * db_location; 
    GArray * memsongs;
} Store_DB;


/**
 * @brief Create a new Store.
 *
 * @param directory the path to the containing dir
 * @param dbname the name of the db file 
 * @param client a connection to operate on.
 *
 * If no db exist yet at this place (assuming it is a valid path),
 * then a new empy is created with a Query-Version of 0.
 * A call to store_update will therefore insert the whole queue to the list.
 *
 * If the db is already created, a call to store_update() will only
 * insert the latest differences (which might be enourmous too,
 * since mpd returns all changes behind a certain ID)
 *  
 * @return a new Store_DB struct, free with store_close()
 */
Store_DB * store_create(Proto_Connector * client, const char * directory, const char * dbname);

/**
 * @brief Update the db to the latest Query of the MPD Server.
 *
 * @param self the Store_DB to operate on.
 * @return the number of updated songs.
 */
int store_update(Store_DB * self);

/**
 * @brief Close the Store.
 *
 * This will write the data on disk, and close all connections,
 * held by libgda.
 *
 * @param self the store to close.
 */
void store_close(Store_DB * self);

/**
 * @brief Search songs in the store, accoring to a Store_Query.
 *
 * @param self Store to operate on.
 * @param qry
 *
 * The Store will look through the metadata, and return pointers
 * to the internally cached mpd_songs, encapsulated in a GArray.
 *
 * Here is a humble warning. 
 * DO NOT FUCKING FREE THE RETURNED MPD_SONGS!
 *
 * @return An GArray of mpd_songs
 */
GArray * store_search(Store_DB * self, Store_Query * qry);

#endif /* end of include guard: DB_GUARD_H */
