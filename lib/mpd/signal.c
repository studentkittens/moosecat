#include "signal.h"
#include "protocol.h"

// memset
#include <string.h>

///////////////////////////////
////////// PRIVATE ////////////
///////////////////////////////
typedef struct {
    mc_ClientEventCallback callback;
    gpointer user_data;
    enum mpd_idle mask;
} mc_SignalTag;

static const char *signal_to_name_table[] = {
    [MC_SIGNAL_CLIENT_EVENT] = "client-event",
    [MC_SIGNAL_CONNECTIVITY] = "connectivity",
    [MC_SIGNAL_LOGGING]      = "logging",
    [MC_SIGNAL_UNKNOWN]      = NULL
};

/*
 * client-event
 * connectivity
 * logging
 *   - error
 *   - progress
 *   - op-finished
 */

static mc_SignalType mc_convert_name_to_signal(const char *signame)
{
    if (signame != NULL)
        for (int i = 0; i < MC_SIGNAL_VALID_COUNT; i++)
            if (g_strcmp0(signal_to_name_table[i], signame) == 0)
                return i;

    return MC_SIGNAL_UNKNOWN;
}

///////////////////////////////
////////// PUBLIC /////////////
///////////////////////////////

void  mc_signal_list_init(mc_SignalList *list)
{
    if (list == NULL)
        return;

    memset(list, 0, sizeof(mc_SignalList));
    g_rec_mutex_init(&list->dispatch_mutex);
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

    mc_SignalTag *tag = g_new0(mc_SignalTag, 1);
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
                    g_free(tag);
                    return;
                }
            }
        }
    }
}

///////////////////////////////

#define DISPATCH_START                                     \
    for (GList * iter = cb_list; iter; iter = iter->next)  \
    {                                                      \
        mc_SignalTag * tag = iter->data;                   \
        if (tag != NULL)                                   \
        {                                                  \
            g_rec_mutex_lock (&list->dispatch_mutex);      \
 
#define DISPATCH_END                                       \
    g_rec_mutex_unlock (&list->dispatch_mutex);            \
        }                                                  \
    }                                                      \
 
void mc_signal_report_event_v(mc_SignalList   *list, const char *signal_name, va_list args)
{
    mc_SignalType sig_type = mc_convert_name_to_signal(signal_name);

    if (sig_type == MC_SIGNAL_UNKNOWN)
        return;

    GList *cb_list = list->signals[sig_type];

    if (cb_list == NULL)
        return;

    /* All callbacks get a client as first param */
    struct mc_Client *client = va_arg(args, struct mc_Client *);

    switch (sig_type) {
    case MC_SIGNAL_CLIENT_EVENT: {
        enum mpd_idle event = va_arg(args, int);
        DISPATCH_START {
            if (tag->mask & event) {
                ((mc_ClientEventCallback) tag->callback)(client, event, tag->user_data);
            }
        }
        DISPATCH_END
        break;
    }

    case MC_SIGNAL_LOGGING: {
        const char *error_msg = va_arg(args, const char *);
        mc_LogLevel level = va_arg(args, int);
        DISPATCH_START {
            ((mc_LoggingCallback) tag->callback)(client,  error_msg, level, tag->user_data);
        }
        DISPATCH_END
        break;
    }

    case MC_SIGNAL_CONNECTIVITY: {
        int server_changed = va_arg(args, int);
        DISPATCH_START {
            ((mc_ConnectivityCallback) tag->callback)(client, server_changed, tag->user_data);
        }
        DISPATCH_END
        break;
    }

    default:
        /* Should not happen */
        break;
    }
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
                g_free(tag);
            }

            g_list_free(list->signals[i]);
        }

        g_rec_mutex_clear(&list->dispatch_mutex);
    }
}
