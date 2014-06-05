#include "moose-debug.h"
#include "moose-config.h"

#include <gio/gio.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

static int moose_print_trace(void) {
    GError *error = NULL;
    gchar pidstr[16];
    gsize bytes_read = 0;
    gint exit_status;

    GInputStream * stdout_stream = NULL;
    GSubprocess *gdb = NULL;

    /* Get the PID of our process */
    g_snprintf(pidstr, sizeof(pidstr), "%d", getpid ());

    gdb = g_subprocess_new(
              G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
              &error,
              "/usr/bin/gdb", "-q", "-nw", "-batch", "-p", pidstr,
              "-ex", "thread apply all bt full",
              NULL
          );

    if(error != NULL) {
        moose_warning("cannot launch gdb-subprocess: %s", error->message);
        g_error_free(error);
        exit_status = EXIT_FAILURE;
        goto cleanup;
    }

    stdout_stream = g_subprocess_get_stdout_pipe(gdb);
    if(stdout_stream == NULL) {
        moose_critical("could not read stdout of gdb-subprocess.");
    }

    unsigned char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    while(g_input_stream_read_all(stdout_stream, buffer, sizeof(buffer) - 1, &bytes_read, NULL, &error)) {
        if(bytes_read == 0) {
            break;
        }
        buffer[bytes_read] = '\0';
        fputs((char *)buffer, stderr);
    }

    g_subprocess_wait(gdb, NULL, &error);
    if(error != NULL) {
        moose_warning("could not wait for gdb finishing. Sorry.");
        g_error_free(error);
        exit_status = EXIT_FAILURE;
        goto cleanup;
    }

    exit_status = g_subprocess_get_exit_status(gdb);

cleanup:
    if(gdb) {
        g_object_unref(gdb);
    }
    return exit_status;
}

static void moose_signal_handler(int signum) {
    g_printerr(
        "\n\n////// BACKTRACE //////\n\n"
        "If you see something like: \"ptrace: Operation not allowed\"\n"
        "then issue the following command and try again:\n\n"
        "    echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope\n\n"
    );

    moose_print_trace();

    g_printerr(
        "\n\n////// INFORMATION //////\n\n"
        "libmoosecat (%s) received a signal that caused a backtrace to be printed.\n"
        "If you did not trigger this willfully, then this probably means that\n"
        "we just crashed. The exact signal was: %s.\n"
        "\n"
        "Please include this info above when submitting a bug-report.\n"
        "Bug reports can be submitted at: https://github.com/sahib/moosecat\n"
        "Thanks for your help.\n",
        moose_debug_version(),
        g_strsignal(signum)
    );
}

void moose_debug_install_handler(void) {
    GArray *signals = g_array_new(FALSE, FALSE, sizeof(gint));
    int signum;
    signum = SIGUSR1;
    g_array_append_val(signals, signum);
    signum = SIGSEGV;
    g_array_append_val(signals, signum);
    signum = SIGABRT;
    g_array_append_val(signals, signum);
    signum = SIGFPE;
    g_array_append_val(signals, signum);
    moose_debug_install_handler_full(signals);
    g_array_free(signals, TRUE);
}

void moose_debug_install_handler_full(GArray *signals) {
    g_assert(signals);

    moose_message("This is PID %d", getpid());

    for(unsigned i = 0; i < signals->len; ++i) {
        int signum = g_array_index(signals, gint, i);

        if(signal(signum, moose_signal_handler) != NULL) {
            moose_warning(
                "cannot install signal handler (#%d: %s).", signum, g_strsignal(signum)
            );
        }
    }
}

const char * moose_debug_version(void) {
    return "libmoosecat " MOOSE_VERSION " (" MOOSE_VERSION_GIT_REVISION ")";
}

#if 0

#include <unistd.h>

static void dummy2(void) {
    pause();
}

static void dummy1(void) {
    dummy2();
}

int main () {
    moose_debug_install_handler();
    g_printerr("Run: kill -USR1 %d\n", getpid());
    dummy1();
    return EXIT_SUCCESS;
}

#endif
