#ifndef MOOSE_METADATA_THREADS_H
#define MOOSE_METADATA_THREADS_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * SECTION: moose-misc-threads
 * @short_description: Easily dispatch time-consuming tasks in other threads.
 *
 * Helper for starting long jobs (like metadata retrieval) in native threads.
 * The finished result gets pushed into a queue. The queue is emptied
 * (after a short amount of time) on the mainthread - which is needed to protect
 * the python part from race conditions.
 *
 * On thread dispatch and mainthread dispatch a callback is called.
 */

/*
 * Type macros.
 */
#define MOOSE_TYPE_THREADS \
    (moose_threads_get_type())
#define MOOSE_THREADS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_THREADS, MooseThreads))
#define MOOSE_IS_THREADS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_THREADS))
#define MOOSE_THREADS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_THREADS, MooseThreadsClass))
#define MOOSE_IS_THREADS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_THREADS))
#define MOOSE_THREADS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_THREADS, MooseThreadsClass))

GType moose_threads_get_type(void);

struct _MooseThreadsPrivate;

typedef struct _MooseThreads {
    GObject parent;
    struct _MooseThreadsPrivate *priv;
} MooseThreads;

typedef struct _MooseThreadsClass {
    GObjectClass parent;
} MooseThreadsClass;

/**
 * moose_threads_new:
 * @max_threads: Maximum number of threads to spawn on demand.
 *
 * Returns: A newly allocated threads-helper, with a refcount of 1
 */
MooseThreads * moose_threads_new(int max_threads);

/**
 * moose_threads_push:
 * @self: a #MooseThreads
 * @data: An arbitary pointer. No ownership is taken.
 *
 * Push some data to a thread pool. A thread is started and the
 * data is delivered to it. Use the 'thread' signal to connect to it
 * and to pass aditional per-signal user-data.
 */
void moose_threads_push(MooseThreads * self, void * data);

/**
 * moose_threads_forward:
 * @self: a #MooseThreads
 * @result: An arbitary pointer. No ownership is taken.
 *
 * Forward results to the main-thread. You're supposed to call this
 * function from a thread. Once the result arrives on the main-thread,
 * the 'dispatch' signal is called.
 */
void moose_threads_forward(MooseThreads * self, void * result);

/**
 * moose_threads_unref:
 * @self: a #MooseThreads
 *
 * Unrefs the #MooseThreads
 */
void moose_threads_unref(MooseThreads * self);

G_END_DECLS

#endif /* end of include guard: MOOSE_METADATA_THREADS_H */
