#ifndef MC_DB_OPERATIONS_H
#define MC_DB_OPERATIONS_H

/**
 * Enumeration of all operations understood by mc_store_job_execute_callback()
 *
 * A name and priority can be given to the enum in mc_JobNames and mc_JobPrios
 * in db.c (somewhere at the top)
 */
typedef enum {
    MC_OPER_UNDEFINED       = 0,       /* Actually, not used.                                  */
    MC_OPER_DESERIALIZE     = 1 << 0,  /* Load all songs from the database                     */
    MC_OPER_LISTALLINFO     = 1 << 1,  /* Get the database from mpd                            */
    MC_OPER_PLCHANGES       = 1 << 2,  /* Get the Queue from mpd                               */
    MC_OPER_DB_SEARCH       = 1 << 3,  /* Search Data in the Queue or whole DB                 */
    MC_OPER_DIR_SEARCH      = 1 << 4,  /* List a directory of the music directory              */
    MC_OPER_SPL_LOAD        = 1 << 5,  /* Load a stored playlist                               */
    MC_OPER_SPL_UPDATE      = 1 << 6,  /* Update loaded playlists                              */
    MC_OPER_SPL_LIST        = 1 << 7,  /* List the names of all loaded playlists               */
    MC_OPER_SPL_QUERY       = 1 << 8,  /* Query a stored playlist                              */
    MC_OPER_SPL_LIST_ALL    = 1 << 9,  /* List the mpd_playlist objects of all known playlists */
    MC_OPER_UPDATE_META     = 1 << 10, /* Update meta information about the table              */
    MC_OPER_WRITE_DATABASE  = 1 << 11, /* Write Database to disk, making a backup              */
    MC_OPER_FIND_SONG_BY_ID = 1 << 12, /* Find a song by it's SongID                           */
    MC_OPER_ENUM_MAX        = 1 << 13, /* Highest Value in this Enum                           */
} mc_StoreOperation;

/**
 * @brief List all Songs from the Database and write them to the Database.
 *
 * The function prototype is compatible to mc_JobManager.
 *
 * @param store The Store passed.
 * @param cancel A flag indicating the desired cancellation of a Job.
 */
void mc_store_oper_listallinfo(mc_Store *store, volatile bool *cancel);

/**
 * @brief List all changes of the Queue since the last time.
 *
 * The function prototype is compatible to mc_JobManager.
 *
 * @param store The Store passed.
 * @param cancel A flag indicating the desired cancellation of a Job.
 */
void mc_store_oper_plchanges(mc_Store *store, volatile bool *cancel);

#endif /* end of include guard: MC_DB_OPERATIONS_H */
