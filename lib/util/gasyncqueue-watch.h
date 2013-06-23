#ifndef GASYNCQUEUE_WATCH_H_GUARD
#define GASYNCQUEUE_WATCH_H_GUARD

#include <glib.h>

/**
 * @brief Callback that is called once an item is pushed into the associated GAsyncQueue
 *
 * @param queue the async queue, use it to pop data from it.
 * @param data provided userdata
 *
 * @return TRUE if you'd like to be notified for subsequent events.
 */
typedef gboolean(* MCAsyncQueueWatchFunc)(GAsyncQueue *, gpointer);

/**
 * mc_async_queue_watch_new:
 * @param queue the #GAsyncQueue to watch
 * @param iteration_timeout max timeout for mainloop's poll(),
 *                          high timeouts are ressource friendly, but less responsive.
 *                          low timeouts respond immediately, but consume more cycles
 *                          and may issue a callback for every event.
 * @param callback callback to invoke when the queue is non-empty
 * @param user_data user data to pass to the callback
 * @param context the #GMainContext to attach the source to
 *
 * Creates a new #GSource that triggers when the #GAsyncQueue is
 * non-empty.  This is used in rhythmbox to process queues within
 * #RhythmDB in the main thread without polling.
 *
 * @return the ID of the new #GSource
 */
guint mc_async_queue_watch_new(GAsyncQueue *queue,
                               gint iteration_timeout,
                               MCAsyncQueueWatchFunc callback,
                               gpointer user_data,
                               GMainContext *context);

#endif /* end of include guard: GASYNCQUEUE_WATCH_H_GUARD */
