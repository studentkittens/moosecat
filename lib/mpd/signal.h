#ifndef mc_signal_H
#define mc_signal_H

#include <glib.h>
#include <mpd/client.h>
#include <stdarg.h>

/* Prototyoe it here */
struct mc_Client;

///////////////////////////////

typedef enum
{
    MC_SIGNAL_CLIENT_EVENT = 0,
    MC_SIGNAL_ERROR,
    MC_SIGNAL_CONNECTIVITY,
    MC_SIGNAL_PROGRESS,
    /* -> Add new callbacks here <- */
    MC_SIGNAL_VALID_COUNT,
    MC_SIGNAL_UNKNOWN
} mc_SignalType;

typedef struct
{
    GList * signals[MC_SIGNAL_VALID_COUNT];
} mc_SignalList;

///////////////////////////////

/* Event callback */
typedef void (* mc_EventCallback) (struct mc_Client *, enum mpd_idle, void * user_data);

/* Error callback */
typedef void (* mc_ErrorCallback) (struct mc_Client *, enum mpd_error, const char * err_msg, bool is_fatal, void * user_data);

/* Connection change callback */
typedef void (* mc_ConnectivityCallback) (struct mc_Client *, bool server_changed, void * user_data);

/* Progress callback (for display&debug) */
typedef void (* mc_ProgressCallback) (struct mc_Client *, const char * progress, void * user_data);

///////////////////////////////

void  mc_signal_list_init (mc_SignalList * list);

void mc_signal_add (
        mc_SignalList * list,
        const char * signal_name,
        void * callback_func,
        void * user_data);

void mc_signal_add_masked (
        mc_SignalList * list,
        const char * signal_name,
        void * callback_func,
        void * user_data,
        enum mpd_idle mask);

void mc_signal_rm (
        mc_SignalList * list,
        const char * signal_name,
        void * callback_addr);

void mc_signal_report_event_v (
        mc_SignalList *  list,
        const char * signal_name,
        va_list args);

void mc_signal_report_event (
        mc_SignalList *  list,
        const char * signal_name,
        ...);;

void mc_signal_list_destroy (
        mc_SignalList * list);

#endif /* end of include guard: mc_signal_H */

