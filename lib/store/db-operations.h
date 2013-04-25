#ifndef MC_DB_OPERATIONS_H
#define MC_DB_OPERATIONS_H

typedef enum {
    MC_OPER_UNDEFINED     = 0,      /* Actually, not used.                     */
    MC_OPER_DESERIALIZE   = 1 << 0, /* Load all songs from the database        */
    MC_OPER_LISTALLINFO   = 1 << 1, /* Get the database from mpd               */
    MC_OPER_PLCHANGES     = 1 << 2, /* Get the Queue from mpd                  */
    MC_OPER_DB_SEARCH     = 1 << 3,
    MC_OPER_DIR_SEARCH    = 1 << 4,
    MC_OPER_SPL_LOAD      = 1 << 5, /* Load a stored playlist                  */
    MC_OPER_SPL_UPDATE    = 1 << 6, /* Update loaded playlists                 */
    MC_OPER_SPL_LIST      = 1 << 7, /* List the names of all playlists         */
    MC_OPER_SPL_QUERY     = 1 << 8, /* Query a stored playlist                 */
    MC_OPER_UPDATE_META   = 1 << 9, /* Update meta information about the table */
} mc_StoreOperation;

void mc_store_oper_listallinfo(mc_StoreDB *store, volatile bool *cancel);

void mc_store_oper_plchanges(mc_StoreDB *store, volatile bool *cancel);

#endif /* end of include guard: MC_DB_OPERATIONS_H */
