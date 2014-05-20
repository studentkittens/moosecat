#include "moose-mpd-protocol.h"
#include "moose-mpd-update.h"

#include "pm/moose-mpd-cmnd-core.h"
#include "pm/moose-mpd-idle-core.h"

#include "moose-mpd-signal-helper.h"
#include "moose-mpd-client.h"

/* memset() */
#include <string.h>

#define ASSERT_IS_MAINTHREAD(client) g_assert(g_thread_self() == (client)->initial_thread)

///////////////////
////  PRIVATE /////
///////////////////

enum {
    ERR_IS_NULL,
    ERR_UNKNOWN
};

///////////////////

static const char * etable[] = {
    [ERR_IS_NULL] = "NULL is not a valid connector.",
    [ERR_UNKNOWN] = "Operation failed for unknow reason."
};

///////////////////
////  PUBLIC //////
///////////////////

MooseClient * moose_create(MoosePmType pm)
{
    MooseClient * client = NULL;

    switch (pm) {
    case MOOSE_PM_IDLE:
        client = moose_create_idler();
        break;

    case MOOSE_PM_COMMAND:
        client = moose_create_cmnder(-1);
        break;
    }

    if (client != NULL) {
        client->_pm = pm;
        client->is_virgin = true;

        /* init the getput mutex */
        g_rec_mutex_init(&client->_getput_mutex);
        g_rec_mutex_init(&client->_client_attr_mutex);

        moose_priv_signal_list_init(&client->_signals);

        client->_update_data = moose_update_data_new(client);
        client->initial_thread = g_thread_self();
    }

    return client;
}

///////////////////

char * moose_connect(
    MooseClient * self,
    GMainContext * context,
    const char * host,
    int port,
    float timeout)
{
    char * err = NULL;

    if (self == NULL) {
        return g_strdup(etable[ERR_IS_NULL]);
    }

    if (moose_is_connected(self)) {
        return g_strdup("Already connected.");
    }

    ASSERT_IS_MAINTHREAD(self);

    /* Some progress! */
    moose_shelper_report_progress(self, true, "Attempting to connect…");

    moose_client_init(self);

    /* Actual implementation of the connection in respective protcolmachine */
    err = g_strdup(self->do_connect(self, context, host, port, ABS(timeout)));

    if (err == NULL && moose_is_connected(self)) {
        /* Force updating of status/stats/song on connect */
        moose_force_sync(self, INT_MAX);

        /* Check if server changed */
        moose_shelper_report_connectivity(self, host, port, timeout);

        /* Report some progress */
        moose_shelper_report_progress(self, true, "...Fully connected!");
    }

    return err;
}

///////////////////

bool moose_is_connected(MooseClient * self)
{
    g_assert(self);

    bool result = (self && self->do_is_connected(self));

    return result;
}

///////////////////

void moose_put(MooseClient * self)
{
    g_assert(self);

    /* Put connection back to event listening. */
    if (moose_is_connected(self) && self->do_put != NULL) {
        self->do_put(self);
    }

    /* Make the connection accesible to other threads */
    g_rec_mutex_unlock(&self->_getput_mutex);
}

///////////////////

struct mpd_connection * moose_get(MooseClient * self) {
    g_assert(self);

    /* Return the readily sendable connection */
    struct mpd_connection * cconn = NULL;

    /* lock the connection to make sure, only one thread
     * can use it. This prevents us from relying on
     * the user to lock himself. */
    g_rec_mutex_lock(&self->_getput_mutex);

    if (moose_is_connected(self)) {
        cconn = self->do_get(self);
        moose_shelper_report_error(self, cconn);
    }

    return cconn;
}

///////////////////

char * moose_disconnect(
    MooseClient * self)
{
    if (self && moose_is_connected(self)) {

        ASSERT_IS_MAINTHREAD(self);

        bool error_happenend = false;

        /* Lock the connection while destroying it */
        g_rec_mutex_lock(&self->_getput_mutex); {
            /* Finish current running command */
            moose_client_destroy(self);

            /* let the connector clean up itself */
            error_happenend = !self->do_disconnect(self);

            /* Reset status/song/stats to NULL */
            moose_update_reset(self->_update_data);

            /* Notify user of the disconnect */
            moose_signal_dispatch(self, "connectivity", self, false, false);

            /* Unlock the mutex - we can use it now again
             * e.g. - queued commands would wake up now
             *        and notice they are not connected anymore
             */
        }
        g_rec_mutex_unlock(&self->_getput_mutex);

        if (error_happenend)
            return g_strdup(etable[ERR_UNKNOWN]);
        else
            return NULL;
    }

    return (self == NULL) ? g_strdup(etable[ERR_IS_NULL]) : NULL;
}

///////////////////

void moose_free(MooseClient * self)
{
    if (self == NULL)
        return;

    ASSERT_IS_MAINTHREAD(self);

    /* Disconnect if not done yet */
    moose_disconnect(self);

    /* Forget any signals */
    moose_priv_signal_list_destroy(&self->_signals);

    if (moose_status_timer_is_active(self)) {
        moose_status_timer_unregister(self);
    }

    /* Free SSS data */
    moose_update_data_destroy(self->_update_data);

    /* Kill any previously connected host info */
    g_rec_mutex_lock(&self->_client_attr_mutex);
    if (self->_host != NULL) {
        g_free(self->_host);
    }

    g_rec_mutex_unlock(&self->_client_attr_mutex);
    g_rec_mutex_clear(&self->_getput_mutex);
    g_rec_mutex_clear(&self->_client_attr_mutex);

    /* Allow special connector to cleanup */
    if (self->do_free != NULL)
        self->do_free(self);
}

///////////////////
//// SIGNALS //////
///////////////////

void moose_signal_add(
    MooseClient * self,
    const char * signal_name,
    void * callback_func,
    void * user_data)
{
    if (self == NULL)
        return;

    moose_priv_signal_add(&self->_signals, signal_name, false, callback_func, user_data);
}

///////////////////

void moose_signal_add_masked(
    MooseClient * self,
    const char * signal_name,
    void * callback_func,
    void * user_data,
    enum mpd_idle mask)
{
    if (self == NULL)
        return;

    moose_priv_signal_add_masked(&self->_signals, signal_name, false, callback_func, user_data, mask);
}

///////////////////

void moose_signal_rm(
    MooseClient * self,
    const char * signal_name,
    void * callback_addr)
{
    if (self == NULL)
        return;

    moose_priv_signal_rm(&self->_signals, signal_name, callback_addr);
}

///////////////////

void moose_signal_dispatch(
    MooseClient * self,
    const char * signal_name,
    ...)
{
    if (self == NULL)
        return;

    va_list args;
    va_start(args, signal_name);
    moose_priv_signal_report_event_v(&self->_signals, signal_name, args);
    va_end(args);
}

///////////////////

int moose_signal_length(
    MooseClient * self,
    const char * signal_name)
{
    if (self != NULL)
        return moose_priv_signal_length(&self->_signals, signal_name);
    else
        return -1;
}

///////////////////

void moose_force_sync(
    MooseClient * self,
    enum mpd_idle events)
{
    g_assert(self);
    moose_update_data_push(self->_update_data, events);
}

///////////////////

const char * moose_get_host(MooseClient * self)
{
    g_assert(self);

    g_rec_mutex_lock(&self->_client_attr_mutex);
    const char * host = self->_host;
    g_rec_mutex_unlock(&self->_client_attr_mutex);

    return host;
}

///////////////////

unsigned moose_get_port(MooseClient * self)
{
    g_rec_mutex_lock(&self->_client_attr_mutex);
    unsigned port = self->_port;
    g_rec_mutex_unlock(&self->_client_attr_mutex);
    return port;
}

///////////////////

bool moose_status_timer_is_active(MooseClient * self)
{
    return moose_update_status_timer_is_active(self);
}

///////////////////

void moose_status_timer_unregister(MooseClient * self)
{
    moose_update_unregister_status_timer(self);
}

///////////////////

float moose_get_timeout(MooseClient * self)
{
    g_rec_mutex_lock(&self->_client_attr_mutex);
    float timeout = self->_timeout;
    g_rec_mutex_unlock(&self->_client_attr_mutex);
    return timeout;
}

///////////////////

void moose_status_timer_register(
    MooseClient * self,
    int repeat_ms,
    bool trigger_event)
{
    moose_update_register_status_timer(self, repeat_ms, trigger_event);
}

MooseStatus * moose_ref_status(struct MooseClient * self) {
    MooseStatus * status = self->_update_data->status;
    if (status != NULL) {
        g_object_ref(status);
    }
    return status;
}

////////////////////////

void moose_block_till_sync(MooseClient * self)
{
    g_assert(self);

    if (moose_is_connected(self)) {
        moose_update_block_till_sync(self->_update_data);
    }
}
