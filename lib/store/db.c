/*
 * API to serialize songs into
 * a persistent DB Store
 */
#include <mpd/client.h>

/**
 * @brief A Handler for a Store.
 */
typedef struct {
    const char * db_location; 
    
    void (* do_store_song)(mpd_song *);
} Store_DB;

/**
 * @brief 
 *
 * @param self
 */
void store_free(Store_DB * self);

/**
 * @brief Store a single song
 *
 * @param song libmpdclient's song structure
 */
void store_song(mpd_song * song);
