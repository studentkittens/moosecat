#include "protocol.h"
#include "signal-helper.h"
#include "update.h"
#include "outputs.h"

#include <glib/gprintf.h>
#include <string.h>

///////////////////////////////

static char * compose_error_msg(const char *topic, const char *src)
{
    if (src && topic) {
        return g_strdup_printf("%s: ,,%s''", topic, src);
    } else {
        return NULL;
    }
}

///////////////////////////////

static bool mc_shelper_report_error_impl(struct mc_Client *self, struct mpd_connection *cconn, bool handle_fatal)
{
    if (self == NULL || cconn == NULL) {
        if (self != NULL) {
            mc_signal_dispatch(
                    self, "logging", self,
                    "Connection to MPD was closed (IsFatal=maybe?)",
                    MC_LOG_WARNING,
                    FALSE 
            );
        }

        return true;
    }

    char *error_message = NULL;
    enum mpd_error error = mpd_connection_get_error(cconn);

    if (error != MPD_ERROR_SUCCESS) {
        switch(error) {
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
            mc_priv_signal_report_event(&self->_signals, "_fatal_error", self);
        }

        /* Dispatch the error to the users */
        mc_shelper_report_error_printf(
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

bool mc_shelper_report_error(struct mc_Client *self, struct mpd_connection *cconn)
{
    return mc_shelper_report_error_impl(self, cconn, true);
}

///////////////////////////////

bool mc_shelper_report_error_without_handling(struct mc_Client *self, struct mpd_connection *cconn)
{
    return mc_shelper_report_error_impl(self, cconn, false);
}

///////////////////////////////

void mc_shelper_report_error_printf(
    struct mc_Client *self,
    const char *format, ...)
{
    char *full_string = NULL;
    va_list list_ptr;
    va_start(list_ptr, format);

    if (g_vasprintf(&full_string, format, list_ptr) != 0 && full_string) {
        mc_signal_dispatch(self, "logging", self, full_string, MC_LOG_ERROR, FALSE);
    }

    va_end(list_ptr);
}

///////////////////////////////

void mc_shelper_report_progress(
    struct mc_Client *self,
    bool print_newline,
    const char *format, ...)
{
    char *full_string = NULL;
    va_list list_ptr;
    va_start(list_ptr, format);

    if (g_vasprintf(&full_string, format, list_ptr) != 0 && full_string) {
        mc_signal_dispatch(self, "logging", self, full_string, MC_LOG_INFO, TRUE);
    }

    va_end(list_ptr);
}

///////////////////////////////

void mc_shelper_report_connectivity(
    struct mc_Client *self,
    const char *new_host,
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
    mc_signal_dispatch(self, "connectivity", 
            self, server_changed,
            mc_is_connected(self)
    );
}

///////////////////////////////

static const char *signal_to_name_table[] = {
    [MC_OP_DB_UPDATED]       = "Finished: Database Updated",
    [MC_OP_QUEUE_UPDATED]    = "Finished: Queue Updated",
    [MC_OP_SPL_UPDATED]      = "Finished: Stored Playlist Updated",
    [MC_OP_SPL_LIST_UPDATED] = "Finished: List of Playlists Updated"
};

static const unsigned signal_to_name_table_size = sizeof(signal_to_name_table) / sizeof(const char *) / 2;

void mc_shelper_report_operation_finished(
    struct mc_Client *self,
    mc_OpFinishedEnum op)
{
    g_assert(self);
    const char *operation;

    if(op < signal_to_name_table_size) {
        operation = signal_to_name_table[op];
        mc_signal_dispatch(self, "logging", self, operation, MC_LOG_INFO, FALSE);
    }
}
