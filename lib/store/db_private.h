#ifndef MC_DB_PRIVATE_H
#define MC_DB_PRIVATE_H

#include "stack.h"
#include "query.h"
#include "sqlite3.h"
#include "../mpd/protocol.h"

/* this is used only internally, not useful for public users */
enum
{
    /* creation of the basic tables */
    _MC_SQL_CREATE = 0,
    /* insert one song */
    _MC_SQL_INSERT,
    /* select using match */
    _MC_SQL_SELECT,
    /* begin statement */
    _MC_SQL_BEGIN,
    /* commit statement */
    _MC_SQL_COMMIT,
    /* total number of defined sources */
    _MC_SQL_SOURCE_COUNT
};


typedef struct mc_StoreDB
{
    /* full path to the sql database (used for open/close) */
    const char * db_path;

    /* songstack - a place for mpd_songs to live in */
    mc_StoreStack * stack;

    /* handle to sqlite */
    sqlite3 * handle;

    /* prepared statements */
    sqlite3_stmt * sql_prep_stmts[_MC_SQL_SOURCE_COUNT];

    /* client associated with this store */
    mc_Client * client;

} mc_StoreDB;

void mc_stprv_prepare_all_statements (mc_StoreDB * self);
bool mc_strprv_open_memdb (mc_StoreDB * self);
bool mc_stprv_insert_song (mc_StoreDB * db, mpd_song * song);
mpd_song ** mc_stprv_select (mc_StoreDB * self, const char * match_clause);
int mc_stprv_load_or_save (mc_StoreDB * self, bool is_save);

void mc_stprv_commit (mc_StoreDB * self);
void mc_stprv_begin (mc_StoreDB * self);
int mc_stprv_select_out (mc_StoreDB * self, const char * match_clause, mpd_song ** song_buffer, int buffer_len);

#endif /* end of include guard: MC_DB_PRIVATE_H */
