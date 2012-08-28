#include "protocol.h"
#include "signal_helper.h"
#include "update.h"

#include <glib/gprintf.h>

void mc_shelper_report_error (struct mc_Client * self, mpd_connection * cconn)
{
    if (self != NULL && cconn != NULL)
        return;

    enum mpd_error error = mpd_connection_get_error (cconn);
    if (error != MPD_ERROR_SUCCESS)
    {
        const char * error_message = mpd_connection_get_error_message (cconn);
        bool is_fatal = !mpd_connection_clear_error (cconn);

        if (is_fatal)
        {
            mc_proto_disconnect (self);
        }

        mc_proto_signal_dispatch (self, "error", error, error_message, is_fatal);
    }
}

///////////////////////////////

void mc_shelper_report_progress (struct mc_Client * self, const char * format, ...)
{
    char * full_string = NULL;
    va_list list_ptr;
    va_start (list_ptr, format);
    if (g_vasprintf (&full_string, format, list_ptr) != 0 && full_string)
    {
        mc_proto_signal_dispatch (self, "progress", self, full_string);
        g_free (full_string);
    }
    va_end (list_ptr);
}

///////////////////////////////

void mc_shelper_report_connectivity (
    struct mc_Client * self,
    const char * new_host,
    int new_port)
{
    bool server_changed = (g_strcmp0 (new_host, self->_host) != 0) || (new_port != self->_port);
    mc_proto_signal_dispatch (self, "connectivity", self, server_changed);

    if (self->_host != NULL)
        g_free (self->_host);

    self->_host = g_strdup (new_host);
    self->_port = new_port;
}

///////////////////////////////

void mc_shelper_report_client_event (
        struct mc_Client * self, 
        enum mpd_idle event)
{
    mc_proto_update_context_info_cb (event, self);
    mc_proto_signal_dispatch (self, "client-event", self, event);
}
