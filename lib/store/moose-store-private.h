#ifndef MOOSE_DB_PRIVATE_H
#define MOOSE_DB_PRIVATE_H

#include "moose-store-store.h"

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
bool moose_stprv_insert_song(MooseStore * db, struct mpd_song * song);

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
sqlite3_stmt * * moose_stprv_prepare_all_statements_listed(MooseStore * self, const char * * sql_stmts, int offset, int n_stmts);

/**
 * @brief Deinitialize the statement buffer of MooseStore
 */
void moose_stprv_finalize_statements(MooseStore * self, sqlite3_stmt * * stmts, int offset, int n_stmts);

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

#endif /* end of include guard: MOOSE_DB_PRIVATE_H */
