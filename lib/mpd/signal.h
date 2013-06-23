#ifndef mc_signal_H
#define mc_signal_H

#include <glib.h>
#include <mpd/client.h>
#include <stdarg.h>

/* Prototyoe it here */
struct mc_Client;

///////////////////////////////

typedef enum {
    MC_SIGNAL_CLIENT_EVENT = 0,
    MC_SIGNAL_CONNECTIVITY,
    MC_SIGNAL_LOGGING,
    /* This signal is used internally 
     * to propagate fatal errors
     */
    MC_SIGNAL_FATAL_ERROR,
    /* -> Add new callbacks here <- */
    MC_SIGNAL_VALID_COUNT,
    MC_SIGNAL_UNKNOWN
} mc_SignalType;

typedef struct {
    /* Holds a list of callbacks,
     * one list for each type.
     */
    GList *signals[MC_SIGNAL_VALID_COUNT];

    /* Signals are send to this queue if they are not dispatched
     * in the mainthread.
     */
    GAsyncQueue * dispatch_queue;

    /* ID of a GAsyncQueueWatch that transfers signals from any 
     * thread to the initial mainthread 
     */
    guint signal_watch_id;

    /* The Thread mc_signal_list_init() was called in.
     * This is the Thread signals are dispatched to.
     */
    GThread *initial_thread;

} mc_SignalList;

///////////////////////////////

typedef enum mc_LogLevels {
    MC_LOG_CRITICAL = 1 << 0,
    MC_LOG_ERROR    = 1 << 1,
    MC_LOG_WARNING  = 1 << 2,
    MC_LOG_INFO     = 1 << 3,
    MC_LOG_DEBUG    = 1 << 4
} mc_LogLevel;

///////////////////////////////

/* Event callback */
typedef void (* mc_ClientEventCallback)(struct mc_Client *, enum mpd_idle, void *user_data);

/* Logging callback */
typedef void (* mc_LoggingCallback)(struct mc_Client *, const char *err_msg, mc_LogLevel level, void *user_data);

/* Connection change callback */
typedef void (* mc_ConnectivityCallback)(struct mc_Client *, bool server_changed, void *user_data);

///////////////////////////////

void  mc_signal_list_init(mc_SignalList *list);

void mc_signal_add(
    mc_SignalList *list,
    const char *signal_name,
    bool call_first,
    void *callback_func,
    void *user_data);

void mc_signal_add_masked(
    mc_SignalList *list,
    const char *signal_name,
    bool call_first,
    void *callback_func,
    void *user_data,
    enum mpd_idle mask);

void mc_signal_rm(
    mc_SignalList *list,
    const char *signal_name,
    void *callback_addr);

void mc_signal_report_event_v(
    mc_SignalList   *list,
    const char *signal_name,
    va_list args);

void mc_signal_report_event(
    mc_SignalList   *list,
    const char *signal_name,
    ...);

void mc_signal_list_destroy(
    mc_SignalList *list);

int mc_signal_length(
    mc_SignalList *list,
    const char *signal_name);

#endif /* end of include guard: mc_signal_H */

