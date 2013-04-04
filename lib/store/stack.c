#include <stdbool.h>
#include <string.h>

#include "stack.h"

///////////////////////////////

mc_Stack *mc_stack_create(long size_hint, GDestroyNotify free_func)
{
    mc_Stack *self = g_new0(mc_Stack, 1);

    if (self != NULL) {
        self->free_func = free_func;
        self->stack = g_ptr_array_sized_new(size_hint);
        g_ptr_array_set_free_func(self->stack, free_func);
        memset(self->stack->pdata, 0, self->stack->len);
    }

    return self;
}

///////////////////////////////

void mc_stack_append(mc_Stack *self, void *ptr)
{
    g_ptr_array_add(self->stack, ptr);
}

///////////////////////////////

void mc_stack_clear(mc_Stack *self)
{
    g_ptr_array_set_size(self->stack, 0);
}

///////////////////////////////

void mc_stack_free(mc_Stack *self)
{
    if (self == NULL)
        return;

    g_ptr_array_free(self->stack, TRUE);
    g_free(self);
}

///////////////////////////////

unsigned mc_stack_length(mc_Stack *self)
{
    if (self == NULL)
        return 0;

    return self->stack->len;
}

///////////////////////////////

void mc_stack_sort(mc_Stack *self, GCompareFunc func)
{
    if (self == NULL || func == NULL)
        return;

    g_ptr_array_sort(self->stack, func);
}

///////////////////////////////

void *mc_stack_at(mc_Stack *self, unsigned at)
{
    return g_ptr_array_index(self->stack, at);
}
