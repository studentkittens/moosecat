#ifndef MC_STORE_STACK_H
#define MC_STORE_STACK_H

#include <glib.h>

typedef struct
{
    GArray * stack;
} mc_StoreStack;

mc_StoreStack * mc_store_stack_create (long size_hint);

void mc_store_stack_append (mc_StoreStack * self, void * ptr);

void mc_store_stack_free (mc_StoreStack * self);

/* For peformance reason a define (vs. inline function) */
#define mc_store_stack_at(self, at) g_array_index (self->stack, struct mpd_song *, at)

#endif /* end of include guard: MC_STORE_STACK_H */

