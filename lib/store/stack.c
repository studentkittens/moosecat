#include <stdbool.h>
#include <string.h>

#include "stack.h"

///////////////////////////////

static void mc_store_stack_free_data (mc_StoreStack * self)
{
    for (gsize i = 0; i < self->stack->len; ++i) {
        gpointer data = g_ptr_array_index (self->stack, i);
        if (data != NULL)
            self->free_func (data);

        g_ptr_array_index (self->stack, i) = NULL;
    }
}

///////////////////////////////

mc_StoreStack * mc_store_stack_create (long size_hint, GDestroyNotify free_func)
{
    mc_StoreStack * self = g_new0 (mc_StoreStack, 1);
    if (self != NULL) {
        self->free_func = free_func;
        self->stack = g_ptr_array_sized_new (size_hint);
        g_ptr_array_set_free_func (self->stack, free_func);
        memset (self->stack->pdata, 0, self->stack->len);
    }

    return self;
}

///////////////////////////////

void mc_store_stack_append (mc_StoreStack * self, void * ptr)
{
    g_ptr_array_add (self->stack, ptr);
}

///////////////////////////////

void mc_store_stack_clear (mc_StoreStack * self, int resize)
{
    mc_store_stack_free_data (self);

    /* enlargen if needed (avoids expensive reallocation) */
    if (resize >= 0 && resize > (int) self->stack->len)
        g_ptr_array_set_size (self->stack, resize);
}

///////////////////////////////

void mc_store_stack_free (mc_StoreStack * self)
{
    if (self == NULL)
        return;

    g_ptr_array_free (self->stack, TRUE);
    g_free (self);
}

///////////////////////////////
