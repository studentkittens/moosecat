/*
 * Original source by:
 *    Copyright (C) 2007 Jonathan Matthew
 *
 * Modified:
 *    Copyright (C) 2012 Christopher Pahl
 *
 * Modifications:
 *    - rename to conventions / formatting
 *    - do not pop items from queue,
 *      just pass the queue as a whole
 *    - timeout-optimizations
 *    - misc changes on the API
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
    GTimer * iteration_timer;
    guint timeout_min;
    guint timeout_max;
} MCAsyncQueueWatch;

//////////////////////

static gint mc_async_queue_watch_calc_next_timeout (
    gint prev_timeout,
    gdouble x,
    guint min,
    guint max)
{
    /* Eq. Solved for:
     *
     * A = (0, max)
     * B = (1, min)
     * C = (5, (max + min) / 2)
     */

    if (x < 4.0)
    {
        gdouble a = (-9. * min + 9.  * max) / 40.;
        gdouble b = (49. * min - 49. * max) / 40.;
        gdouble c = max;

        return (prev_timeout + 2 * (a * (x * x) + b * x + c) ) / 3.;
    }
    else
    {
        return 30;
    }
}

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

    /* Calculate next timeout */
    watch->iteration_timeout = mc_async_queue_watch_calc_next_timeout (
                                   watch->iteration_timeout,
                                   g_timer_elapsed (watch->iteration_timer, NULL),
                                   watch->timeout_min,
                                   watch->timeout_max
                               );

    g_print ("New timeout: %d\n", watch->iteration_timeout);

    /* Reset the timeout */
    g_timer_start (watch->iteration_timer);

    /* Call the callback, or shutdown yourself,
     * since life without a callback is pointless */
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
        g_timer_destroy (watch->iteration_timer);
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

    watch->timeout_min = 1;
    watch->timeout_max = 60;
    watch->iteration_timeout = 10;

    watch->iteration_timer = g_timer_new();
    watch->queue = g_async_queue_ref (queue);
    watch->iteration_timeout =
        (iteration_timeout < 0) ?
        30 :
        CLAMP (iteration_timeout, 5, 500);

    g_source_set_callback (source, (GSourceFunc) callback, user_data, NULL);
    guint id = g_source_attach (source, context == NULL ? g_main_context_default() : context);
    g_source_unref (source);

    return id;
}
