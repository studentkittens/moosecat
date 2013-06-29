#include "protocol.h"
#include "update.h"

#include "pm/cmnd-core.h"
#include "pm/idle-core.h"

#include "signal-helper.h"
#include "outputs.h"

#include "client.h"

/* memset */
#include <string.h>

///////////////////
////  PRIVATE /////
///////////////////

enum {
    ERR_OK,
    ERR_IS_NULL,
    ERR_UNKNOWN
};

///////////////////

static const char *etable[] = {
    [ERR_OK]      = NULL,
    [ERR_IS_NULL] = "NULL is not a valid connector.",
    [ERR_UNKNOWN] = "Operation failed for unknow reason."
};

///////////////////
////  PUBLIC //////
///////////////////

mc_Client *mc_proto_create(mc_PmType pm)
{
    mc_Client *client = NULL;

    switch (pm) {
    case MC_PM_IDLE:
        client = mc_proto_create_idler();
        break;

    case MC_PM_COMMAND: 
        client = mc_proto_create_cmnder(-1);
        break;
    }

    if (client != NULL) {
        client->_pm = pm;
        mc_signal_list_init(&client->_signals);
    }

    client->_update_data = mc_proto_update_data_new(client);
    client->_outputs = mc_proto_outputs_new(client);

    /* init the getput mutex */
    g_rec_mutex_init(&client->_getput_mutex);

    return client;
}

///////////////////

char *mc_proto_connect(
    mc_Client *self,
    GMainContext *context,
    const char *host,
    int port,
    float timeout)
{
    char *err = NULL;

    if (self == NULL)
        return g_strdup(etable[ERR_IS_NULL]);

    /* Some progress! */
    mc_shelper_report_progress(self, true, "Attempting to connect…");

    mc_client_init(self);

    /* Actual implementation of the connection in respective protcolmachine */
    err = g_strdup(self->do_connect(self, context, host, port, ABS(timeout)));

    if (err == NULL && mc_proto_is_connected(self)) {
        /* Force updating of status/stats/song on connect */
        mc_proto_force_sync(self, INT_MAX);

        /* For bugreports only */
        self->_timeout = timeout;

        /* Check if server changed */
        mc_shelper_report_connectivity(self, host, port);

        /* Report some progress */
        mc_shelper_report_progress(self, true, "…Fully connected!");
    }

    return err;
}

///////////////////

bool mc_proto_is_connected(mc_Client *self)
{
    g_assert(self);

    bool result = (self && self->do_is_connected(self));

    return result;
}

///////////////////

void mc_proto_put(mc_Client *self)
{
    g_assert(self);

    /* Put connection back to event listening. */
    if (mc_proto_is_connected(self) && self->do_put != NULL) {
        self->do_put(self);

        /* Make the connection accesible to other threads */
        g_rec_mutex_unlock(&self->_getput_mutex);

    }
}

///////////////////

struct mpd_connection *mc_proto_get(mc_Client *self) {
    g_assert(self);

    /* Return the readily sendable connection */
    struct mpd_connection *cconn = NULL;


    if (mc_proto_is_connected(self)) {
        /* lock the connection to make sure, only one thread
        * can use it. This prevents us from relying on
        * the user to lock himself. */
        g_rec_mutex_lock(&self->_getput_mutex);

        cconn = self->do_get(self);
        mc_shelper_report_error(self, cconn);
    }

    return cconn;
}

///////////////////

char *mc_proto_disconnect(
    mc_Client *self)
{
    if (self && mc_proto_is_connected(self)) {
        /* Lock the connection while destroying it */
        g_rec_mutex_lock(&self->_getput_mutex);

        /* Finish current running command */
        mc_client_destroy(self);

        /* let the connector clean up itself */
        bool error_happenend = !self->do_disconnect(self);

        /* Reset status/song/stats to NULL */
        mc_proto_update_reset(self->_update_data);

        /* Notify user of the disconnect */
        mc_proto_signal_dispatch(self, "connectivity", self, false);

        /* Free output list */
        mc_proto_outputs_destroy(self->_outputs);

        /* Unlock the mutex - we can use it now again
         * e.g. - queued commands would wake up now
         *        and notice they are not connected anymore
         */
        g_rec_mutex_unlock(&self->_getput_mutex);

        if (error_happenend)
            return g_strdup(etable[ERR_UNKNOWN]);
        else
            return NULL;
    }

    return (self == NULL) ? g_strdup(etable[ERR_IS_NULL]) : NULL;
}

///////////////////

void mc_proto_free(mc_Client *self)
{
    if (self == NULL)
        return;

    if (mc_proto_status_timer_is_active(self)) {
        mc_proto_status_timer_unregister(self);
    }

    /* Disconnect if not done yet */
    mc_proto_disconnect(self);

    /* Kill any previously connected host info */
    if (self->_host != NULL)
        g_free(self->_host);

    /* Forget any signals */
    mc_signal_list_destroy(&self->_signals);

    /* Free SSS data */
    mc_proto_update_reset(self->_update_data);
    mc_proto_update_data_destroy(self->_update_data);

    g_rec_mutex_clear(&self->_getput_mutex);

    /* Allow special connector to cleanup */
    if (self->do_free != NULL)
        self->do_free(self);
}

///////////////////
//// SIGNALS //////
///////////////////

void mc_proto_signal_add(
    mc_Client *self,
    const char *signal_name,
    void *callback_func,
    void *user_data)
{
    if (self == NULL)
        return;

    mc_signal_add(&self->_signals, signal_name, false, callback_func, user_data);
}

///////////////////

void mc_proto_signal_add_masked(
    mc_Client *self,
    const char *signal_name,
    void *callback_func,
    void *user_data,
    enum mpd_idle mask)
{
    if (self == NULL)
        return;

    mc_signal_add_masked(&self->_signals, signal_name, false, callback_func, user_data, mask);
}

///////////////////

void mc_proto_signal_rm(
    mc_Client *self,
    const char *signal_name,
    void *callback_addr)
{
    if (self == NULL)
        return;

    mc_signal_rm(&self->_signals, signal_name, callback_addr);
}

///////////////////

void mc_proto_signal_dispatch(
    mc_Client *self,
    const char *signal_name,
    ...)
{
    if (self == NULL)
        return;

    va_list args;
    va_start(args, signal_name);
    mc_signal_report_event_v(&self->_signals, signal_name, args);
    va_end(args);
}

///////////////////

int mc_proto_signal_length(
    mc_Client *self,
    const char *signal_name)
{
    if (self != NULL)
        return mc_signal_length(&self->_signals, signal_name);
    else
        return -1;
}

///////////////////

void mc_proto_force_sync(
    mc_Client *self,
    enum mpd_idle events)
{
    g_assert(self);
    mc_proto_update_data_push(self->_update_data, events);
}

///////////////////

const char *mc_proto_get_host(mc_Client *self)
{
    g_assert(self);

    return self->_host;
}

///////////////////

int mc_proto_get_port(mc_Client *self)
{
    return self->_port;
}

///////////////////

bool mc_proto_status_timer_is_active(mc_Client *self)
{
    return mc_proto_update_status_timer_is_active(self);
}

///////////////////

void mc_proto_status_timer_unregister(mc_Client *self)
{
    mc_proto_update_unregister_status_timer(self);
}

///////////////////

int mc_proto_get_timeout(mc_Client *self)
{
    return self->_timeout;
}

///////////////////

void mc_proto_status_timer_register(
    mc_Client *self,
    int repeat_ms,
    bool trigger_event)
{
    mc_proto_update_register_status_timer(self, repeat_ms, trigger_event);
}
