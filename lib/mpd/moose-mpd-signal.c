#include "moose-mpd-signal.h"
#include "moose-mpd-protocol.h"

/* memset() */
#include <string.h>

///////////////////////////////
////////// PRIVATE ////////////
///////////////////////////////

typedef struct {
    MooseClientEventCallback callback;
    enum mpd_idle mask;
    gpointer user_data;
} MooseSignalTag;


typedef struct {
    MooseClient * client;
    const char * signal_name;
    const char * error_msg;
    int client_event;
    int log_level;
    int is_connected;
    int server_changed;
    bool free_log_message;
} MooseDispatchTag;

static const char * signal_to_name_table[] = {
    [MOOSE_SIGNAL_CLIENT_EVENT] = "client-event",
    [MOOSE_SIGNAL_CONNECTIVITY] = "connectivity",
    [MOOSE_SIGNAL_LOGGING]      = "logging",
    [MOOSE_SIGNAL_FATAL_ERROR]  = "_fatal_error",
    [MOOSE_SIGNAL_UNKNOWN]      = NULL
};


static MooseSignalType moose_convert_name_to_signal(const char * signame)
{
    if (signame != NULL)
        for (int i = 0; i < MOOSE_SIGNAL_VALID_COUNT; i++)
            if (g_strcmp0(signal_to_name_table[i], signame) == 0)
                return i;

    return MOOSE_SIGNAL_UNKNOWN;
}

///////////////////////////////

#define DISPATCH_START                                     \
    for (GList * iter = cb_list; iter; iter = iter->next)  \
    {                                                      \
        MooseSignalTag * tag = iter->data;                   \
        if (tag != NULL)                                   \
        {                                                  \

#define DISPATCH_END                                       \
    }                                                  \
    }                                                      \

void moose_priv_signal_list_report_event_v(MooseSignalList * list, const char * signal_name, MooseDispatchTag * data)
{
    MooseSignalType sig_type = moose_convert_name_to_signal(signal_name);
    if (sig_type == MOOSE_SIGNAL_UNKNOWN)
        return;

    GList * cb_list = list->signals[sig_type];

    /* All callbacks get a client as first param */
    switch (sig_type) {
    case MOOSE_SIGNAL_CLIENT_EVENT: {
        DISPATCH_START {
            if (tag->mask & data->client_event) {
                ((MooseClientEventCallback)tag->callback)(
                    data->client, data->client_event, tag->user_data
                    );
            }
        }
        DISPATCH_END
        break;
    }
    case MOOSE_SIGNAL_LOGGING: {
        DISPATCH_START {
            ((MooseLoggingCallback)tag->callback)(
                data->client,
                data->error_msg,
                data->log_level,
                tag->user_data
                );
        }
        DISPATCH_END

        if (data->free_log_message) {
            g_free((char *)data->error_msg);
        }
        break;
    }
    case MOOSE_SIGNAL_CONNECTIVITY: {
        DISPATCH_START {
            ((MooseConnectivityCallback)tag->callback)(
                data->client,
                data->server_changed,
                data->is_connected,
                tag->user_data
                );
        }
        DISPATCH_END
        break;
    }
    case MOOSE_SIGNAL_FATAL_ERROR: {
        moose_client_disconnect(data->client);
    }
    default:  {
        /* Should not happen */
        break;
    }
    }
}

///////////////////////////////


MooseDispatchTag * moose_priv_signal_list_unpack_valist(const char * signal_name, va_list args)
{
    MooseSignalType sig_type = moose_convert_name_to_signal(signal_name);

    if (sig_type == MOOSE_SIGNAL_UNKNOWN) {
        return NULL;
    }

    MooseDispatchTag * tag = g_slice_new0(MooseDispatchTag);
    tag->signal_name = signal_name;

    /* All callbacks get a client as first param */
    tag->client = va_arg(args, struct MooseClient *);

    switch (sig_type) {
    case MOOSE_SIGNAL_CLIENT_EVENT: {
        tag->client_event = va_arg(args, int);
        break;
    }
    case MOOSE_SIGNAL_LOGGING: {
        tag->error_msg = va_arg(args, const char *);
        tag->log_level = va_arg(args, int);
        tag->free_log_message = va_arg(args, int);
        break;
    }
    case MOOSE_SIGNAL_CONNECTIVITY: {
        tag->server_changed = va_arg(args, int);
        tag->is_connected = va_arg(args, int);
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

static gboolean moose_priv_signal_list_dispatch_cb(gpointer user_data)
{
    MooseSignalList * list = user_data;
    MooseDispatchTag * tag = NULL;
    MooseDispatchTag * client_event_buffer = NULL;

    while ((tag = g_async_queue_try_pop(list->dispatch_queue))) {
        /* Merge all clients event to a Single Bitmask.
         * This prevents unnecessart signal emission
         * causing all the client stuff to update.
         */
        if (tag->client_event != 0) {
            if (client_event_buffer == NULL) {
                client_event_buffer = tag;
            }
            client_event_buffer->client_event |= tag->client_event;
        } else {
            /* Other signals can be reported immediately */
            moose_priv_signal_list_report_event_v(list, tag->signal_name, tag);
        }

        if (client_event_buffer != tag) {
            g_slice_free(MooseDispatchTag, tag);
        }
    }

    /* Report only one client event - if one actually happened */
    if (client_event_buffer != NULL) {
        moose_priv_signal_list_report_event_v(
            list, client_event_buffer->signal_name, client_event_buffer
            );
        g_slice_free(MooseDispatchTag, client_event_buffer);
    }

    return FALSE;
}

///////////////////////////////
////////// PUBLIC /////////////
///////////////////////////////

void  moose_priv_signal_list_init(MooseSignalList * list)
{
    if (list == NULL)
        return;

    memset(list, 0, sizeof(MooseSignalList));

    g_rec_mutex_init(&list->api_mtx);
    list->dispatch_queue = g_async_queue_new();
}

///////////////////////////////

void moose_priv_signal_add_masked(
    MooseSignalList   * list,
    const char * signal_name,
    bool call_first,
    void * callback_func,
    void * user_data,
    enum mpd_idle mask)
{
    if (list == NULL || callback_func == NULL)
        return;

    g_rec_mutex_lock(&list->api_mtx);
    MooseSignalTag * tag = g_slice_new0(MooseSignalTag);
    tag->user_data = user_data;
    tag->callback = callback_func;
    tag->mask = mask;

    if (tag != NULL) {
        MooseSignalType type = moose_convert_name_to_signal(signal_name);

        if (type != MOOSE_SIGNAL_UNKNOWN) {
            if (call_first)
                list->signals[type] = g_list_prepend(list->signals[type], tag);
            else
                list->signals[type] = g_list_append(list->signals[type], tag);
        }
    }
    g_rec_mutex_unlock(&list->api_mtx);
}

///////////////////////////////

void moose_priv_signal_add(
    MooseSignalList   * list,
    const char * signal_name,
    bool call_first,
    void * callback_func,
    void * user_data)
{
    moose_priv_signal_add_masked(list, signal_name, call_first, callback_func, user_data, INT_MAX);
}

///////////////////////////////

int moose_priv_signal_length(
    MooseSignalList * list,
    const char * signal_name)
{
    int result = -1;

    g_rec_mutex_lock(&list->api_mtx);
    MooseSignalType type = moose_convert_name_to_signal(signal_name);
    if (type != MOOSE_SIGNAL_UNKNOWN)
        return (list) ? (int)g_list_length(list->signals[type]) : -1;

    g_rec_mutex_unlock(&list->api_mtx);
    return result;
}

///////////////////////////////

void moose_priv_signal_rm(
    MooseSignalList   * list,
    const char * signal_name,
    void * callback_addr)
{
    g_rec_mutex_lock(&list->api_mtx);
    MooseSignalType type = moose_convert_name_to_signal(signal_name);

    if (type != MOOSE_SIGNAL_UNKNOWN) {
        GList * head = list->signals[type];

        for (GList * iter = head; iter; iter = iter->next) {
            MooseSignalTag * tag = iter->data;

            if (tag != NULL) {
                if (tag->callback == callback_addr) {
                    list->signals[type] = g_list_delete_link(head, iter);
                    g_slice_free(MooseSignalTag, tag);
                    g_rec_mutex_unlock(&list->api_mtx);
                    return;
                }
            }
        }
    }
    g_rec_mutex_unlock(&list->api_mtx);
}

///////////////////////////////

void moose_priv_signal_report_event_v(MooseSignalList * list, const char * signal_name, va_list args)
{
    g_assert(list);

    g_rec_mutex_lock(&list->api_mtx);
    MooseDispatchTag * data_tag = moose_priv_signal_list_unpack_valist(signal_name, args);
    if (data_tag != NULL) {
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
        g_idle_add(moose_priv_signal_list_dispatch_cb, list);
    }
    g_rec_mutex_unlock(&list->api_mtx);
}

///////////////////////////////

void moose_priv_signal_report_event(MooseSignalList * list, const char * signal_name, ...)
{
    va_list args;
    va_start(args, signal_name);
    moose_priv_signal_report_event_v(list, signal_name, args);
    va_end(args);
}

///////////////////////////////

void moose_priv_signal_list_destroy(MooseSignalList * list)
{
    if (list != NULL) {
        for (int i = 0; i < MOOSE_SIGNAL_VALID_COUNT; i++) {
            for (GList * iter = list->signals[i]; iter; iter = iter->next) {
                MooseSignalTag * tag = iter->data;
                g_slice_free(MooseSignalTag, tag);
            }

            g_list_free(list->signals[i]);
        }

        g_async_queue_unref(list->dispatch_queue);
        g_rec_mutex_clear(&list->api_mtx);
    }
}
