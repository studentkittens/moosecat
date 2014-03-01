/* Own header */
#include "metadata-threads.h"

/* mc_async_queue_watch_new */
#include "../util/gasyncqueue-watch.h"

////////////////////////
// INTERNAL CALLBACKS //
////////////////////////

static void mc_mdthreads_dispatch(gpointer data, gpointer user_data)
{
    g_assert(user_data);

    mc_MetadataThreads * self = user_data;

    /* Probably some python object, just pass it */
    gpointer result = self->thread_callback(self, data, self->user_data);
    mc_mdthreads_forward(self, result);
}

///////////////////////

static gboolean mc_mdthreads_mainloop_callback(GAsyncQueue * queue, gpointer user_data)
{
    g_assert(queue);
    g_assert(user_data);

    void * result = NULL;
    mc_MetadataThreads * self = user_data;
    
    /* It's possible that there happended more events */
    while((result = g_async_queue_try_pop(queue)) != NULL) {
        self->deliver_callback(self, result, self->user_data);
    }

    return TRUE;
}

///////////////////////
// PUBLIC INTERFACE  //
///////////////////////

mc_MetadataThreads * mc_mdthreads_new(
    mc_MetadataCallback thread_callback,
    mc_MetadataCallback deliver_callback,
    void * user_data,
    int max_threads
) 
{
    /* New Instance */
    mc_MetadataThreads * self = g_new0(mc_MetadataThreads, 1);

    /* Callbacks */
    self->thread_callback = thread_callback;
    self->deliver_callback= deliver_callback;
    self->user_data = user_data;

    /* Allocated data */
    GError * error = NULL;
    self->queue = g_async_queue_new();
    self->pool = g_thread_pool_new(
        mc_mdthreads_dispatch,
        self,
        max_threads,
        FALSE,
        &error
    );

    /* Watch for the Queue, returns an ID */
    self->watch = mc_async_queue_watch_new(
        self->queue, 
        100,
        mc_mdthreads_mainloop_callback, 
        self,
        NULL
    );

    /* Be nice and check for the error */
    if(error != NULL) {
        g_printerr("Moosecat-Internal: Failed to instance GThreadPool: %s\n",
            error->message
        );
        
        g_error_free(error);
    }

    return self;
}

///////////////////////

void mc_mdthreads_push(mc_MetadataThreads * self, void * data)
{
    g_assert(self);

    /* Check if not destroyed yet */
    if(self->watch != -1) {
        g_thread_pool_push(self->pool, data, NULL);
    }

}

///////////////////////

void mc_mdthreads_forward(mc_MetadataThreads * self, void * result)
{
    if(result != NULL) {
        g_async_queue_push(self->queue, result);
    }

}

///////////////////////

void mc_mdthreads_free(mc_MetadataThreads * self) 
{
    g_assert(self);

    /* Make sure no callbacks are called anymore */
    g_source_remove(self->watch);
    self->watch = -1;

    /* Stop the ThreadPool */
    g_thread_pool_free(self->pool, TRUE, FALSE);

    /* Get rid of the Queue */
    g_async_queue_unref(self->queue);

    /* suicide */
    g_free(self);
}


#if 0

////////////////////////
// TEST MAIN FUNCTION //
////////////////////////

static void * thread(struct _mc_MetadataThreads * self, void * data, void * user_data)
{
    /* 1 second */
    g_printerr("THREAD %p\n", data);
    g_usleep(1 * 1000 * 1000);
    g_printerr("THREAD END %p\n", data);
    return data;
}


static void * deliver(struct _mc_MetadataThreads * self, void * data, void * user_data)
{
    g_printerr("Delivered: %p\n", data);
    return NULL;
}

static gboolean timeout_cb(gpointer user_data)
{
    GMainLoop *loop = user_data;
    g_printerr("LOOP STOP\n");
    g_main_loop_quit(loop);
    return FALSE;
}

int main(int argc, char const *argv[])
{
    mc_MetadataThreads * self = mc_mdthreads_new(thread, deliver, NULL, 10);

    for(int i = 0; i < 10; ++i) {
        mc_mdthreads_push(self, (void *)(long)(i + 1));
    }

    GMainLoop *loop = g_main_loop_new(NULL, TRUE);
    g_timeout_add(5000, timeout_cb, loop);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    mc_mdthreads_free(self);
    return 0;
}

#endif
