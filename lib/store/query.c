#include "query.h"
#include <glib.h>

struct mc_StoreQuery {
    GArray * where_clausels;
    GArray * column_clausels;
};

struct mc_StoreQuery * mc_store_query_create (const char * search_string) {
    (void) search_string;
    return NULL;
}
