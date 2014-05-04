#include <stdbool.h>
#include <string.h>

#include "stack.h"

///////////////////////////////

mc_Playlist *mc_stack_create(long size_hint, GDestroyNotify free_func)
{
    mc_Playlist *self = g_new0(mc_Playlist, 1);

    if (self != NULL) {
        self->free_func = free_func;
        self->stack = g_ptr_array_sized_new(size_hint);
        g_ptr_array_set_free_func(self->stack, free_func);
        memset(self->stack->pdata, 0, self->stack->len);
    }

    return self;
}

///////////////////////////////

void mc_stack_append(mc_Playlist *self, void *ptr)
{
    g_ptr_array_add(self->stack, ptr);
}

///////////////////////////////

void mc_stack_clear(mc_Playlist *self)
{
    g_ptr_array_set_size(self->stack, 0);
}

///////////////////////////////

void mc_stack_free(mc_Playlist *self)
{
    if (self == NULL)
        return;

    g_ptr_array_free(self->stack, TRUE);
    g_free(self);
}

///////////////////////////////

unsigned mc_stack_length(mc_Playlist *self)
{
    if (self == NULL)
        return 0;

    return self->stack->len;
}

///////////////////////////////

void mc_stack_sort(mc_Playlist *self, GCompareFunc func)
{
    if (self == NULL || func == NULL)
        return;

    g_ptr_array_sort(self->stack, func);
}

///////////////////////////////

void *mc_stack_at(mc_Playlist *self, unsigned at)
{
    if(at < mc_stack_length(self)) {
        return g_ptr_array_index(self->stack, at);
    } else {
        g_error("Invalid index for stack %p: %d\n", self, at);
        return NULL;
    }
}

///////////////////////////////

mc_Playlist * mc_stack_copy(mc_Playlist *self)
{
    size_t size = mc_stack_length(self);
    if(self == NULL || size == 0)
        return NULL;

    mc_Playlist *other = mc_stack_create(size, self->free_func);

    for(size_t i = 0; i < size; ++i) {
        g_ptr_array_add(other->stack, 
            g_ptr_array_index(self->stack, i)
        );
    }

    return other;
}
