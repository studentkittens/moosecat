#include <glib.h>

#include "moose-misc-external-logs.h"

//////////////////////

static void forward_log(const gchar *log_domain,
                        GLogLevelFlags log_level,
                        const gchar *message,
                        gpointer user_data) {
    GObject *object = user_data;
    const char *log_level_string = NULL;

    switch(log_level) {
    case G_LOG_LEVEL_CRITICAL:
        log_level_string = "Critical";
        break;

    case G_LOG_LEVEL_ERROR:
        log_level_string = "Error";
        break;

    case G_LOG_LEVEL_WARNING:
        log_level_string = "Warning";
        break;

    case G_LOG_LEVEL_MESSAGE:
        log_level_string = "Message";
        break;

    case G_LOG_LEVEL_INFO:
        log_level_string = "Info";
        break;

    case G_LOG_LEVEL_DEBUG:
        log_level_string = "Debug";
        break;

    default:
        log_level_string = "Unknown";
    }

    g_signal_emit_by_name(object, "log-message", log_domain, log_level_string, message);
}

//////////////////////

static void moose_register_log_domains(const char *domain, GObject *object) {
    GLogLevelFlags flags = (G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION);
    g_log_set_handler(domain, flags, forward_log, object);
}

//////////////////////

void moose_misc_catch_external_logs(GObject *object) {
    moose_register_log_domains("GLib", object);
    moose_register_log_domains("GLib-GObject", object);
    moose_register_log_domains("Moose", object);
}
