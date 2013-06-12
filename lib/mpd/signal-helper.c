#include "protocol.h"
#include "signal-helper.h"
#include "update.h"
#include "outputs.h"

#include <glib/gprintf.h>
#include <string.h>

///////////////////////////////

static void compose_error_msg(const char *topic, char *dest, const char *src)
{
    if (src && topic) {
        g_snprintf(dest, _MC_PROTO_MAX_ERR_LEN, "%s: ,,%s''", topic, src);
    }
}

///////////////////////////////

static bool mc_shelper_report_error_impl(struct mc_Client *self, struct mpd_connection *cconn, bool handle_fatal)
{
    if (self == NULL || cconn == NULL) {
        if (self != NULL) {
            mc_proto_signal_dispatch(
                    self, "logging", self,
                    "Connection to MPD was closed (IsFatal=maybe?)",
                    true
            );
        }

        return true;
    }

    enum mpd_error error = mpd_connection_get_error(cconn);

    if (error != MPD_ERROR_SUCCESS) {
        if (MPD_ERROR_SYSTEM == error) {
            compose_error_msg("System-Error", self->error_buffer,
                              g_strerror(mpd_connection_get_system_error(cconn)));
        } else if (MPD_ERROR_SERVER == error) {
            compose_error_msg("Server-Error", self->error_buffer,
                              mpd_connection_get_error_message(cconn));
        } else {
            compose_error_msg("Client-Error", self->error_buffer,
                              mpd_connection_get_error_message(cconn));
        }

        /* Try to clear the error */
        bool is_fatal = !(mpd_connection_clear_error(cconn));

        /* On really fatal error we better disconnect,
         * than using an invalid connection */
        if (handle_fatal && is_fatal) {
            mc_proto_disconnect(self);
        }

        /* Dispatch the error to the users */
        mc_shelper_report_error_printf(
                self,
                "ErrID=%d: %s (IsFatal=%s)",
                error,
                self->error_buffer,
                is_fatal ? "True" : "False"
        );
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
        mc_proto_signal_dispatch(self, "logging", self, full_string, MC_LOG_ERROR);
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
        mc_proto_signal_dispatch(self, "logging", self, full_string, MC_LOG_INFO);
        g_free(full_string);
    }

    va_end(list_ptr);
}

///////////////////////////////

void mc_shelper_report_connectivity(
    struct mc_Client *self,
    const char *new_host,
    int new_port)
{
    bool server_changed = (g_strcmp0(new_host, self->_host) != 0) || (new_port != self->_port);

    if (self->_host != NULL)
        g_free(self->_host);

    self->_host = g_strdup(new_host);
    self->_port = new_port;
    /* Dispatch *after* host being set */
    mc_proto_signal_dispatch(self, "connectivity", self, server_changed);
}

///////////////////////////////

void mc_shelper_report_client_event(
    struct mc_Client *self,
    enum mpd_idle event)
{
    if (event != 0) {
        mc_proto_force_sss_update(self, event);
        mc_proto_signal_dispatch(self, "client-event", self, event);
    }
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
    if(op >= 0 && op < signal_to_name_table_size) {
        operation = signal_to_name_table[op];
        mc_proto_signal_dispatch(self, "logging", self, operation, MC_LOG_INFO);
    }
}
