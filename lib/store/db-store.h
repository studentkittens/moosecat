#ifndef MC_DB_STORE_HH
#define MC_DB_STORE_HH

#include <sqlite3.h>
#include <glib.h>

#include "stack.h"
#include "db-settings.h"

#include "../mpd/protocol.h"

#include "../util/job-manager.h"

typedef struct mc_Store {
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

    /* Write database to disk?
     * on changes this gets set to True 
     * */
    bool write_to_disk;

    /* Support for stored playlists */
    struct {
        /* A stack of mpd_playlists (all of them) */
        mc_Stack *stack;

        /* Sql statements for stored playlists */
        sqlite3_stmt *select_tables_stmt;
    } spl;


    bool force_update_listallinfo;
    bool force_update_plchanges;

    /* Job manager used to process database tasks in the background */
    struct mc_JobManager *jm;

} mc_Store;

#endif /* end of include guard: MC_DB_STORE_HH */
