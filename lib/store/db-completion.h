#ifndef MC_DB_COMPLETION_H
#define MC_DB_COMPLETION_H

struct mc_StoreCompletion;
typedef struct mc_StoreCompletion mc_StoreCompletion;

struct mc_Store;

mc_StoreCompletion * mc_store_cmpl_new(struct mc_Store *store);
void mc_store_cmpl_free(mc_StoreCompletion *self);
char * mc_store_cmpl_lookup(
    mc_StoreCompletion *self,
    enum mpd_tag_type tag, const char *key
);


#endif /* end of include guard: MC_DB_COMPLETION_H */

