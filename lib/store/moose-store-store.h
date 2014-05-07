#ifndef MOOSE_DB_STORE_HH
#define MOOSE_DB_STORE_HH

#include <sqlite3.h>
#include <glib.h>

#include "moose-store-playlist.h"
#include "moose-store-settings.h"

#include "../mpd/moose-mpd-protocol.h"

#include "../util/moose-util-job-manager.h"


/* Prototype MooseStoreCompletion to prevent circle-include. */
struct MooseStoreCompletion; 


typedef struct MooseStore {
    /* directory db lies in */
    char *db_directory;

    /* songstack - a place for mpd_songs to live in */
    MoosePlaylist *stack;

    /* handle to sqlite */
    sqlite3 *handle;

    /* prepared statements for normal operations */
    sqlite3_stmt **sql_prep_stmts;
    
    /* prepared statements for directory operations */
    sqlite3_stmt **sql_dir_stmts;

    /* client associated with this store */
    MooseClient *client;

    /* Various user defined settings go here */
    MooseStoreSettings *settings;

    /* Write database to disk?
     * on changes this gets set to True 
     * */
    bool write_to_disk;

    /* Support for stored playlists */
    struct {
        /* A stack of mpd_playlists (all of them, loaded or not) */
        MoosePlaylist *stack;

        /* Sql statements for stored playlists */
        sqlite3_stmt *select_tables_stmt;
    } spl;

    /* If this flag is set listallinfo will retrieve all songs,
     * and skipping the check if is actually necessary.
     * */
    bool force_update_listallinfo;

    /* If this flag is set plchanges will take last_version as 0,
     * even if, judging from the queue version, no full update is necessary.
     * */
    bool force_update_plchanges;

    /* Job manager used to process database tasks in the background */
    struct MooseJobManager *jm;

    /* Locked when setting an attribute, or reading from one 
     * Attributes are:
     *    - stack
     *    - spl.stack
     *
     * db-dirs.c append copied data onto the stack. 
     *  
     * */
    GMutex attr_set_mtx;

    /*
     * The Port of the Server this Store mirrors
     */
    int mirrored_port;

    /*
     * The Host this Store mirrors
     */
    char * mirrored_host;

    GMutex mirrored_mtx;

    struct MooseStoreCompletion* completion;

} MooseStore;

#endif /* end of include guard: MOOSE_DB_STORE_HH */
