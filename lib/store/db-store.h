#ifndef MC_DB_STORE_HH
#define MC_DB_STORE_HH

#include <sqlite3.h>
#include <glib.h>

#include "stack.h"
#include "db-settings.h"

#include "../mpd/protocol.h"


typedef struct mc_StoreDB {
    /* directory db lies in */
    char * db_directory;

    /* songstack - a place for mpd_songs to live in */
    mc_Stack * stack;

    /* handle to sqlite */
    sqlite3 * handle;

    /* prepared statements */
    sqlite3_stmt ** sql_prep_stmts;
    sqlite3_stmt ** sql_dir_stmts;

    /* client associated with this store */
    mc_Client * client;

    /* if true, we need to write the db to disk on termination */
    bool write_to_disk;

    /* true when the full queue is needed, false to only list differences */
    bool need_full_queue;

    /* this flag is only set to true on connection change (or on startup) */
    bool need_full_db;

    /* locked whenever a background thread updates the db */
    GRecMutex db_update_lock;

    /* GMutex provides no is_locked function? */
    bool db_is_locked;

    /* songs that are received on network,
     * are pushed to this queue,
     * and are processed by SQL */
    GAsyncQueue * sqltonet_queue;

    /* If db is locked, should we wait for it to finish? */
    bool wait_for_db_finish;

    /* true, when the current listallinfo transaction should be stopped. */
    bool stop_listallinfo;

    /* Various user defined settings go here */
    mc_StoreSettings * settings;

    /* Support for stored playlists */
    struct {
        /* A stack of mpd_playlists (all of them) */
        mc_Stack * stack;

        sqlite3_stmt * select_tables_stmt;
    } spl;

    /* Things that only are important for creating */
    struct {
        bool double_unlock;
        GMutex mutex;
    } create;

} mc_StoreDB;

#endif /* end of include guard: MC_DB_STORE_HH */
