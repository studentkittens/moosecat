#ifndef MOOSE_STORE_STACK_H
#define MOOSE_STORE_STACK_H

#include <glib-object.h>

G_BEGIN_DECLS

/*
 * Type macros.
 */
#define MOOSE_TYPE_PLAYLIST \
    (moose_playlist_get_type ())
#define MOOSE_PLAYLIST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOOSE_TYPE_PLAYLIST, MoosePlaylist))
#define MOOSE_IS_PLAYLIST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOOSE_TYPE_PLAYLIST))
#define MOOSE_PLAYLIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MOOSE_TYPE_PLAYLIST, MoosePlaylistClass))
#define MOOSE_IS_PLAYLIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MOOSE_TYPE_PLAYLIST))
#define MOOSE_PLAYLIST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MOOSE_TYPE_PLAYLIST, MoosePlaylistClass))


struct _MoosePlaylistPrivate;

typedef struct _MoosePlaylist {
    GObject parent;      
    struct _MoosePlaylistPrivate *priv;
} MoosePlaylist;


typedef struct _MoosePlaylistClass {
  GObjectClass parent_class;
} MoosePlaylistClass;
    

GType moose_playlist_get_type(void);

/**
 * @brief Create a new stack
 *
 * @param size_hint how many items to preallocate?
 * @param free_func call this on each element on free.
 *
 * @return a newly allocated stack
 */
MoosePlaylist *moose_playlist_new(long size_hint, GDestroyNotify free_func);

/**
 * @brief Append a new element to the stack.
 *
 * Allocates memory in case.
 *
 * @param self the stack to operate on
 * @param ptr the addr to add
 */
void moose_playlist_append(MoosePlaylist *self, void *ptr);

/**
 * @brief Clear contents of stack totally.
 *
 * Calls free_func if passed on _create()
 *
 * @param self the stack to operate on
 */
void moose_playlist_clear(MoosePlaylist *self);

/**
 * @brief Calculates the length of the stack
 *
 * @param self the stack to operate on
 *
 * @return the length from 0 - UINT_MAX
 */
unsigned moose_playlist_length(MoosePlaylist *self);

/**
 * @brief Sort the stack.
 *
 * @param self the stack to operate on.
 * @param func a GCompareFunc.
 */
void moose_playlist_sort(MoosePlaylist *self, GCompareFunc func);

/**
 * @brief Access elememts indexed.
 *
 * For performance reasons, this is an inline macro.
 *
 * @param self the stack to operate on
 * @param at an integer from 0 - moose_playlist_length()
 *
 * @return a void* being at that place.
 */
void *moose_playlist_at(MoosePlaylist *self, unsigned at);


/**
 * @brief Copy the stack pointed to by the argument.
 *
 * @return A newly allocated, but identical stack.
 */
MoosePlaylist * moose_playlist_copy(MoosePlaylist *self);

G_END_DECLS

#endif /* end of include guard: MOOSE_STORE_STACK_H */

