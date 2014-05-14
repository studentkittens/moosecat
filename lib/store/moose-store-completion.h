#ifndef MOOSE_DB_COMPLETION_H
#define MOOSE_DB_COMPLETION_H

struct MooseStoreCompletion;
typedef struct MooseStoreCompletion MooseStoreCompletion;

struct MooseStore;

MooseStoreCompletion * moose_store_cmpl_new(struct MooseStore * store);
void moose_store_cmpl_free(MooseStoreCompletion * self);
char * moose_store_cmpl_lookup(
    MooseStoreCompletion * self,
    enum mpd_tag_type tag, const char * key
    );


#endif /* end of include guard: MOOSE_DB_COMPLETION_H */
