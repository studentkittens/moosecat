#ifndef MOOSE_METADATA_THREADS_H
#define MOOSE_METADATA_THREADS_H

#include <glib.h>

/* Helper for starting long jobs (like metadata retrieval) in native threads.
 * The finished result gets pushed into a queue. The queue is emptied 
 * (after a short amount of time) on the mainthread - which is needed to protect
 * the python part from race conditions.
 *
 * On thread dispatch and mainthread dispatch a callback is called.
 */ 

struct _MooseMetadataThreads;

typedef void * (* MooseMetadataCallback)(
    struct _MooseMetadataThreads *, /* self      */
    void *,                       /* data      */
    void *                        /* user_data */
);

///////////////////////

typedef struct _MooseMetadataThreads {
    /* Thread Pool to distribute the jobs */
    GThreadPool * pool;

    /* Queue to pass back the results to the mainloop */
    GAsyncQueue * queue;

    /* ID of the GAsyncQueueWatch */
    gint watch;

    /* Callback called when the thread is ready
     * Call your blockings calls here.
     * */
    MooseMetadataCallback thread_callback;

    /* Callback called when the thread had a result
     * and the result is available on the main thread.
     */
    MooseMetadataCallback deliver_callback;
    
    /* User data for *both* callbacks 
     */
    gpointer user_data;
} MooseMetadataThreads;

///////////////////////

/**
 * @brief Create a new MetadataThreads Pool.
 *
 * @param thread_callback Callback to be called on ready threads. Do your
 * blocking calls here.
 * @param deliver_callback Called when the thread's result arrived in the
 * mainthread.
 * @param user_data Data passed to both callbacks.
 * @param max_threads Maximum number of threads to instance.
 *
 * @return 
 */
MooseMetadataThreads * moose_mdthreads_new(
    MooseMetadataCallback thread_callback,
    MooseMetadataCallback deliver_callback,
    void * user_data,
    int max_threads
);


/**
 * @brief Push a new job to the Pool.
 *
 * @param self The Pool to call on.
 * @param data The data package to pass in.
 */
void moose_mdthreads_push(MooseMetadataThreads * self, void * data);

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
void moose_mdthreads_forward(MooseMetadataThreads * self, void * result);

/**
 * @brief Free the Pool.
 *
 * @param self Well, guess.
 */
void moose_mdthreads_free(MooseMetadataThreads * self);

#endif /* end of include guard: MOOSE_METADATA_THREADS_H */

