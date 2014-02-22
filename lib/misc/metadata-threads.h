#ifndef MC_METADATA_THREADS_H
#define MC_METADATA_THREADS_H

#include <glib.h>

/* Helper for starting long jobs (like metadata retrieval) in native threads.
 * The finished result gets pushed into a queue. The queue is emptied 
 * (after a short amount of time) on the mainthread - which is needed to protect
 * the python part from race conditions.
 *
 * On thread dispatch and mainthread dispatch a callback is called.
 */ 

struct _mc_MetadataThreads;

typedef void * (* mc_MetadataCallback)(
    struct _mc_MetadataThreads *, /* self      */
    void *,                       /* data      */
    void *                        /* user_data */
);

///////////////////////

typedef struct _mc_MetadataThreads {
    /* Thread Pool to distribute the jobs */
    GThreadPool * pool;

    /* Queue to pass back the results to the mainloop */
    GAsyncQueue * queue;

    /* ID of the GAsyncQueueWatch */
    gint watch;

    /* Callback called when the thread is ready
     * Call your blockings calls here.
     * */
    mc_MetadataCallback thread_callback;

    /* Callback called when the thread had a result
     * and the result is available on the main thread.
     */
    mc_MetadataCallback deliver_callback;
    gpointer user_data;
} mc_MetadataThreads;

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
mc_MetadataThreads * mc_mdthreads_new(
    mc_MetadataCallback thread_callback,
    mc_MetadataCallback deliver_callback,
    void * user_data,
    int max_threads
);


/**
 * @brief Push a new job to the Pool.
 *
 * @param self The Pool to call on.
 * @param data The data package to pass in.
 */
void mc_mdthreads_push(mc_MetadataThreads * self, void * data);

/**
 * @brief Free the Pool.
 *
 * @param self Well, guess.
 */
void mc_mdthreads_free(mc_MetadataThreads * self);

#endif /* end of include guard: MC_METADATA_THREADS_H */

