#include "db.h"

struct mc_StoreDB
{
    const char * db_location;
    GArray * memsongs;
};

///////////////

struct mc_StoreDB * mc_store_create (mc_Client * client, const char * directory, const char * dbname)
{
    // Create the structure and the file
    //
    (void) client;
    (void) directory;
    (void) dbname;
    return NULL;
}

///////////////

int mc_store_update (struct mc_StoreDB * self)
{
    (void) self;
    return 0;
}

///////////////

void mc_store_close (struct mc_StoreDB * self)
{
    // Write to disk
    (void) self;
}

///////////////

GArray * mc_store_search (struct mc_StoreDB * self, struct mc_StoreQuery * qry)
{
    (void) self;
    (void) qry;
    return NULL;
}
