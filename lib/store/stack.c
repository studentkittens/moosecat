#include <stdbool.h>
#include <string.h>
#include <mpd/client.h>

#include "stack.h"

///////////////////////////////

mc_StoreStack * mc_store_stack_create (long size_hint)
{
    mc_StoreStack * self = g_new0 (mc_StoreStack, 1);
    if (self != NULL)
    {
        self->stack = g_array_sized_new (false, false, sizeof (void *), size_hint);
        memset (self->stack->data, 0, self->stack->len);
    }

    return self;
}

///////////////////////////////

void mc_store_stack_append (mc_StoreStack * self, void * ptr)
{
    g_array_append_val (self->stack, ptr);
}

///////////////////////////////

void mc_store_stack_free (mc_StoreStack * self)
{
    if (self == NULL)
        return;

    gsize i = 0;
    for (i = 0; i < self->stack->len; i++)
    {
        mpd_song_free (mc_store_stack_at (self, i) );
    }

    g_array_free (self->stack, TRUE);
    g_free (self);
}

///////////////////////////////
