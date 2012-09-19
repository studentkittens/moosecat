#ifndef MC_STORE_STACK_H
#define MC_STORE_STACK_H

#include <glib.h>

typedef struct {
    GPtrArray * stack;
    GDestroyNotify free_func;
} mc_StoreStack;

mc_StoreStack * mc_store_stack_create (long size_hint, GDestroyNotify free_func);

void mc_store_stack_append (mc_StoreStack * self, void * ptr);

void mc_store_stack_free (mc_StoreStack * self);

void mc_store_stack_clear (mc_StoreStack * self, int resize);

/* For peformance reason a define (vs. inline function) */
#define mc_store_stack_at(self, at) g_ptr_array_index (self->stack, at)

#endif /* end of include guard: MC_STORE_STACK_H */

