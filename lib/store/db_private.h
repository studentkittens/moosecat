#ifndef MC_DB_PRIVATE_H
#define MC_DB_PRIVATE_H

#include "stack.h"
#include "query.h"
#include "sqlite3.h"
#include "../mpd/protocol.h"

/* DB Layout version. Older tables will not be loaded. */
#define MC_DB_SCHEMA_VERSION 0

#define MC_STORE_SUPPORTED_TOKENIZERS {"simple", "porter", "unicode61", "icu", NULL}

#define PRINT_LOCKS 0

#define LOCK_UPDATE_MTX(store) {                               \
        store->db_is_locked = TRUE;                            \
        g_rec_mutex_lock (&store->db_update_lock);             \
        if(PRINT_LOCKS)                                        \
            g_print ("LOCK (%s:%d)\n", __FILE__, __LINE__);    \
    }

#define UNLOCK_UPDATE_MTX(store) {                             \
        g_rec_mutex_unlock (&store->db_update_lock);           \
        store->db_is_locked = FALSE;                           \
        if(PRINT_LOCKS)                                        \
            g_print ("UNLOCK (%s:%d)\n", __FILE__, __LINE__);  \
    }

#define REPORT_SQL_ERROR(store, message)                                                           \
    mc_shelper_report_error_printf (store->client, "[%s:%d] %s -> %s (#%d)",                       \
                                    __FILE__, __LINE__, message,                                   \
                                    sqlite3_errmsg(store->handle), sqlite3_errcode(store->handle)) \
 
#define CLEAR_BINDS(stmt)          \
    sqlite3_reset (stmt);          \
    sqlite3_clear_bindings (stmt); \

#define CLEAR_BINDS_BY_NAME(store, type) \
    CLEAR_BINDS (SQL_STMT (store, type))

/* this is used only internally, not useful for public users */
enum {
    /* creation of the basic tables */
    _MC_SQL_CREATE = 0,
    /* create the dummy meta table
     * (just to let the statements compile) */
    _MC_SQL_META_DUMMY,
    /* update info about the db */
    _MC_SQL_META,
    /* === border between non-prepared/prepared statements === */
    _MC_SQL_NEED_TO_PREPARE_COUNT,
    /* ======================================================= */
    /* update queue_pos / queue_idx */
    _MC_SQL_QUEUE_UPDATE_ROW,
    /* clear the pos/id fields */
    _MC_SQL_QUEUE_CLEAR,
    /* clear the pos/id fields by filename */
    _MC_SQL_QUEUE_CLEAR_ROW,
    /* select using match */
    _MC_SQL_SELECT_MATCHED,
    _MC_SQL_SELECT_MATCHED_ALL,
    /* select all */
    _MC_SQL_SELECT_ALL,
    /* select all queue songs */
    _MC_SQL_SELECT_ALL_QUEUE,
    /* delete all content from 'songs' */
    _MC_SQL_DELETE_ALL,
    /* select meta attributes */
    _MC_SQL_SELECT_META_DB_VERSION,
    _MC_SQL_SELECT_META_PL_VERSION,
    _MC_SQL_SELECT_META_SC_VERSION,
    _MC_SQL_SELECT_META_MPD_PORT,
    _MC_SQL_SELECT_META_MPD_HOST,
    /* count songs in db */
    _MC_SQL_COUNT,
    /* insert one song */
    _MC_SQL_INSERT,
    /* begin statement */
    _MC_SQL_BEGIN,
    /* commit statement */
    _MC_SQL_COMMIT,
    /* Update stored playlist info of a single song */
    _MC_SQL_SPL_UPDATE_SONG,
    /* select all playlist names from playlists table */
    _MC_SQL_SPL_SELECT_ALL_NAMES,
    _MC_SQL_DIR_INSERT,
    _MC_SQL_DIR_SELECT_DEPTH,
    _MC_SQL_DIR_DELETE_ALL,
    /* === total number of defined sources === */
    _MC_SQL_SOURCE_COUNT
    /* ======================================= */
};

typedef struct mc_StoreDB {
    /* directory db lies in */
    char * db_directory;

    /* songstack - a place for mpd_songs to live in */
    mc_StoreStack * stack;

    /* stored playlist stack - place for mpd_playlist */
    mc_StoreStack * spl_stack;

    /* handle to sqlite */
    sqlite3 * handle;

    /* prepared statements */
    sqlite3_stmt * sql_prep_stmts[_MC_SQL_SOURCE_COUNT];

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
    struct {
        bool use_compression;
    } settings;

    /* Support for stored playlists */
    struct {
        /* A stack of mpd_playlists (all of them) */
        mc_StoreStack * stack;

        sqlite3_stmt * select_tables_stmt;
    } spl;

} mc_StoreDB;


/**
 * @brief Open a :memory: db
 *
 * Returns true on success.
 */
bool mc_strprv_open_memdb (mc_StoreDB * self, const char * tokenizer);

/**
 * @brief Insert a single song to the db.
 *
 * You should call mc_stprv_begin/commit before and after.
 */
bool mc_stprv_insert_song (mc_StoreDB * db, mpd_song * song, int dir_index);

/**
 * @brief Update the db's meta table.
 */
void mc_stprv_insert_meta_attributes (mc_StoreDB * self);

/**
 * @brief Compile all sqlite3_stmt objects. 
 *
 * Needed before any calls after open_memdb
 */
void mc_stprv_prepare_all_statements (mc_StoreDB * self);

/**
 * @brief Creates the (empty) song table. 
 *
 * @param tokenizer algorithm to use to split words. NULL == default == "porter"
 *
 * See: http://www.sqlite.org/fts3.html#tokenizer
 */
bool mc_stprv_create_song_table (mc_StoreDB * self, const char * tokenizer);

/**
 * @brief Execute COMMIT;
 */
void mc_stprv_commit (mc_StoreDB * self);

/**
 * @brief Execute BEGIN IMMEDIATE;
 */
void mc_stprv_begin (mc_StoreDB * self);

/**
 * @brief DELETE FROM SONGS;
 */
void mc_stprv_delete_songs_table (mc_StoreDB * self);

/**
 * @brief load or save a memory db to/from disk.
 *
 * @param is_save true if db should be saved.
 *
 * @returns 0 on success, an SQLITE errcode otherwise. 
 */
void mc_stprv_load_or_save (mc_StoreDB * self, bool is_save, const char * db_path);

/**
 * @brief Load songs from previously saved database into stack.
 */
void mc_stprv_deserialize_songs (mc_StoreDB * self, bool lock_self);

/**
 * @brief Search the songs table. 
 *
 * @param self store to operate on
 * @param match_clause an FTS match clause.
 * @param queue_only limit search to queue?
 * @param song_buffer an self allocated buffer
 * @param buffer_len length of song_buffer
 *
 * @return the number of selected songs.
 */
int mc_stprv_select_to_buf (
        mc_StoreDB * self,
        const char * match_clause,
        bool queue_only,
        mpd_song ** song_buffer,
        int buffer_len);

/**
 * @brief Same as mc_stprv_select_to_buf, but use stack instead of buf.
 *
 * @param self store to operate on 
 * @param match_clause FTS match clause
 * @param queue_only limit search to queue?
 * @param stack an mc_StoreStack, ideally preallocated to the expected size.
 *
 * @return the number of selected songs.
 */
int mc_stprv_select_to_stack (
        mc_StoreDB * self,
        const char * match_clause,
        bool queue_only,
        mc_StoreStack * stack);

/**
 * @brief get server db version
 */
int mc_stprv_get_db_version (mc_StoreDB * self);

/**
 * @brief get playlist version
 */
int mc_stprv_get_pl_version (mc_StoreDB * self);

/**
 * @brief get schema version (only to check if db is old)
 */
int mc_stprv_get_sc_version (mc_StoreDB * self);

/**
 * @brief get mpd port to the db where this belongs to.
 */
int mc_stprv_get_mpd_port (mc_StoreDB * self);

/**
 * @brief select count(*) from songs;
 */
int mc_stprv_get_song_count (mc_StoreDB * self);

/**
 * @brief get mpd host in the meta table.
 *
 * Free return value if no longer used.
 */
char * mc_stprv_get_mpd_host (mc_StoreDB * self);

/**
 * @brief Clear the update flag in the whole table.
 */
void mc_stprv_queue_clear_update_flag (mc_StoreDB * self);

/**
 * @brief Clear pos/id in the songs table (to -1)
 */
int mc_stprv_queue_clip (mc_StoreDB * self, int since_pos);

/**
 * @brief Update a song, identified by file's pos/id to pos/idx
 */
void mc_stprv_queue_update_posid (mc_StoreDB * self, int pos, int idx, const char * file);

/**
 * @brief Update the song's stack songs pos/id according to the songs table.
 */
void mc_stprv_queue_update_stack_posid (mc_StoreDB * self);

/**
 * @brief Close the sqlite3 handle
 */
void mc_stprv_close_handle (mc_StoreDB * self);

void mc_stprv_dir_insert (mc_StoreDB * self, const char * path);
void mc_stprv_dir_delete (mc_StoreDB * self);
int mc_stprv_dir_select_to_stack (mc_StoreDB * self, mc_StoreStack * stack, const char * dir, int depth);

#endif /* end of include guard: MC_DB_PRIVATE_H */
