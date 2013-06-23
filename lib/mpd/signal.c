#include "signal.h"
#include "protocol.h"

#include "../util/gasyncqueue-watch.h"

// memset
#include <string.h>

///////////////////////////////
////////// PRIVATE ////////////
///////////////////////////////

typedef struct {
    mc_ClientEventCallback callback;
    enum mpd_idle mask;
    gpointer user_data;
} mc_SignalTag;


typedef struct {
    mc_Client *client;
    const char *signal_name;
    const char *error_msg;
    int client_event;
    int log_level;
    int server_changed;
    bool free_log_message;
} mc_DispatchTag;

static const char *signal_to_name_table[] = {
    [MC_SIGNAL_CLIENT_EVENT] = "client-event",
    [MC_SIGNAL_CONNECTIVITY] = "connectivity",
    [MC_SIGNAL_LOGGING]      = "logging",
    [MC_SIGNAL_FATAL_ERROR]  = "_fatal_error",
    [MC_SIGNAL_UNKNOWN]      = NULL
};


static mc_SignalType mc_convert_name_to_signal(const char *signame)
{
    if (signame != NULL)
        for (int i = 0; i < MC_SIGNAL_VALID_COUNT; i++)
            if (g_strcmp0(signal_to_name_table[i], signame) == 0)
                return i;

    return MC_SIGNAL_UNKNOWN;
}

///////////////////////////////

#define DISPATCH_START                                     \
    for (GList * iter = cb_list; iter; iter = iter->next)  \
    {                                                      \
        mc_SignalTag * tag = iter->data;                   \
        if (tag != NULL)                                   \
        {                                                  \
 
#define DISPATCH_END                                       \
        }                                                  \
    }                                                      \
 
void mc_signal_list_report_event_v(mc_SignalList *list, const char *signal_name, mc_DispatchTag *data)
{
    mc_SignalType sig_type = mc_convert_name_to_signal(signal_name);
    if (sig_type == MC_SIGNAL_UNKNOWN)
        return;

    GList *cb_list = list->signals[sig_type];

    g_printerr("GOT: %s\n", signal_name);

    /* All callbacks get a client as first param */
    switch (sig_type) {
        case MC_SIGNAL_CLIENT_EVENT: {
            DISPATCH_START {
                if (tag->mask & data->client_event) {
                    ((mc_ClientEventCallback) tag->callback)(
                        data->client, data->client_event, tag->user_data
                    );
                }
            }
            DISPATCH_END
            break;
        }
        case MC_SIGNAL_LOGGING: {
            DISPATCH_START {
                ((mc_LoggingCallback) tag->callback)(
                    data->client,
                    data->error_msg,
                    data->log_level,
                    tag->user_data
                );
            }
            DISPATCH_END

            if(data->free_log_message) {
                g_free((char *)data->error_msg);
            }
            break;
        }
        case MC_SIGNAL_CONNECTIVITY: {
            DISPATCH_START {
                ((mc_ConnectivityCallback) tag->callback)(
                    data->client,
                    data->server_changed, 
                    tag->user_data
                );
            }
            DISPATCH_END
            break;
        }
        case MC_SIGNAL_FATAL_ERROR: {
            g_print("Calling disconnect on main thread. %p %p\n", data, data->client);
            mc_proto_disconnect(data->client);
        }
        default:  {
            /* Should not happen */
            break;
        }
    }
}

///////////////////////////////


mc_DispatchTag * mc_signal_list_unpack_valist(const char *signal_name, va_list args)
{
    mc_SignalType sig_type = mc_convert_name_to_signal(signal_name);

    if (sig_type == MC_SIGNAL_UNKNOWN) {
        g_print("Signal Name: '%s' is unknown.\n", signal_name);
        return NULL;
    }
    
    mc_DispatchTag *tag = g_slice_new0(mc_DispatchTag);
    tag->signal_name = signal_name;

    /* All callbacks get a client as first param */
    tag->client = va_arg(args, struct mc_Client *);

    switch (sig_type) {
        case MC_SIGNAL_CLIENT_EVENT: {
            tag->client_event = va_arg(args, int);
            break;
        }
        case MC_SIGNAL_LOGGING: {
            tag->error_msg = va_arg(args, const char *);
            tag->log_level = va_arg(args, int);
            tag->free_log_message = va_arg(args, int);
            break;
        }
        case MC_SIGNAL_CONNECTIVITY: {
            tag->server_changed = va_arg(args, int);
            break;
        }
        default: {
            /* Nothing to unpack */
            break;
        }
    }

    return tag;
}

///////////////////////////////

static gboolean mc_signal_list_dispatch_cb(GAsyncQueue * queue, gpointer user_data)
{
    mc_SignalList * list = user_data;
    
    mc_DispatchTag * tag = NULL;
    mc_DispatchTag * client_event_buffer = NULL;

    while((tag = g_async_queue_try_pop(queue))) {
        /* Merge all clients event to a Single Bitmask.
         * This prevents unnecessart signal emission 
         * causing all the client stuff to update.
         */
        if(tag->client_event != 0) {
            if(client_event_buffer == NULL) {
                client_event_buffer = tag;
            } 
            g_printerr("Merging %d on %d\n", client_event_buffer->client_event, tag->client_event);
            client_event_buffer->client_event |= tag->client_event;
        } else {
            /* Other signals can be reported immediately */
            mc_signal_list_report_event_v(list, tag->signal_name, tag);
        }

        if(client_event_buffer != tag) {
            g_slice_free(mc_DispatchTag, tag);
        }
    }
         
    /* Report only one client event - if one actually happened */
    if(client_event_buffer != NULL) {
        g_printerr("Client event with %d\n", client_event_buffer->client_event);
        mc_signal_list_report_event_v(
                list, client_event_buffer->signal_name, client_event_buffer
        );
        g_slice_free(mc_DispatchTag, client_event_buffer);
    }
    
    return TRUE;
}

///////////////////////////////
////////// PUBLIC /////////////
///////////////////////////////

void  mc_signal_list_init(mc_SignalList *list)
{
    if (list == NULL)
        return;

    memset(list, 0, sizeof(mc_SignalList));

    list->dispatch_queue = g_async_queue_new();
    list->signal_watch_id = mc_async_queue_watch_new(
            list->dispatch_queue,       /* Queue to watch                             */
            -1,                         /* Let GAsyncQueueWatch determine the timeout */
            mc_signal_list_dispatch_cb, /* Callback to be called                      */
            list,                       /* User data                                  */
            NULL                        /* Default MainLoop Context                   */
    );

    /* Set the target dispatch thread to the current one.
     * We need this for later comparasion */
    list->initial_thread = g_thread_self();
}

///////////////////////////////

void mc_signal_add_masked(
    mc_SignalList   *list,
    const char *signal_name,
    bool call_first,
    void *callback_func,
    void *user_data,
    enum mpd_idle mask)
{
    if (list == NULL || callback_func == NULL)
        return;

    mc_SignalTag *tag = g_slice_new0(mc_SignalTag);
    tag->user_data = user_data;
    tag->callback = callback_func;
    tag->mask = mask;

    if (tag != NULL) {
        mc_SignalType type = mc_convert_name_to_signal(signal_name);

        if (type != MC_SIGNAL_UNKNOWN) {
            if (call_first)
                list->signals[type] = g_list_prepend(list->signals[type], tag);
            else
                list->signals[type] = g_list_append(list->signals[type], tag);
        }
    }
}

///////////////////////////////

void mc_signal_add(
    mc_SignalList   *list,
    const char *signal_name,
    bool call_first,
    void *callback_func,
    void *user_data)
{
    mc_signal_add_masked(list, signal_name, call_first, callback_func, user_data, INT_MAX);
}

///////////////////////////////

int mc_signal_length(
    mc_SignalList *list,
    const char *signal_name)
{
    mc_SignalType type = mc_convert_name_to_signal(signal_name);

    if (type != MC_SIGNAL_UNKNOWN)
        return (list) ? (int) g_list_length(list->signals[type]) : -1;
    else
        return -1;
}

///////////////////////////////

void mc_signal_rm(
    mc_SignalList   *list,
    const char *signal_name,
    void *callback_addr)
{
    mc_SignalType type = mc_convert_name_to_signal(signal_name);

    if (type != MC_SIGNAL_UNKNOWN) {
        GList *head = list->signals[type];

        for (GList *iter = head; iter; iter = iter->next) {
            mc_SignalTag *tag = iter->data;

            if (tag != NULL) {
                if (tag->callback == callback_addr) {
                    list->signals[type] = g_list_delete_link(head, iter);
                    g_slice_free(mc_SignalTag, tag);
                    return;
                }
            }
        }
    }
}

///////////////////////////////

void mc_signal_report_event_v(mc_SignalList *list, const char *signal_name, va_list args)
{
    g_assert(list);

    mc_DispatchTag *data_tag = mc_signal_list_unpack_valist(signal_name, args);
    if(data_tag == NULL)
        return;

    /* All signals are currently pushed into a queue.
     * even if they are already on the Main-Thread.
     * This guarantess that the dispatch happens always on the main thread.
     * 
     * This decision was done to be able to "buffer" events.
     * Client-Events are pulled after a timeout or after too many events
     * out of the queue and are merged into a single one. 
     *
     * Sliding the Volume Slider for example causes much less traffic this way.
     */
    g_async_queue_push(list->dispatch_queue, data_tag);
}

///////////////////////////////

void mc_signal_report_event(mc_SignalList *list, const char *signal_name, ...)
{
    va_list args;
    va_start(args, signal_name);
    mc_signal_report_event_v(list, signal_name, args);
    va_end(args);
}

///////////////////////////////

void mc_signal_list_destroy(mc_SignalList *list)
{
    if (list != NULL) {
        for (int i = 0; i < MC_SIGNAL_VALID_COUNT; i++) {
            for (GList *iter = list->signals[i]; iter; iter = iter->next) {
                mc_SignalTag *tag = iter->data;
                g_slice_free(mc_SignalTag, tag);
            }

            g_list_free(list->signals[i]);
        }

        g_async_queue_unref(list->dispatch_queue);
    }
}
