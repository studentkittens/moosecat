#include "protocol.h"
#include "update.h"

#include "pm/cmnd_core.h"
#include "pm/idle_core.h"

#include "signal_helper.h"

/* memset */
#include <string.h>

enum {
    ERR_OK,
    ERR_IS_NULL,
    ERR_UNKNOWN
};

///////////////////

static const char * etable[] = {
    [ERR_OK]      = NULL,
    [ERR_IS_NULL] = "NULL is not a valid connector.",
    [ERR_UNKNOWN] = "Operation failed for unknow reason."
};


///////////////////
////  PRIVATE /////
///////////////////

///////////////////

static void mc_proto_reset (mc_Client * self)
{
    /* Free status/song/stats */
    if (self->status != NULL)
        mpd_status_free (self->status);

    if (self->stats != NULL)
        mpd_stats_free (self->stats);

    if (self->song != NULL)
        mpd_song_free (self->song);

    self->song = NULL;
    self->stats = NULL;
    self->status = NULL;
}

///////////////////
////  PUBLIC //////
///////////////////

mc_Client * mc_proto_create (mc_PmType pm)
{
    mc_Client * client = NULL;
    switch (pm) {
    case MC_PM_IDLE:
        client = mc_proto_create_idler();
    case MC_PM_COMMAND: // TODO: 
        client = mc_proto_create_cmnder(-1);
    }

    if (client != NULL) {
        client->_pm = pm;
        memset (client->error_buffer, 0, _MC_PROTO_MAX_ERR_LEN);
        mc_signal_list_init (&client->_signals);
    }

    return client;
}

///////////////////

char * mc_proto_connect (
    mc_Client * self,
    GMainContext * context,
    const char * host,
    int port,
    float timeout)
{
    char * err = NULL;
    if (self == NULL)
        return g_strdup (etable[ERR_IS_NULL]);

    /* Some progress! */
    mc_shelper_report_progress (self, true, "Attempting to connect... ");

    /* init the getput mutex */
    g_rec_mutex_init (&self->_getput_mutex);

    /* Actual implementation of the connection in respective protcolmachine */
    err = g_strdup (self->do_connect (self, context, host, port, ABS (timeout) ) );

    if (err == NULL && mc_proto_is_connected (self) ) {
        /* Force updating of status/stats/song on connect */
        mc_proto_update_context_info_cb (self, INT_MAX, NULL);

        /* For bugreports only */
        self->_timeout = timeout;

        /* Check if server changed */
        mc_shelper_report_connectivity (self, host, port);

        /* Report some progress */
        mc_shelper_report_progress (self, true, "Fully connected!");
    }
    return err;
}

///////////////////

bool mc_proto_is_connected (mc_Client * self)
{
    return self && self->do_is_connected (self);
}

///////////////////

void mc_proto_put (mc_Client * self)
{
    g_assert (self);

    /* Put connection back to event listening. */
    if (mc_proto_is_connected (self) && self->do_put != NULL)
        self->do_put (self);

    /* Make the connection accesible to other threads */
    g_rec_mutex_unlock (&self->_getput_mutex);
}

///////////////////

mpd_connection * mc_proto_get (mc_Client * self)
{
    g_assert (self);
    
    /* lock the connection to make sure, only one thread
     * can use it. This prevents us from relying on
     * the user to lock himself. */
    g_rec_mutex_lock (&self->_getput_mutex);

    /*
     * Return the readily sendable connection
     */
    mpd_connection * cconn = NULL;
    if (mc_proto_is_connected (self) ) {
        cconn = self->do_get (self);
        mc_shelper_report_error (self, cconn);
    }

    return cconn;
}

///////////////////

char * mc_proto_disconnect (
    mc_Client * self)
{
    if (self && mc_proto_is_connected (self) ) {
        /* let the connector clean up itself */
        bool error_happenend = !self->do_disconnect (self);

        /* Reset status/song/stats to NULL */
        mc_proto_reset (self);

        /* Notify user of the disconnect */
        mc_proto_signal_dispatch (self, "connectivity", self, false);

        if (error_happenend)
            return g_strdup (etable[ERR_UNKNOWN]);
        else
            return NULL;
    }

    return (self == NULL) ? g_strdup (etable[ERR_IS_NULL]) : NULL;
}

///////////////////

void mc_proto_free (mc_Client * self)
{
    if (self == NULL)
        return;

    /* Disconnect if not done yet */
    mc_proto_disconnect (self);

    /* Kill any previously connected host info */
    if (self->_host != NULL)
        g_free (self->_host);

    /* Forget any signals */
    mc_signal_list_destroy (&self->_signals);

    /* kill the mutex */
    g_rec_mutex_clear (&self->_getput_mutex);

    /* Allow special connector to cleanup */
    if (self->do_free != NULL)
        self->do_free (self);
}

///////////////////
//// SIGNALS //////
///////////////////

void mc_proto_signal_add (
    mc_Client * self,
    const char * signal_name,
    void * callback_func,
    void * user_data)
{
    if (self == NULL)
        return;

    mc_signal_add (&self->_signals, signal_name, callback_func, user_data);
}

///////////////////

void mc_proto_signal_add_masked (
    mc_Client * self,
    const char * signal_name,
    void * callback_func,
    void * user_data,
    enum mpd_idle mask)
{
    if (self == NULL)
        return;

    mc_signal_add_masked (&self->_signals, signal_name, callback_func, user_data, mask);
}

///////////////////

void mc_proto_signal_rm (
    mc_Client * self,
    const char * signal_name,
    void * callback_addr)
{
    if (self == NULL)
        return;

    mc_signal_rm (&self->_signals, signal_name, callback_addr);
}

///////////////////

void mc_proto_signal_dispatch (
    mc_Client * self,
    const char * signal_name,
    ...)
{
    if (self == NULL)
        return;

    va_list args;
    va_start (args, signal_name);
    mc_signal_report_event_v (&self->_signals, signal_name, args);
    va_end (args);
}

///////////////////
//

int mc_proto_signal_length (
    mc_Client * self,
    const char * signal_name)
{
    if (self != NULL)
        return mc_signal_length (&self->_signals, signal_name);
    else
        return -1;
}
