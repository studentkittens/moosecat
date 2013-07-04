#ifndef MC_DB_DIRS_HH
#define MC_DB_DIRS_HH

/**
 * @brief Insert a new directory to the database 
 *
 * @param self Store to insert it inot
 * @param path what path you want to insert
 */
void mc_stprv_dir_insert(mc_Store *self, const char *path);

/**
 * @brief Delete all contents from the dir table.
 *
 * @param self the store to operate on
 */
void mc_stprv_dir_delete(mc_Store *self);

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
int mc_stprv_dir_select_to_stack(mc_Store *self, mc_Stack *stack, const char *directory, int depth);

/**
 * @brief Compile all SQL statements
 *
 * @param self Store to operate on
 */
void mc_stprv_dir_prepare_statemtents(mc_Store *self);

/**
 * @brief Free all prepared statements
 *
 * @param self Store to operate on 
 */
void mc_stprv_dir_finalize_statements(mc_Store *self);

#endif /* end of include guard: MC_DB_DIRS_HH */

