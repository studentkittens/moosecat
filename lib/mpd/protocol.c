#include "protocol.h"
#include "update.h"

#include "pm/cmnd-core.h"
#include "pm/idle-core.h"

#include "signal-helper.h"
#include "outputs.h"

#include "client.h"

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

static const char *etable[] = {
    [ERR_IS_NULL] = "NULL is not a valid connector.",
    [ERR_UNKNOWN] = "Operation failed for unknow reason."
};

///////////////////
////  PUBLIC //////
///////////////////

mc_Client *mc_create(mc_PmType pm)
{
    mc_Client *client = NULL;

    switch (pm) {
        case MC_PM_IDLE:
            client = mc_create_idler();
            break;

        case MC_PM_COMMAND: 
            client = mc_create_cmnder(-1);
            break;
    }

    if (client != NULL) {
        client->_pm = pm;
        client->is_virgin = true;

        /* init the getput mutex */
        g_rec_mutex_init(&client->_getput_mutex);
        g_rec_mutex_init(&client->_client_attr_mutex);

        mc_priv_signal_list_init(&client->_signals);

        client->_update_data = mc_update_data_new(client);
        client->_outputs = mc_priv_outputs_new(client);
        client->initial_thread = g_thread_self();
    }

    return client;
}

///////////////////

char *mc_connect(
    mc_Client *self,
    GMainContext *context,
    const char *host,
    int port,
    float timeout)
{
    char *err = NULL;

    if (self == NULL)
        return g_strdup(etable[ERR_IS_NULL]);

    ASSERT_IS_MAINTHREAD(self);

    /* Some progress! */
    mc_shelper_report_progress(self, true, "Attempting to connect…");

    mc_client_init(self);

    /* Actual implementation of the connection in respective protcolmachine */
    err = g_strdup(self->do_connect(self, context, host, port, ABS(timeout)));

    if (err == NULL && mc_is_connected(self)) {
        /* Force updating of status/stats/song on connect */
        mc_force_sync(self, INT_MAX);

        /* Check if server changed */
        mc_shelper_report_connectivity(self, host, port, timeout);

        /* Report some progress */
        mc_shelper_report_progress(self, true, "...Fully connected!");
    }

    return err;
}

///////////////////

bool mc_is_connected(mc_Client *self)
{
    g_assert(self);

    bool result = (self && self->do_is_connected(self));

    return result;
}

///////////////////

void mc_put(mc_Client *self)
{
    g_assert(self);

    /* Put connection back to event listening. */
    if (mc_is_connected(self) && self->do_put != NULL) {
        self->do_put(self);
    }

    /* Make the connection accesible to other threads */
    g_rec_mutex_unlock(&self->_getput_mutex);
}

///////////////////

struct mpd_connection *mc_get(mc_Client *self) {
    g_assert(self);

    /* Return the readily sendable connection */
    struct mpd_connection *cconn = NULL;

    /* lock the connection to make sure, only one thread
    * can use it. This prevents us from relying on
    * the user to lock himself. */
    g_rec_mutex_lock(&self->_getput_mutex);

    if (mc_is_connected(self)) {
        cconn = self->do_get(self);
        mc_shelper_report_error(self, cconn);
    }

    return cconn;
}

///////////////////

char *mc_disconnect(
    mc_Client *self)
{
    if (self && mc_is_connected(self)) {

        ASSERT_IS_MAINTHREAD(self);

        /* Lock the connection while destroying it */
        g_rec_mutex_lock(&self->_getput_mutex);

        /* Finish current running command */
        mc_client_destroy(self);

        /* let the connector clean up itself */
        bool error_happenend = !self->do_disconnect(self);

        /* Reset status/song/stats to NULL */
        mc_update_reset(self->_update_data);

        /* Notify user of the disconnect */
        mc_signal_dispatch(self, "connectivity", self, false, false);

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

void mc_free(mc_Client *self)
{
    if (self == NULL)
        return;

    ASSERT_IS_MAINTHREAD(self);

    /* Disconnect if not done yet */
    mc_disconnect(self);

    /* Forget any signals */
    mc_priv_signal_list_destroy(&self->_signals);

    if (mc_status_timer_is_active(self)) {
        mc_status_timer_unregister(self);
    }

    /* Free SSS data */
    mc_update_data_destroy(self->_update_data);

    /* Kill any previously connected host info */
    g_rec_mutex_lock(&self->_client_attr_mutex);
    if (self->_host != NULL)
        g_free(self->_host);
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

void mc_signal_add(
    mc_Client *self,
    const char *signal_name,
    void *callback_func,
    void *user_data)
{
    if (self == NULL)
        return;

    mc_priv_signal_add(&self->_signals, signal_name, false, callback_func, user_data);
}

///////////////////

void mc_signal_add_masked(
    mc_Client *self,
    const char *signal_name,
    void *callback_func,
    void *user_data,
    enum mpd_idle mask)
{
    if (self == NULL)
        return;

    mc_priv_signal_add_masked(&self->_signals, signal_name, false, callback_func, user_data, mask);
}

///////////////////

void mc_signal_rm(
    mc_Client *self,
    const char *signal_name,
    void *callback_addr)
{
    if (self == NULL)
        return;

    mc_priv_signal_rm(&self->_signals, signal_name, callback_addr);
}

///////////////////

void mc_signal_dispatch(
    mc_Client *self,
    const char *signal_name,
    ...)
{
    if (self == NULL)
        return;

    va_list args;
    va_start(args, signal_name);
    mc_priv_signal_report_event_v(&self->_signals, signal_name, args);
    va_end(args);
}

///////////////////

int mc_signal_length(
    mc_Client *self,
    const char *signal_name)
{
    if (self != NULL)
        return mc_priv_signal_length(&self->_signals, signal_name);
    else
        return -1;
}

///////////////////

void mc_force_sync(
    mc_Client *self,
    enum mpd_idle events)
{
    g_assert(self);
    mc_update_data_push(self->_update_data, events);
}

///////////////////

const char *mc_get_host(mc_Client *self)
{
    g_assert(self);

    g_rec_mutex_lock(&self->_client_attr_mutex);
    const char * host = self->_host;
    g_rec_mutex_unlock(&self->_client_attr_mutex);

    return host;
}

///////////////////

unsigned mc_get_port(mc_Client *self)
{
    g_rec_mutex_lock(&self->_client_attr_mutex);
    unsigned port = self->_port;
    g_rec_mutex_unlock(&self->_client_attr_mutex);
    return port;
}

///////////////////

bool mc_status_timer_is_active(mc_Client *self)
{
    return mc_update_status_timer_is_active(self);
}

///////////////////

void mc_status_timer_unregister(mc_Client *self)
{
    mc_update_unregister_status_timer(self);
}

///////////////////

float mc_get_timeout(mc_Client *self)
{
    g_rec_mutex_lock(&self->_client_attr_mutex);
    float timeout = self->_timeout;
    g_rec_mutex_unlock(&self->_client_attr_mutex);
    return timeout;
}

///////////////////

void mc_status_timer_register(
    mc_Client *self,
    int repeat_ms,
    bool trigger_event)
{
    mc_update_register_status_timer(self, repeat_ms, trigger_event);
}

///////////////////
//    OUTPUTS    //
///////////////////

bool mc_outputs_get_state(mc_Client *self, const char *output_name)
{
    g_assert(self);

    return mc_priv_outputs_get_state(self->_outputs, output_name);
}

///////////////////

const char ** mc_outputs_get_names(mc_Client *self)
{
    g_assert(self);

    return mc_priv_outputs_get_names(self->_outputs);
}

///////////////////

bool mc_outputs_set_state(mc_Client *self, const char *output_name, bool state)
{
    g_assert(self);

    return mc_priv_outputs_set_state(self->_outputs, output_name, state);
}

////////////////////////
//    LOCKING STUFF   //
////////////////////////

struct mpd_status *mc_lock_status(struct mc_Client *self)
{
    g_rec_mutex_lock(&self->_update_data->mtx_status);
    return self->_update_data->status;
}

////////////////////////

void mc_unlock_status(struct mc_Client *self)
{
    g_rec_mutex_unlock(&self->_update_data->mtx_status);
}

////////////////////////

struct mpd_stats * mc_lock_statistics(struct mc_Client *self)
{
    g_rec_mutex_lock(&self->_update_data->mtx_statistics);
    return self->_update_data->statistics;
}

////////////////////////

void mc_unlock_statistics(struct mc_Client *self)
{
    g_rec_mutex_unlock(&self->_update_data->mtx_statistics);
}

////////////////////////

struct mpd_song * mc_lock_current_song(struct mc_Client *self)
{
    g_rec_mutex_lock(&self->_update_data->mtx_current_song);
    return self->_update_data->current_song;
}

////////////////////////

void mc_unlock_current_song(struct mc_Client *self)
{
    g_rec_mutex_unlock(&self->_update_data->mtx_current_song);
}

////////////////////////

void mc_lock_outputs(struct mc_Client *self)
{
    g_rec_mutex_lock(&self->_update_data->mtx_outputs);
}

////////////////////////

void mc_unlock_outputs(struct mc_Client *self)
{
    g_rec_mutex_unlock(&self->_update_data->mtx_outputs);
}

////////////////////////

void mc_block_till_sync(mc_Client *self)
{
    g_assert(self);

    mc_update_block_till_sync(self->_update_data);
}
