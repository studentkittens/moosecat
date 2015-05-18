#ifndef MOOSE_STORE_STACK_H
#define MOOSE_STORE_STACK_H

/**
 * SECTION: moose-store-playlist
 * @short_description: Playlist-Class, a container for Songs.
 *
 * This basically is a wrapper around #GPtrArray, but might be extended
 * in the future to other Playlist responsibilities.
 */

#include <glib-object.h>
#include "../mpd/moose-song.h"

G_BEGIN_DECLS

/*
 * Type macros.
 */
#define MOOSE_TYPE_PLAYLIST (moose_playlist_get_type())
#define MOOSE_PLAYLIST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_PLAYLIST, MoosePlaylist))
#define MOOSE_IS_PLAYLIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_PLAYLIST))
#define MOOSE_PLAYLIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_PLAYLIST, MoosePlaylistClass))
#define MOOSE_IS_PLAYLIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_PLAYLIST))
#define MOOSE_PLAYLIST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_PLAYLIST, MoosePlaylistClass))

struct _MoosePlaylistPrivate;

typedef struct _MoosePlaylist {
    GObject parent;
    struct _MoosePlaylistPrivate* priv;
} MoosePlaylist;

typedef struct _MoosePlaylistClass { GObjectClass parent_class; } MoosePlaylistClass;

GType moose_playlist_get_type(void);

/**
 * moose_playlist_new:
 *
 * Allocates a new #MoosePlaylist
 *
 * Return value: a new #MoosePlaylist.
 */
MoosePlaylist* moose_playlist_new(void);

/**
 * moose_playlist_new_full:
 * @size_hint: Number of preallocations.
 * @free_func: Func to free the item.
 *
 * Allocates a new #MoosePlaylist
 *
 * Return value: a new #MoosePlaylist.
 */
MoosePlaylist* moose_playlist_new_full(long size_hint, GDestroyNotify free_func);

/**
 * moose_playlist_append:
 * @self: The #MoosePlaylist to append to.
 * @ptr: Some pointer.
 *
 * Append items to the Playlist.
 */
void moose_playlist_append(MoosePlaylist* self, void* ptr);

/**
 * moose_playlist_clear:
 * @self: The #MoosePlaylist to append to.
 *
 * Clear the whole playlist back to factory settings.
 */
void moose_playlist_clear(MoosePlaylist* self);

/**
 * moose_playlist_length:
 * @self: The #MoosePlaylist to append to.
 *
 * Returns: Length of the #MoosePlaylist.
 */
unsigned moose_playlist_length(MoosePlaylist* self);

/**
 * moose_playlist_sort:
 * @self: The #MoosePlaylist to append to.
 * @func: (scope call): C-Style Comparasion function.
 */
void moose_playlist_sort(MoosePlaylist* self, GCompareFunc func);

/**
 * moose_playlist_at:
 * @self: A #MoosePlaylist
 * @at: Index of the element to get.
 *
 * Bounds check are performed, bad indices emit warnings.
 *
 * Returns: (transfer none): An appended object.
 */
MooseSong* moose_playlist_at(MoosePlaylist* self, unsigned at);

/**
 * moose_playlist_copy:
 * @self: A #MoosePlaylist
 *
 * Returns: (transfer full): A #MoosePlaylist, being an exact clone of @self.
 */
MoosePlaylist* moose_playlist_copy(MoosePlaylist* self);

/**
 * moose_playlist_unref:
 * @self: A #MoosePlaylist
 *
 * Unrefs an playlist.
 */
void moose_playlist_unref(MoosePlaylist* self);

G_END_DECLS

#endif /* end of include guard: MOOSE_STORE_STACK_H */
