#include "db.h"

struct Store_DB
{
    const char * db_location;
    GArray * memsongs;
};

///////////////

struct Store_DB * store_create (Proto_Connector * client, const char * directory, const char * dbname)
{
    // Create the structure and the file
    //
    (void) client;
    (void) directory;
    (void) dbname;
    return NULL;
}

///////////////

int store_update (struct Store_DB * self)
{
    (void) self;
    return 0;
}

///////////////

void store_close (struct Store_DB * self)
{
    // Write to disk
    (void) self;
}

///////////////

GArray * store_search (struct Store_DB * self, Store_Query * qry)
{
    (void) self;
    (void) qry;
    return NULL;
}
