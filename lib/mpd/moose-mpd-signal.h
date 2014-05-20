#ifndef moose_priv_signal_H
#define moose_priv_signal_H

#include <glib.h>
#include <stdarg.h>
#include <mpd/client.h>


/* Prototyoe it here */
struct MooseClient;

///////////////////////////////

typedef enum {
    MOOSE_SIGNAL_CLIENT_EVENT = 0,
    MOOSE_SIGNAL_CONNECTIVITY,
    MOOSE_SIGNAL_LOGGING,
    /* This signal is used internally
     * to propagate fatal errors
     */
    MOOSE_SIGNAL_FATAL_ERROR,
    /* -> Add new callbacks here <- */
    MOOSE_SIGNAL_VALID_COUNT,
    MOOSE_SIGNAL_UNKNOWN
} MooseSignalType;

typedef struct {
    /* Holds a list of callbacks,
     * one list for each type.
     */
    GList * signals[MOOSE_SIGNAL_VALID_COUNT];

    /* Signals are send to this queue if they are not dispatched
     * in the mainthread.
     */
    GAsyncQueue * dispatch_queue;

    /* Protect signal_add/rm/length/dispatch
     * from accessing it simultaneously from serveral threads
     */
    GRecMutex api_mtx;

} MooseSignalList;

///////////////////////////////

typedef enum MooseLogLevels {
    MOOSE_LOG_CRITICAL = 1 << 0,
        MOOSE_LOG_ERROR    = 1 << 1,
        MOOSE_LOG_WARNING  = 1 << 2,
        MOOSE_LOG_INFO     = 1 << 3,
        MOOSE_LOG_DEBUG    = 1 << 4
} MooseLogLevel;

///////////////////////////////

/* Event callback */
typedef void (* MooseClientEventCallback)(struct MooseClient *, enum mpd_idle, void * user_data);

/* Logging callback */
typedef void (* MooseLoggingCallback)(struct MooseClient *, const char * err_msg, MooseLogLevel level, void * user_data);

/* Connection change callback */
typedef void (* MooseConnectivityCallback)(struct MooseClient *, bool server_changed, bool was_connected, void * user_data);

///////////////////////////////

void  moose_priv_signal_list_init(MooseSignalList * list);

void moose_priv_signal_add(
    MooseSignalList * list,
    const char * signal_name,
    bool call_first,
    void * callback_func,
    void * user_data);

void moose_priv_signal_add_masked(
    MooseSignalList * list,
    const char * signal_name,
    bool call_first,
    void * callback_func,
    void * user_data,
    enum mpd_idle mask);

void moose_priv_signal_rm(
    MooseSignalList * list,
    const char * signal_name,
    void * callback_addr);

void moose_priv_signal_report_event_v(
    MooseSignalList   * list,
    const char * signal_name,
    va_list args);

void moose_priv_signal_report_event(
    MooseSignalList   * list,
    const char * signal_name,
    ...);

void moose_priv_signal_list_destroy(
    MooseSignalList * list);

int moose_priv_signal_length(
    MooseSignalList * list,
    const char * signal_name);

#endif /* end of include guard: moose_priv_signal_H */
