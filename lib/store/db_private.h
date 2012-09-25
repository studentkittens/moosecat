#ifndef MC_DB_PRIVATE_H
#define MC_DB_PRIVATE_H

#include "stack.h"
#include "query.h"
#include "sqlite3.h"
#include "../mpd/protocol.h"

/* DB Layout version. Older tables will not be loaded. */
#define MC_DB_SCHEMA_VERSION 0
     
#define PRINT_LOCKS 0

#define LOCK_UPDATE_MTX(store)                           \
    store->db_is_locked = TRUE;                          \
    g_rec_mutex_lock (&store->db_update_lock);           \
    if(PRINT_LOCKS)                                      \
      g_print ("LOCK (%s:%d)\n", __FILE__, __LINE__);

#define UNLOCK_UPDATE_MTX(store)                         \
    g_rec_mutex_unlock (&store->db_update_lock);         \
    store->db_is_locked = FALSE;                         \
    if(PRINT_LOCKS)                                      \
      g_print ("UNLOCK (%s:%d)\n", __FILE__, __LINE__);  \

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
    /* === total number of defined sources === */
    _MC_SQL_SOURCE_COUNT
    /* ======================================= */
};

typedef struct mc_StoreDB {
    /* full path to the sql database (used for open/close) */
    char * db_path;

    /* songstack - a place for mpd_songs to live in */
    mc_StoreStack * stack;

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

} mc_StoreDB;


bool mc_strprv_open_memdb (mc_StoreDB * self);
bool mc_stprv_insert_song (mc_StoreDB * db, mpd_song * song);
void mc_stprv_insert_meta_attributes (mc_StoreDB * self);

void mc_stprv_prepare_all_statements (mc_StoreDB * self);
bool mc_stprv_create_song_table (mc_StoreDB * self);
void mc_stprv_finalize_all_statements (mc_StoreDB * self);
void mc_stprv_commit (mc_StoreDB * self);
void mc_stprv_begin (mc_StoreDB * self);
void mc_stprv_delete_songs_table (mc_StoreDB * self);

int mc_stprv_load_or_save (mc_StoreDB * self, bool is_save);
int mc_stprv_select_out (mc_StoreDB * self, const char * match_clause, bool queue_only, mpd_song ** song_buffer, int buffer_len);
gpointer mc_stprv_deserialize_songs (mc_StoreDB * self);
void mc_stprv_deserialize_songs_bkgd (mc_StoreDB * self);

/* getting information about the db itself */
int mc_stprv_get_db_version (mc_StoreDB * self);
int mc_stprv_get_pl_version (mc_StoreDB * self);
int mc_stprv_get_sc_version (mc_StoreDB * self);
int mc_stprv_get_mpd_port (mc_StoreDB * self);
int mc_stprv_get_song_count (mc_StoreDB * self);

char * mc_stprv_get_mpd_host (mc_StoreDB * self);

void mc_stprv_queue_update_posid (mc_StoreDB * self, int pos, int idx, const char * file);
void mc_stprv_queue_update_stack_posid (mc_StoreDB * self);

#endif /* end of include guard: MC_DB_PRIVATE_H */
