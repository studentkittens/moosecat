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
} MoosePlaylist;

/**
 * @brief Create a new stack
 *
 * @param size_hint how many items to preallocate?
 * @param free_func call this on each element on free.
 *
 * @return a newly allocated stack
 */
MoosePlaylist *moose_stack_create(long size_hint, GDestroyNotify free_func);

/**
 * @brief Append a new element to the stack.
 *
 * Allocates memory in case.
 *
 * @param self the stack to operate on
 * @param ptr the addr to add
 */
void moose_stack_append(MoosePlaylist *self, void *ptr);

/**
 * @brief Free the stack
 *
 * If you passed a free_func, it will
 * called on each element now.
 *
 * @param self the stack to operate on
 */
void moose_stack_free(MoosePlaylist *self);

/**
 * @brief Clear contents of stack totally.
 *
 * Calls free_func if passed on _create()
 *
 * @param self the stack to operate on
 */
void moose_stack_clear(MoosePlaylist *self);

/**
 * @brief Calculates the length of the stack
 *
 * @param self the stack to operate on
 *
 * @return the length from 0 - UINT_MAX
 */
unsigned moose_stack_length(MoosePlaylist *self);

/**
 * @brief Sort the stack.
 *
 * @param self the stack to operate on.
 * @param func a GCompareFunc.
 */
void moose_stack_sort(MoosePlaylist *self, GCompareFunc func);

/**
 * @brief Access elememts indexed.
 *
 * For performance reasons, this is an inline macro.
 *
 * @param self the stack to operate on
 * @param at an integer from 0 - moose_stack_length()
 *
 * @return a void* being at that place.
 */
void *moose_stack_at(MoosePlaylist *self, unsigned at);


/**
 * @brief Copy the stack pointed to by the argument.
 *
 * @return A newly allocated, but identical stack.
 */
MoosePlaylist * moose_stack_copy(MoosePlaylist *self);

#endif /* end of include guard: MC_STORE_STACK_H */

