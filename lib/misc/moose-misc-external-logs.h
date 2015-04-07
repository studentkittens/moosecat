#ifndef MC_EXTERNAL_LOGS
#define MC_EXTERNAL_LOGS

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * moose_misc_catch_external_logs:
 * @object: Any object that has a matching `log-message` signal.
 *
 * Utiltiy function to redirect GLib's logging system to error signals. The
 * passed in GObject needs to have a matching `log-message` signal.  Inside of
 * this library, this is the case for MooseCLient.
 */
void moose_misc_catch_external_logs(GObject *object);

G_END_DECLS

#endif /* end of include guard: MC_EXTERNAL_LOGS */
