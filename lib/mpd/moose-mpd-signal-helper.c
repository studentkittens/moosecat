#include "moose-mpd-client.h"
#include "moose-mpd-signal-helper.h"

#include <glib/gprintf.h>
#include <string.h>

///////////////////////////////

static char * compose_error_msg(const char * topic, const char * src)
{
    if (src && topic) {
        return g_strdup_printf("%s: ,,%s''", topic, src);
    } else {
        return NULL;
    }
}

///////////////////////////////

static bool moose_shelper_report_error_impl(struct MooseClient * self, struct mpd_connection * cconn, bool handle_fatal)
{
    if (self == NULL || cconn == NULL) {
        if (self != NULL) {
            moose_client_signal_dispatch(
                self, "logging", self,
                "Connection to MPD was closed (IsFatal=maybe?)",
                MOOSE_LOG_WARNING,
                FALSE
                );
        }

        return true;
    }

    char * error_message = NULL;
    enum mpd_error error = mpd_connection_get_error(cconn);

    if (error != MPD_ERROR_SUCCESS) {
        switch (error) {
        case MPD_ERROR_SYSTEM:
            error_message = compose_error_msg(
                "System-Error",
                g_strerror(mpd_connection_get_system_error(cconn))
                );
            break;
        case MPD_ERROR_SERVER:
            error_message = compose_error_msg(
                "Server-Error",
                mpd_connection_get_error_message(cconn)
                );
            break;
        default:
            error_message = compose_error_msg(
                "Client-Error",
                mpd_connection_get_error_message(cconn)
                );
            break;
        }

        /* Try to clear the error */
        bool is_fatal = !(mpd_connection_clear_error(cconn));

        /* On really fatal error we better disconnect,
         * than using an invalid connection */
        if (handle_fatal && is_fatal) {
            moose_priv_signal_report_event(&self->_signals, "_fatal_error", self);
        }

        /* Dispatch the error to the users */
        moose_shelper_report_error_printf(
            self,
            "ErrID=%d: %s (IsFatal=%s)",
            error,
            error_message,
            is_fatal ? "True" : "False"
            );
        g_free(error_message);
        return true;
    }

    return false;
}

///////////////////////////////

bool moose_shelper_report_error(struct MooseClient * self, struct mpd_connection * cconn)
{
    return moose_shelper_report_error_impl(self, cconn, true);
}

///////////////////////////////

bool moose_shelper_report_error_without_handling(struct MooseClient * self, struct mpd_connection * cconn)
{
    return moose_shelper_report_error_impl(self, cconn, false);
}

///////////////////////////////

void moose_shelper_report_error_printf(
    struct MooseClient * self,
    const char * format, ...)
{
    char * full_string = NULL;
    va_list list_ptr;
    va_start(list_ptr, format);

    if (g_vasprintf(&full_string, format, list_ptr) != 0 && full_string) {
        moose_client_signal_dispatch(self, "logging", self, full_string, MOOSE_LOG_ERROR, FALSE);
    }

    va_end(list_ptr);
}

///////////////////////////////

void moose_shelper_report_progress(
    struct MooseClient * self,
    G_GNUC_UNUSED bool print_newline,
    const char * format, ...)
{
    char * full_string = NULL;
    va_list list_ptr;
    va_start(list_ptr, format);

    if (g_vasprintf(&full_string, format, list_ptr) != 0 && full_string) {
        moose_client_signal_dispatch(self, "logging", self, full_string, MOOSE_LOG_INFO, TRUE);
    }

    va_end(list_ptr);
}

///////////////////////////////

void moose_shelper_report_connectivity(
    struct MooseClient * self,
    const char * new_host,
    int new_port,
    float new_timeout)
{

    g_rec_mutex_lock(&self->_client_attr_mutex);
    bool server_changed = self->is_virgin == false &&
                          ((g_strcmp0(new_host, self->_host) != 0) || (new_port != self->_port));

    /* Defloreate the Client (Wheeeh!) */
    self->is_virgin = false;

    if (self->_host != NULL)
        g_free(self->_host);

    self->_host = g_strdup(new_host);
    self->_port = new_port;
    self->_timeout = new_timeout;
    g_rec_mutex_unlock(&self->_client_attr_mutex);


    /* Dispatch *after* host being set */
    moose_client_signal_dispatch(self, "connectivity",
                                 self, server_changed,
                                 moose_client_is_connected(self)
                                 );
}

///////////////////////////////

static const char * signal_to_name_table[] = {
    [MOOSE_OP_DB_UPDATED]       = "Finished: Database Updated",
    [MOOSE_OP_QUEUE_UPDATED]    = "Finished: Queue Updated",
    [MOOSE_OP_SPL_UPDATED]      = "Finished: Stored Playlist Updated",
    [MOOSE_OP_SPL_LIST_UPDATED] = "Finished: List of Playlists Updated"
};

static const unsigned signal_to_name_table_size = sizeof(signal_to_name_table) / sizeof(const char *) / 2;

void moose_shelper_report_operation_finished(
    struct MooseClient * self,
    MooseOpFinishedEnum op)
{
    g_assert(self);
    const char * operation;

    if (op < signal_to_name_table_size) {
        operation = signal_to_name_table[op];
        moose_client_signal_dispatch(self, "logging", self, operation, MOOSE_LOG_INFO, FALSE);
    }
}
