#include <glib.h>

#include "moose-misc-external-logs.h"
#include "../moose-config.h"



static void forward_log(const gchar * log_domain,
                        GLogLevelFlags log_level,
                        const gchar * message,
                        G_GNUC_UNUSED gpointer user_data) {
    const char * log_level_string = NULL;

    switch (log_level) {
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

    // TODO
    g_printerr("%s-%s: %s\n", log_domain, log_level_string, message);
}



static void moose_register_log_domains(const char * domain) {
    GLogLevelFlags flags = (G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION);
    g_log_set_handler(domain, flags, forward_log, NULL);
}



void moose_misc_catch_external_logs(void) {
    moose_register_log_domains("GLib");
    moose_register_log_domains("GLib-GObject");
}
