#include "moose-misc-metadata-threads.h"

////////////////////////
// INTERNAL CALLBACKS //
////////////////////////

static void moose_mdthreads_dispatch(gpointer data, gpointer user_data)
{
    g_assert(user_data);

    MooseMetadataThreads * self = user_data;

    /* Probably some python object, just pass it */
    gpointer result = self->thread_callback(self, data, self->user_data);
    moose_mdthreads_forward(self, result);
}

///////////////////////

static gboolean moose_mdthreads_mainloop_callback(gpointer user_data)
{
    g_assert(user_data);

    gpointer result = NULL;
    MooseMetadataThreads * self = user_data;

    /* It's possible that there happended more events */
    while ((result = g_async_queue_try_pop(self->queue)) != NULL) {
        self->deliver_callback(self, result, self->user_data);
    }

    return FALSE;
}

///////////////////////
// PUBLIC INTERFACE  //
///////////////////////

MooseMetadataThreads * moose_mdthreads_new(
    MooseMetadataCallback thread_callback,
    MooseMetadataCallback deliver_callback,
    void * user_data,
    int max_threads
    )
{
    /* New Instance */
    MooseMetadataThreads * self = g_new0(MooseMetadataThreads, 1);

    /* Callbacks */
    self->thread_callback = thread_callback;
    self->deliver_callback = deliver_callback;
    self->user_data = user_data;

    /* Allocated data */
    GError * error = NULL;
    self->queue = g_async_queue_new();
    self->pool = g_thread_pool_new(
        moose_mdthreads_dispatch,
        self,
        max_threads,
        FALSE,
        &error
        );

    /* Be nice and check for the error */
    if (error != NULL) {
        g_printerr("Moosecat-Internal: Failed to instance GThreadPool: %s\n",
                   error->message
                   );

        g_error_free(error);
    }

    return self;
}

///////////////////////

void moose_mdthreads_push(MooseMetadataThreads * self, void * data)
{
    g_assert(self);

    /* Check if not destroyed yet */
    if (self->pool != NULL) {
        g_thread_pool_push(self->pool, data, NULL);
    }

}

///////////////////////

void moose_mdthreads_forward(MooseMetadataThreads * self, void * result)
{
    if (result != NULL) {
        g_async_queue_push(self->queue, result);
        g_idle_add(moose_mdthreads_mainloop_callback, self);
    }

}

///////////////////////

void moose_mdthreads_free(MooseMetadataThreads * self)
{
    g_assert(self);

    /* Stop the ThreadPool */
    g_thread_pool_free(self->pool, TRUE, FALSE);
    self->pool = NULL;

    /* Get rid of the Queue */
    g_async_queue_unref(self->queue);

    /* suicide */
    g_free(self);
}


#if 0

////////////////////////
// TEST MAIN FUNCTION //
////////////////////////

static void * thread(struct _MooseMetadataThreads * self, void * data, void * user_data)
{
    /* 1 second */
    g_printerr("THREAD %p\n", data);
    g_usleep(1 * 1000 * 1000);
    g_printerr("THREAD END %p\n", data);
    return data;
}


static void * deliver(struct _MooseMetadataThreads * self, void * data, void * user_data)
{
    g_printerr("Delivered: %p\n", data);
    return NULL;
}

static gboolean timeout_cb(gpointer user_data)
{
    GMainLoop * loop = user_data;
    g_printerr("LOOP STOP\n");
    g_main_loop_quit(loop);
    return FALSE;
}

int main(int argc, char const * argv[])
{
    MooseMetadataThreads * self = moose_mdthreads_new(thread, deliver, NULL, 10);

    for (int i = 0; i < 10; ++i) {
        moose_mdthreads_push(self, (void *)(long)(i + 1));
    }

    GMainLoop * loop = g_main_loop_new(NULL, TRUE);
    g_timeout_add(5000, timeout_cb, loop);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    moose_mdthreads_free(self);
    return 0;
}

#endif
