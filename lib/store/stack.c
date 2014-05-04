#include <stdbool.h>
#include <string.h>

#include "stack.h"

///////////////////////////////

MoosePlaylist *moose_stack_create(long size_hint, GDestroyNotify free_func)
{
    MoosePlaylist *self = g_new0(MoosePlaylist, 1);

    if (self != NULL) {
        self->free_func = free_func;
        self->stack = g_ptr_array_sized_new(size_hint);
        g_ptr_array_set_free_func(self->stack, free_func);
        memset(self->stack->pdata, 0, self->stack->len);
    }

    return self;
}

///////////////////////////////

void moose_stack_append(MoosePlaylist *self, void *ptr)
{
    g_ptr_array_add(self->stack, ptr);
}

///////////////////////////////

void moose_stack_clear(MoosePlaylist *self)
{
    g_ptr_array_set_size(self->stack, 0);
}

///////////////////////////////

void moose_stack_free(MoosePlaylist *self)
{
    if (self == NULL)
        return;

    g_ptr_array_free(self->stack, TRUE);
    g_free(self);
}

///////////////////////////////

unsigned moose_stack_length(MoosePlaylist *self)
{
    if (self == NULL)
        return 0;

    return self->stack->len;
}

///////////////////////////////

void moose_stack_sort(MoosePlaylist *self, GCompareFunc func)
{
    if (self == NULL || func == NULL)
        return;

    g_ptr_array_sort(self->stack, func);
}

///////////////////////////////

void *moose_stack_at(MoosePlaylist *self, unsigned at)
{
    if(at < moose_stack_length(self)) {
        return g_ptr_array_index(self->stack, at);
    } else {
        g_error("Invalid index for stack %p: %d\n", self, at);
        return NULL;
    }
}

///////////////////////////////

MoosePlaylist * moose_stack_copy(MoosePlaylist *self)
{
    size_t size = moose_stack_length(self);
    if(self == NULL || size == 0)
        return NULL;

    MoosePlaylist *other = moose_stack_create(size, self->free_func);

    for(size_t i = 0; i < size; ++i) {
        g_ptr_array_add(other->stack, 
            g_ptr_array_index(self->stack, i)
        );
    }

    return other;
}
