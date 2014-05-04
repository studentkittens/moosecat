#ifndef MC_STORE_STACK_H
#define MC_STORE_STACK_H

#include <glib.h>

/**
 * @brief A stack.
 *
 * It is basically a GPtrArray, but
 * you should not assume this, nor
 * access the members...
 */
typedef struct {
    GPtrArray *stack;
    GDestroyNotify free_func;
} mc_Playlist;

/**
 * @brief Create a new stack
 *
 * @param size_hint how many items to preallocate?
 * @param free_func call this on each element on free.
 *
 * @return a newly allocated stack
 */
mc_Playlist *mc_stack_create(long size_hint, GDestroyNotify free_func);

/**
 * @brief Append a new element to the stack.
 *
 * Allocates memory in case.
 *
 * @param self the stack to operate on
 * @param ptr the addr to add
 */
void mc_stack_append(mc_Playlist *self, void *ptr);

/**
 * @brief Free the stack
 *
 * If you passed a free_func, it will
 * called on each element now.
 *
 * @param self the stack to operate on
 */
void mc_stack_free(mc_Playlist *self);

/**
 * @brief Clear contents of stack totally.
 *
 * Calls free_func if passed on _create()
 *
 * @param self the stack to operate on
 */
void mc_stack_clear(mc_Playlist *self);

/**
 * @brief Calculates the length of the stack
 *
 * @param self the stack to operate on
 *
 * @return the length from 0 - UINT_MAX
 */
unsigned mc_stack_length(mc_Playlist *self);

/**
 * @brief Sort the stack.
 *
 * @param self the stack to operate on.
 * @param func a GCompareFunc.
 */
void mc_stack_sort(mc_Playlist *self, GCompareFunc func);

/**
 * @brief Access elememts indexed.
 *
 * For performance reasons, this is an inline macro.
 *
 * @param self the stack to operate on
 * @param at an integer from 0 - mc_stack_length()
 *
 * @return a void* being at that place.
 */
void *mc_stack_at(mc_Playlist *self, unsigned at);


/**
 * @brief Copy the stack pointed to by the argument.
 *
 * @return A newly allocated, but identical stack.
 */
mc_Playlist * mc_stack_copy(mc_Playlist *self);

#endif /* end of include guard: MC_STORE_STACK_H */

