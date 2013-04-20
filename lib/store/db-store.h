#ifndef MC_DB_STORE_HH
#define MC_DB_STORE_HH

#include <sqlite3.h>
#include <glib.h>

#include "stack.h"
#include "db-settings.h"

#include "../mpd/protocol.h"

typedef struct mc_StoreDB {
    /* directory db lies in */
    char *db_directory;

    /* songstack - a place for mpd_songs to live in */
    mc_Stack *stack;

    /* handle to sqlite */
    sqlite3 *handle;

    /* prepared statements for normal operations */
    sqlite3_stmt **sql_prep_stmts;
    
    /* prepared statements for directory operations */
    sqlite3_stmt **sql_dir_stmts;

    /* client associated with this store */
    mc_Client *client;

    /* Various user defined settings go here */
    mc_StoreSettings *settings;

    /* Support for stored playlists */
    struct {
        /* A stack of mpd_playlists (all of them) */
        mc_Stack *stack;

        /* Sql statements for stored playlists */
        sqlite3_stmt *select_tables_stmt;
    } spl;

} mc_StoreDB;

#endif /* end of include guard: MC_DB_STORE_HH */
