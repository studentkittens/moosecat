#ifndef MOOSE_DB_DIRS_HH
#define MOOSE_DB_DIRS_HH

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

#endif /* end of include guard: MOOSE_DB_DIRS_HH */
