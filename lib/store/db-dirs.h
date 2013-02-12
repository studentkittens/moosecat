#ifndef MC_DB_DIRS_HH
#define MC_DB_DIRS_HH

void mc_stprv_dir_insert(mc_StoreDB *self, const char *path);
void mc_stprv_dir_delete(mc_StoreDB *self);
int mc_stprv_dir_select_to_stack(mc_StoreDB *self, mc_Stack *stack, const char *directory, int depth);
void mc_stprv_dir_prepare_statemtents(mc_StoreDB *self);
void mc_stprv_dir_finalize_statements(mc_StoreDB *self);

#endif /* end of include guard: MC_DB_DIRS_HH */

