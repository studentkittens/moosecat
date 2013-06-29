#ifndef MC_DB_PRIVATE_H
#define MC_DB_PRIVATE_H

#include "db-store.h"

/**
 * @brief Open a :memory: db
 *
 * Returns true on success.
 */
bool mc_strprv_open_memdb(mc_Store *self);

/**
 * @brief Insert a single song to the db.
 *
 * You should call mc_stprv_begin/commit before and after.
 */
bool mc_stprv_insert_song(mc_Store *db, struct mpd_song *song);

/**
 * @brief Update the db's meta table.
 */
void mc_stprv_insert_meta_attributes(mc_Store *self);

/**
 * @brief Compile all sqlite3_stmt objects.
 *
 * Needed before any calls after open_memdb
 */
void mc_stprv_prepare_all_statements(mc_Store *self);

/**
 * @brief Initialize the statement buffer of mc_Store
 */
sqlite3_stmt **mc_stprv_prepare_all_statements_listed(mc_Store *self, const char **sql_stmts, int offset, int n_stmts);

/**
 * @brief Deinitialize the statement buffer of mc_Store
 */
void mc_stprv_finalize_statements(mc_Store *self, sqlite3_stmt **stmts, int offset, int n_stmts);

/**
 * @brief Creates the (empty) song table.
 */
bool mc_stprv_create_song_table(mc_Store *self);

/**
 * @brief Execute COMMIT;
 */
void mc_stprv_commit(mc_Store *self);

/**
 * @brief Execute BEGIN IMMEDIATE;
 */
void mc_stprv_begin(mc_Store *self);

/**
 * @brief DELETE FROM SONGS;
 */
void mc_stprv_delete_songs_table(mc_Store *self);

/**
 * @brief load or save a memory db to/from disk.
 *
 * @param is_save true if db should be saved.
 *
 * @returns 0 on success, an SQLITE errcode otherwise.
 */
void mc_stprv_load_or_save(mc_Store *self, bool is_save, const char *db_path);

/**
 * @brief Load songs from previously saved database into stack.
 */
void mc_stprv_deserialize_songs(mc_Store *self);

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
int mc_stprv_select_to_buf(
    mc_Store *self,
    const char *match_clause,
    bool queue_only,
    struct mpd_song **song_buffer,
    int buffer_len);

/**
 * @brief Same as mc_stprv_select_to_buf, but use stack instead of buf.
 *
 * @param self store to operate on
 * @param match_clause FTS match clause
 * @param queue_only limit search to queue?
 * @param stack an mc_Stack, ideally preallocated to the expected size.
 * @param limit_len Max songs to select, or -1 for no limit
 *
 * @return the number of selected songs.
 */
int mc_stprv_select_to_stack(
    mc_Store *self,
    const char *match_clause,
    bool queue_only,
    mc_Stack *stack,
    int limit_len);

/**
 * @brief get server db version
 */
int mc_stprv_get_db_version(mc_Store *self);

/**
 * @brief get playlist version
 */
int mc_stprv_get_pl_version(mc_Store *self);

/**
 * @brief get schema version (only to check if db is old)
 */
int mc_stprv_get_sc_version(mc_Store *self);

/**
 * @brief get mpd port to the db where this belongs to.
 */
int mc_stprv_get_mpd_port(mc_Store *self);

/**
 * @brief select count(*) from songs;
 */
int mc_stprv_get_song_count(mc_Store *self);

/**
 * @brief get mpd host in the meta table.
 *
 * Free return value if no longer used.
 */
char *mc_stprv_get_mpd_host(mc_Store *self);

/**
 * @brief Clear pos/id in the songs table (to -1)
 */
int mc_stprv_queue_clip(mc_Store *self, int since_pos);

/**
 * @brief Update a song, identified by file's pos/id to pos/idx
 */
void mc_stprv_queue_insert_posid(mc_Store *self, int pos, int idx, const char *file);

/**
 * @brief Update the song's stack songs pos/id according to the songs table.
 */
void mc_stprv_queue_update_stack_posid(mc_Store *self);

/**
 * @brief Close the sqlite3 handle
 */
void mc_stprv_close_handle(mc_Store *self);

/**
 * @brief Lock self->stack and self->spl.stack for change
 */
void mc_stprv_lock_attributes(mc_Store *self);

/**
 * @brief Unlock previous lock by mc_stprv_lock_attributes()
 */
void mc_stprv_unlock_attributes(mc_Store *self);

#endif /* end of include guard: MC_DB_PRIVATE_H */
