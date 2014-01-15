#ifndef DB_MACROS_HH
#define DB_MACROS_HH

#define bind_int(db, type, pos_idx, value, error_id) \
    error_id |= sqlite3_bind_int (SQL_STMT(db, type), (pos_idx)++, value);

#define bind_txt(db, type, pos_idx, value, error_id) \
    error_id |= sqlite3_bind_text (SQL_STMT(db, type), (pos_idx)++, value, -1, NULL);

#define bind_tag(db, type, pos_idx, song, tag_id, error_id) \
    bind_txt (db, type, pos_idx, mpd_song_get_tag (song, tag_id, 0), error_id)

/*
 * DB Layout version.
 * Older tables will not be loaded.
 * */
#define MC_DB_SCHEMA_VERSION 2

#define MC_STORE_TMP_DB_PATH "/tmp/.moosecat.tmp.db"


#define REPORT_SQL_ERROR(store, message)                                                           \
    mc_shelper_report_error_printf (store->client, "[%s:%d] %s -> %s (#%d)",                       \
                                    __FILE__, __LINE__, message,                                   \
                                    sqlite3_errmsg(store->handle), sqlite3_errcode(store->handle)) \
 
#define CLEAR_BINDS(stmt)          \
    sqlite3_reset (stmt);          \
    sqlite3_clear_bindings (stmt); \
 
#define CLEAR_BINDS_BY_NAME(store, type) \
    CLEAR_BINDS (SQL_STMT (store, type))

#endif
