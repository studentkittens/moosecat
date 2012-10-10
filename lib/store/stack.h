#ifndef MC_STORE_STACK_H
#define MC_STORE_STACK_H

#include <glib.h>

typedef struct {
    GPtrArray * stack;
    GDestroyNotify free_func;
} mc_Stack;

mc_Stack * mc_stack_create (long size_hint, GDestroyNotify free_func);

void mc_stack_append (mc_Stack * self, void * ptr);

void mc_stack_free (mc_Stack * self);

void mc_stack_clear (mc_Stack * self, int resize);

unsigned mc_stack_length (mc_Stack * self);

void mc_stack_sort (mc_Stack * self, GCompareFunc func);

/* For peformance reason a define (vs. inline function) */
#define mc_stack_at(self, at) g_ptr_array_index (self->stack, at)

#endif /* end of include guard: MC_STORE_STACK_H */

