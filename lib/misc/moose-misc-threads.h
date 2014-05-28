#ifndef MOOSE_METADATA_THREADS_H
#define MOOSE_METADATA_THREADS_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* Helper for starting long jobs (like metadata retrieval) in native threads.
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
 * @brief Create a new MetadataThreads Pool.
 *
 * @param max_threads Maximum number of threads to instance.
 *
 * @return
 */
MooseThreads * moose_threads_new(int max_threads);

/**
 * @brief Push a new job to the Pool.
 *
 * @param self The Pool to call on.
 * @param data The data package to pass in.
 */
void moose_threads_push(MooseThreads * self, void * data);

/**
 * @brief This is useful for pushing individual results to the mainloop early.
 *
 * For usescases when the results come in one by one with some delay inbetween.
 * To be called from the thread-callback.
 *
 * When the result arrives, the deliver callback is called.
 *
 * @param self The Pool.
 * @param result the data to forward to the mainthread.
 */
void moose_threads_forward(MooseThreads * self, void * result);

/**
 * @brief Free the Pool.
 *
 * @param self Well, guess.
 */
void moose_threads_unref(MooseThreads * self);

G_END_DECLS

#endif /* end of include guard: MOOSE_METADATA_THREADS_H */
