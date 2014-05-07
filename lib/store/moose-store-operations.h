#ifndef MOOSE_DB_OPERATIONS_H
#define MOOSE_DB_OPERATIONS_H

/**
 * Enumeration of all operations understood by moose_store_job_execute_callback()
 *
 * A name and priority can be given to the enum in MooseJobNames and MooseJobPrios
 * in db.c (somewhere at the top)
 */
typedef enum {
    MOOSE_OPER_UNDEFINED       = 0,       /* Actually, not used.                                  */
    MOOSE_OPER_DESERIALIZE     = 1 << 0,  /* Load all songs from the database                     */
    MOOSE_OPER_LISTALLINFO     = 1 << 1,  /* Get the database from mpd                            */
    MOOSE_OPER_PLCHANGES       = 1 << 2,  /* Get the Queue from mpd                               */
    MOOSE_OPER_DB_SEARCH       = 1 << 3,  /* Search Data in the Queue or whole DB                 */
    MOOSE_OPER_DIR_SEARCH      = 1 << 4,  /* List a directory of the music directory              */
    MOOSE_OPER_SPL_LOAD        = 1 << 5,  /* Load a stored playlist                               */
    MOOSE_OPER_SPL_UPDATE      = 1 << 6,  /* Update loaded playlists                              */
    MOOSE_OPER_SPL_LIST        = 1 << 7,  /* List the names of all loaded playlists               */
    MOOSE_OPER_SPL_QUERY       = 1 << 8,  /* Query a stored playlist                              */
    MOOSE_OPER_SPL_LIST_ALL    = 1 << 9,  /* List the mpd_playlist objects of all known playlists */
    MOOSE_OPER_UPDATE_META     = 1 << 10, /* Update meta information about the table              */
    MOOSE_OPER_WRITE_DATABASE  = 1 << 11, /* Write Database to disk, making a backup              */
    MOOSE_OPER_FIND_SONG_BY_ID = 1 << 12, /* Find a song by it's SongID                           */
    MOOSE_OPER_ENUM_MAX        = 1 << 13, /* Highest Value in this Enum                           */
} MooseStoreOperation;

/**
 * @brief List all Songs from the Database and write them to the Database.
 *
 * The function prototype is compatible to MooseJobManager.
 *
 * @param store The Store passed.
 * @param cancel A flag indicating the desired cancellation of a Job.
 */
void moose_store_oper_listallinfo(MooseStore *store, volatile bool *cancel);

/**
 * @brief List all changes of the Queue since the last time.
 *
 * The function prototype is compatible to MooseJobManager.
 *
 * @param store The Store passed.
 * @param cancel A flag indicating the desired cancellation of a Job.
 */
void moose_store_oper_plchanges(MooseStore *store, volatile bool *cancel);

#endif /* end of include guard: MOOSE_DB_OPERATIONS_H */
