/*
 * Original source by:
 *    Copyright (C) 2007 Jonathan Matthew
 *
 * Modified:
 *    Copyright (C) 2012 Christopher Pahl
 */

#include "gasyncqueue-watch.h"

/**
 * @brief: GSource for watching a GAsyncQueue in the main loop
 */
typedef struct
{
    GSource source;
    GAsyncQueue *queue;
    gint iteration_timeout;
} MCAsyncQueueWatch;

//////////////////////

static gboolean mc_async_queue_watch_check (GSource *source)
{
    MCAsyncQueueWatch *watch = (MCAsyncQueueWatch *) source;
    return (g_async_queue_length (watch->queue) > 0);
}

//////////////////////

static gboolean mc_async_queue_watch_prepare (GSource *source, gint *timeout)
{
    MCAsyncQueueWatch * watch = (MCAsyncQueueWatch *) source;
    *timeout = watch->iteration_timeout;
    return mc_async_queue_watch_check (source);
}

//////////////////////

static gboolean mc_async_queue_watch_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
    MCAsyncQueueWatch *watch = (MCAsyncQueueWatch *) source;
    MCAsyncQueueWatchFunc cb = (MCAsyncQueueWatchFunc) callback;

    if (cb == NULL)
        return FALSE;
    else
        return cb (watch->queue, user_data);
}

//////////////////////

static void mc_async_queue_watch_finalize (GSource *source)
{
    MCAsyncQueueWatch *watch = (MCAsyncQueueWatch *) source;

    if (watch->queue != NULL)
    {
        g_async_queue_unref (watch->queue);
        watch->queue = NULL;
    }
}

//////////////////////

static GSourceFuncs mc_async_queue_watch_funcs =
{
    mc_async_queue_watch_prepare,
    mc_async_queue_watch_check,
    mc_async_queue_watch_dispatch,
    mc_async_queue_watch_finalize,
    NULL,
    NULL
};

//////////////////////

guint mc_async_queue_watch_new (GAsyncQueue *queue,
                                gint iteration_timeout,
                                MCAsyncQueueWatchFunc callback,
                                gpointer user_data,
                                GMainContext *context)
{
    GSource * source = g_source_new (&mc_async_queue_watch_funcs, sizeof (MCAsyncQueueWatch) );
    MCAsyncQueueWatch * watch = (MCAsyncQueueWatch *) source;
    watch->queue = g_async_queue_ref (queue);
    watch->iteration_timeout = iteration_timeout < 0 ? 30 : CLAMP (iteration_timeout, 5, 500);

    g_source_set_priority (source, G_PRIORITY_DEFAULT);

    g_source_set_callback (source, (GSourceFunc) callback, user_data, NULL);
    guint id = g_source_attach (source, context == NULL ? g_main_context_default() : context);
    g_source_unref (source);
    return id;
}
