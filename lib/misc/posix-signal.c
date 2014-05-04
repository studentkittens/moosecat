#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <glib.h>

#ifdef G_OS_UNIX
#include <execinfo.h>
#endif


#include "posix-signal.h"
#include "bug-report.h"

static volatile MooseClient *gl_client = NULL;

///////////////////////////////

static void moose_print_backtrace(void)
{
#ifdef G_OS_UNIX
    const int bt_length = 10;
    void *bt_array[bt_length];
    int bt_size;
    char **calls = NULL;
    bt_size = backtrace(bt_array, bt_length);
    calls = backtrace_symbols(bt_array, bt_size);

    if (calls != NULL) {
        g_print("\nLast %02d function calls shown:\n", bt_length);
        g_print("=============================\n\n");

        for (int i = 0; i < bt_size; i++) {
            g_print(" [%02d] %s\n", i + 1, calls[i]);
        }

        g_print("\n");
        free(calls);
    }

#endif
}

///////////////////////////////

static void moose_priv_signal_handler(int signal)
{
    switch (signal) {
    case SIGFPE:
    case SIGABRT:
    case SIGSEGV:
        g_print("\n\nFATAL: libmoosecat received a terminal signal: %s\n\n", g_strsignal(signal));
        gchar *bug_report = moose_misc_bug_report((MooseClient *) gl_client);
        g_print("%s", bug_report);
        g_free(bug_report);
        moose_print_backtrace();
        g_print("Most recent call first. I'm going to die now. Please debug me.\n\n");
        break;
    }
}

///////////////////////////////

void moose_misc_register_posix_signal(MooseClient *client)
{
    if (client == NULL)
        return;

    /* signal() (nor sigaction) doesn't have a way to pass user data to it. */
    gl_client = client;
    int catch_signals[] = {SIGSEGV, SIGABRT, SIGFPE};

    for (int i = 0; i < (int)(sizeof(catch_signals) / sizeof(int)); i++)
        signal(catch_signals[i], moose_priv_signal_handler);
}
