#include "protocol.h"
#include "update.h"

enum
{
    ERR_OK,
    ERR_IS_NULL,
    ERR_UNKNOWN
};

///////////////////

static const char * etable[] =
{
    [ERR_OK]      = NULL,
    [ERR_IS_NULL] = "NULL is not a valid connector.",
    [ERR_UNKNOWN] = "Operation failed for unknow reason."
};

///////////////////

typedef struct
{
    Proto_EventCallback callback;
    gpointer user_data;
    mpd_idle mask;
} Proto_CallbackTag;

///////////////////
////  PRIVATE /////
///////////////////

static void mc_proto_reset(mc_Client * self)
{
    g_print ("Reset.\n");
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

static GList * mc_proto_find_callback (Proto_EventCallback callback, GList * list)
{
    for (GList * iter = list; iter; iter = iter->next)
    {
        Proto_CallbackTag * tag = (Proto_CallbackTag *) iter->data;
        if (tag != NULL)
        {
            if (tag->callback == callback)
            {
                return iter;
            }
        }
    }
    return NULL;
}

///////////////////

static void mc_proto_add_callback (
    GList ** list,
    Proto_EventCallback callback,
    gpointer user_data,
    mpd_idle mask)
{
    if (list && mc_proto_find_callback (callback, *list) == NULL)
    {
        Proto_CallbackTag * tag = g_slice_alloc (sizeof (Proto_CallbackTag) );
        tag->callback = callback;
        tag->mask = mask;
        tag->user_data = user_data;

        *list = g_list_prepend (*list, tag);
    }
}

///////////////////

static void mc_proto_free_ctag (gpointer data)
{
    g_slice_free (Proto_CallbackTag, data);
}

///////////////////
////  PUBLIC //////
///////////////////

char * mc_proto_connect (
    mc_Client * self,
    GMainContext * context,
    const char * host,
    int port, int timeout)
{
    char * err = NULL;
    if (self == NULL)
        return g_strdup(etable[ERR_IS_NULL]);

    err = g_strdup (self->do_connect (self, context, host, port, timeout) );

    if(err == NULL)
        mc_proto_update_context_info_cb(INT_MAX, self);

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
    /*
     * Put connection back to event listening.
     */
    if (self && self->do_put != NULL)
    {
        self->do_put (self);
    }
}

///////////////////

mpd_connection * mc_proto_get (mc_Client * self)
{
    /*
     * Return the readily sendable connection
     */
    if (self)
        return self->do_get (self);
    else
        return NULL;
}

///////////////////

void mc_proto_add_event_callback_mask (
    mc_Client * self,
    Proto_EventCallback callback,
    gpointer user_data, mpd_idle mask)
{
    if (self) mc_proto_add_callback (
            &self->_event_callbacks,
            callback,
            user_data,
            mask);
}

///////////////////

void mc_proto_add_event_callback (
    mc_Client * self,
    Proto_EventCallback callback,
    gpointer user_data)
{
    if (self) mc_proto_add_event_callback_mask (
            self,
            callback,
            user_data,
            INT_MAX);
}

///////////////////

void mc_proto_rm_event_callback (
    mc_Client * self,
    Proto_EventCallback callback)
{
    if (self && callback)
    {
        GList * tag = mc_proto_find_callback (callback, self->_event_callbacks);
        self->_event_callbacks = g_list_delete_link (self->_event_callbacks, tag);
    }
}

///////////////////

void mc_proto_add_error_callback (mc_Client * self,
                               Proto_ErrorCallback callback,
                               gpointer user_data)
{
    if (self) mc_proto_add_callback (
            &self->_error_callbacks,
            (Proto_EventCallback) callback,
            user_data,
            INT_MAX);
}

///////////////////

void mc_proto_rm_error_callback (
    mc_Client * self,
    Proto_ErrorCallback callback)
{
    if (self && callback)
    {
        GList * tag = mc_proto_find_callback (
                          (Proto_EventCallback) callback,
                          self->_error_callbacks);

        self->_error_callbacks = g_list_delete_link (self->_error_callbacks, tag);
    }
}

///////////////////

char * mc_proto_disconnect (
    mc_Client * self)
{
    if (self)
    {
        /* let the connector clean up itself */
        bool error_happenend = self->do_disconnect (self);

        /* Reset status/song/stats to NULL */
        mc_proto_reset (self);

        if (error_happenend)
            return g_strdup (etable[ERR_UNKNOWN]);
        else
            return NULL;
    }
    return g_strdup (etable[ERR_IS_NULL]);
}

///////////////////

void mc_proto_free (mc_Client * self)
{
    /* Free the callback list */
    g_list_free_full (self->_event_callbacks, mc_proto_free_ctag);
    g_list_free_full (self->_error_callbacks, mc_proto_free_ctag);

    /* Free status/stats/song */
    mc_proto_reset (self);

    /* Allow special connector to cleanup */
    if (self->do_free != NULL)
        self->do_free (self);
}

///////////////////

void mc_proto_update (
    mc_Client * self,
    enum mpd_idle events)
{
    mc_proto_update_context_info_cb (events, self);
    for (GList * iter = self->_event_callbacks; iter; iter = iter->next)
    {
        Proto_CallbackTag * tag = iter->data;
        if (tag != NULL && (tag->mask & events) > 0)
        {
            tag->callback (events, tag->user_data);
        }
    }
}
