#include <stdbool.h>
#include <string.h>

#include "moose-store-playlist.h"

typedef struct _MoosePlaylistPrivate {
    GPtrArray * stack;
    GDestroyNotify free_func;
} MoosePlaylistPrivate;


G_DEFINE_TYPE_WITH_PRIVATE(MoosePlaylist, moose_playlist, G_TYPE_OBJECT);



static void moose_playlist_finalize(GObject * gobject) {
    MoosePlaylist * self = MOOSE_PLAYLIST(gobject);

    if (self == NULL) {
        return;
    }

    g_ptr_array_free(self->priv->stack, TRUE);

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}



static void moose_playlist_class_init(MoosePlaylistClass * klass) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_playlist_finalize;
}



static void moose_playlist_init(MoosePlaylist * self) {
    self->priv = moose_playlist_get_instance_private(self);
    self->priv->stack = g_ptr_array_new();
    self->priv->free_func = NULL;
    memset(self->priv->stack->pdata, 0, self->priv->stack->len);
}


//          PUBLIC           //


MoosePlaylist * moose_playlist_new(void) {
    return g_object_new(MOOSE_TYPE_PLAYLIST, NULL);
}



MoosePlaylist * moose_playlist_new_full(long size_hint, GDestroyNotify free_func) {
    MoosePlaylist * self = g_object_new(MOOSE_TYPE_PLAYLIST, NULL);

    if (self->priv->stack) {
        g_ptr_array_free(self->priv->stack, TRUE);
    }

    self->priv->stack = g_ptr_array_sized_new(size_hint);
    g_ptr_array_set_free_func(self->priv->stack, free_func);
    self->priv->free_func = free_func;
    return self;
}



void moose_playlist_append(MoosePlaylist * self, void * ptr) {
    g_ptr_array_add(self->priv->stack, ptr);
}



void moose_playlist_clear(MoosePlaylist * self) {
    g_ptr_array_set_size(self->priv->stack, 0);
}



unsigned moose_playlist_length(MoosePlaylist * self) {
    g_return_val_if_fail(self, 1);
    return self->priv->stack->len;
}



void moose_playlist_sort(MoosePlaylist * self, GCompareFunc func) {
    if (self == NULL || func == NULL) {
        return;
    }

    g_ptr_array_sort(self->priv->stack, func);
}



void * moose_playlist_at(MoosePlaylist * self, unsigned at) {
    if (at < moose_playlist_length(self)) {
        return g_ptr_array_index(self->priv->stack, at);
    } else {
        g_warning("Invalid index for stack %p: %d\n", self, at);
        return NULL;
    }
}



MoosePlaylist * moose_playlist_copy(MoosePlaylist * self) {
    size_t size = moose_playlist_length(self);
    if (self == NULL || size == 0) {
        return NULL;
    }

    MoosePlaylist * other = moose_playlist_new_full(size, self->priv->free_func);

    for (size_t i = 0; i < size; ++i) {
        g_ptr_array_add(other->priv->stack,
                        g_ptr_array_index(self->priv->stack, i)
                       );
    }

    return other;
}
