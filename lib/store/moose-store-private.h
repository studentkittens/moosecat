#ifndef MOOSE_DB_PRIVATE_H
#define MOOSE_DB_PRIVATE_H

#include "moose-store.h"

/*
 * DB Layout version.
 * Older tables will not be loaded.
 * */
#define MOOSE_DB_SCHEMA_VERSION 2

#define MOOSE_STORE_TMP_DB_PATH "/tmp/.moosecat.tmp.db"


/**
 * @brief Open a :memory: db
 *
 * Returns true on success.
 */
bool moose_strprv_open_memdb(MooseStore * self);

/**
 * @brief Insert a single song to the db.
 *
 * You should call moose_stprv_begin/commit before and after.
 */
bool moose_stprv_insert_song(MooseStore * db, MooseSong * song);

/**
 * @brief Update the db's meta table.
 */
void moose_stprv_insert_meta_attributes(MooseStore * self);

/**
 * @brief Compile all sqlite3_stmt objects.
 *
 * Needed before any calls after open_memdb
 */
void moose_stprv_prepare_all_statements(MooseStore * self);

/**
 * @brief Initialize the statement buffer of MooseStore
 */
sqlite3_stmt ** moose_stprv_prepare_all_statements_listed(MooseStore * self, const char ** sql_stmts, int offset, int n_stmts);

/**
 * @brief Deinitialize the statement buffer of MooseStore
 */
void moose_stprv_finalize_statements(MooseStore * self, sqlite3_stmt ** stmts, int offset, int n_stmts);

/**
 * @brief Creates the (empty) song table.
 */
bool moose_stprv_create_song_table(MooseStore * self);

/**
 * @brief Execute COMMIT;
 */
void moose_stprv_commit(MooseStore * self);

/**
 * @brief Execute BEGIN IMMEDIATE;
 */
void moose_stprv_begin(MooseStore * self);

/**
 * @brief DELETE FROM SONGS;
 */
void moose_stprv_delete_songs_table(MooseStore * self);

/**
 * @brief load or save a memory db to/from disk.
 *
 * @param is_save true if db should be saved.
 *
 * @returns 0 on success, an SQLITE errcode otherwise.
 */
void moose_stprv_load_or_save(MooseStore * self, bool is_save, const char * db_path);

/**
 * @brief Load songs from previously saved database into stack.
 */
void moose_stprv_deserialize_songs(MooseStore * self);

/**
 * @brief Same as moose_stprv_select_to_buf, but use stack instead of buf.
 *
 * @param self store to operate on
 * @param match_clause FTS match clause
 * @param queue_only limit search to queue?
 * @param stack an MoosePlaylist, ideally preallocated to the expected size.
 * @param limit_len Max songs to select, or -1 for no limit
 *
 * @return the number of selected songs.
 */
int moose_stprv_select_to_stack(
    MooseStore * self,
    const char * match_clause,
    bool queue_only,
    MoosePlaylist * stack,
    int limit_len);

/**
 * @brief get server db version
 */
int moose_stprv_get_db_version(MooseStore * self);

/**
 * @brief get playlist version
 */
int moose_stprv_get_pl_version(MooseStore * self);

/**
 * @brief get schema version (only to check if db is old)
 */
int moose_stprv_get_sc_version(MooseStore * self);

/**
 * @brief get mpd port to the db where this belongs to.
 */
int moose_stprv_get_mpd_port(MooseStore * self);

/**
 * @brief select count(*) from songs;
 */
int moose_stprv_get_song_count(MooseStore * self);

/**
 * @brief get mpd host in the meta table.
 *
 * Free return value if no longer used.
 */
char * moose_stprv_get_mpd_host(MooseStore * self);

/**
 * @brief Clear pos/id in the songs table (to -1)
 */
int moose_stprv_queue_clip(MooseStore * self, int since_pos);

/**
 * @brief Update a song, identified by file's pos/id to pos/idx
 */
void moose_stprv_queue_insert_posid(MooseStore * self, int pos, int idx, const char * file);

/**
 * @brief Update the song's stack songs pos/id according to the songs table.
 */
void moose_stprv_queue_update_stack_posid(MooseStore * self);

/**
 * @brief Close the sqlite3 handle
 */
void moose_stprv_close_handle(MooseStore * self, bool free_statements);

/**
 * @brief Lock self->stack and self->spl.stack for change
 */
void moose_stprv_lock_attributes(MooseStore * self);

/**
 * @brief Unlock previous lock by moose_stprv_lock_attributes()
 */
void moose_stprv_unlock_attributes(MooseStore * self);

/* @brief Count the 'depth' of a path.
 * @param dir_path the path to count
 *
 * Examples:
 *
 *  Path:     Depth:
 *  /         0
 *  a/        0
 *  a         0
 *  a/b       1
 *  a/b/      1
 *  a/b/c     2
 *
 *  @return the depth.
 */
int moose_stprv_path_get_depth(const char * dir_path);

/**
 * @brief Insert a new directory to the database
 *
 * @param self Store to insert it inot
 * @param path what path you want to insert
 */
void moose_stprv_dir_insert(MooseStore * self, const char * path);

/**
 * @brief Delete all contents from the dir table.
 *
 * @param self the store to operate on
 */
void moose_stprv_dir_delete(MooseStore * self);

/**
 * @brief Query the database
 *
 * @param self the store to search.
 * @param stack stack to write the results too
 * @param directory Search query or NULL
 * @param depth depth or -1
 *
 * @return number of matches
 */
int moose_stprv_dir_select_to_stack(MooseStore * self, MoosePlaylist * stack, const char * directory, int depth);

/**
 * @brief Compile all SQL statements
 *
 * @param self Store to operate on
 */
void moose_stprv_dir_prepare_statemtents(MooseStore * self);

/**
 * @brief Free all prepared statements
 *
 * @param self Store to operate on
 */
void moose_stprv_dir_finalize_statements(MooseStore * self);


/**
 * Enumeration of all operations understood by moose_store_job_execute_callback()
 *
 * A name and priority can be given to the enum in MooseJobNames and MooseJobPrios
 * in db.c (somewhere at the top)
 */
typedef enum {
    MOOSE_OPER_UNDEFINED       = 0,       /* Actually, not used.                                 */
    MOOSE_OPER_DESERIALIZE     = 1 << 0,  /* Load all songs from the database                    */
    MOOSE_OPER_LISTALLINFO     = 1 << 1, /* Get the database from mpd                            */
    MOOSE_OPER_PLCHANGES       = 1 << 2, /* Get the Queue from mpd                               */
    MOOSE_OPER_DB_SEARCH       = 1 << 3, /* Search Data in the Queue or whole DB                 */
    MOOSE_OPER_DIR_SEARCH      = 1 << 4, /* List a directory of the music directory              */
    MOOSE_OPER_SPL_LOAD        = 1 << 5, /* Load a stored playlist                               */
    MOOSE_OPER_SPL_UPDATE      = 1 << 6, /* Update loaded playlists                              */
    MOOSE_OPER_SPL_LIST        = 1 << 7, /* List the names of all loaded playlists               */
    MOOSE_OPER_SPL_QUERY       = 1 << 8, /* Query a stored playlist                              */
    MOOSE_OPER_SPL_LIST_ALL    = 1 << 9, /* List the mpd_playlist objects of all known playlists */
    MOOSE_OPER_UPDATE_META     = 1 << 10, /* Update meta information about the table             */
    MOOSE_OPER_WRITE_DATABASE  = 1 << 11, /* Write Database to disk, making a backup             */
    MOOSE_OPER_FIND_SONG_BY_ID = 1 << 12, /* Find a song by it's SongID                          */
    MOOSE_OPER_ENUM_MAX        = 1 << 13, /* Highest Value in this Enum                          */
} MooseStoreOperation;

/**
 * @brief List all Songs from the Database and write them to the Database.
 *
 * The function prototype is compatible to MooseJobManager.
 *
 * @param store The Store passed.
 * @param cancel A flag indicating the desired cancellation of a Job.
 */
void moose_stprv_oper_listallinfo(MooseStore * store, volatile bool * cancel);

/**
 * @brief List all changes of the Queue since the last time.
 *
 * The function prototype is compatible to MooseJobManager.
 *
 * @param store The Store passed.
 * @param cancel A flag indicating the desired cancellation of a Job.
 */
void moose_stprv_oper_plchanges(MooseStore * store, volatile bool * cancel);

/**
 * @brief Init self->spl.*
 *
 * @param self the store to initialzie stored playlist support
 */
void moose_stprv_spl_init(MooseStore * self);

/**
 * @brief Destroy all ressources allocated by moose_stprv_spl_init()
 *
 * @param self the store you called moose_stprv_spl_init on
 */
void moose_stprv_spl_destroy(MooseStore * self);

/**
 * @brief Update stored playlists from MPD
 *
 * - This will only fetch metadata about the playlist.
 *   use moose_stprv_spl_load to actually load them
 * - This will drop orphanded playlists.
 *
 * @param self the store to update.
 */
void moose_stprv_spl_update(MooseStore * self);

/**
 * @brief Load a certain playlist.
 *
 * This actually fetches the
 *
 * @param store the store to load them to.
 * @param playlist a mpd_playlist struct
 */
bool moose_stprv_spl_load(MooseStore * store, struct mpd_playlist * playlist);

/**
 * @brief Load a playlist by it's name.
 *
 * @param store the store to load them to
 * @param playlist_name  a playlist name
 */
bool moose_stprv_spl_load_by_playlist_name(MooseStore * store, const char * playlist_name);

/**
 * @brief Search in a Playlist
 *
 * @param store the store that knows about the Playlist
 * @param out_stack stack to write the results to
 * @param playlist_name name of the playlist
 * @param match_clause a match clause as understood by FTS
 *
 * @return Number of selected songs.
 */
int moose_stprv_spl_select_playlist(MooseStore * store, MoosePlaylist * out_stack, const char * playlist_name, const char * match_clause);

/**
 * @brief Select all loaded mpd_playlists object to a stack.
 *
 * @param store the store that knows about the playlists
 * @param stack the stack to write the mpd_playlist objects to
 *
 * @return Number of laoded playlists
 */
int moose_stprv_spl_get_loaded_playlists(MooseStore * store, MoosePlaylist * stack);

/**
 * @brief Return a list of a all known mpd_playlists structs
 *
 * @param store the store which knows about them
 * @param stack the stack where to write them to
 *
 * @return number of appended playlisyts
 */
int moose_stprv_spl_get_known_playlists(MooseStore * store, GPtrArray * stack);


#endif /* end of include guard: MOOSE_DB_PRIVATE_H */
