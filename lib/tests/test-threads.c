#include <glib.h>
#include <time.h>
#include "../moose-api.h"
#include "../misc/moose-misc-threads.h"

static MooseThreads *threads;
static int n = 0, N = 10;
static GMainLoop *loop = NULL;

static void *thread(G_GNUC_UNUSED MooseThreads *self, void *data,
                    G_GNUC_UNUSED void *user_data) {
    g_assert(self == threads);
    g_usleep(1 * 1000 * 1000 + 1000); /* 1.1 seconds. */
    return data;                      /* Forward old timestamp */
}

static gboolean idle_cb(gpointer user_data) {
    g_main_loop_quit((GMainLoop *)user_data);
    return FALSE;
}

static void *deliver(G_GNUC_UNUSED MooseThreads *self, void *data,
                     G_GNUC_UNUSED void *user_data) {
    g_assert(self == threads);
    time_t now = time(NULL);
    time_t old = GPOINTER_TO_INT(data);
    g_assert(now - old == 1);

    if(++n == N) {
        g_idle_add(idle_cb, loop);
    }
    return NULL;
}

static void test_launch_threads(void) {
    threads = moose_threads_new(10);
    g_signal_connect(threads, "thread", G_CALLBACK(thread), NULL);
    g_signal_connect(threads, "deliver", G_CALLBACK(deliver), NULL);

    for(int i = 0; i < 10; ++i) {
        moose_threads_push(threads, (void *)(long) time(NULL));
    }

    loop = g_main_loop_new(NULL, TRUE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    moose_threads_unref(threads);
}

int main(int argc, char **argv) {
    moose_debug_install_handler();
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/misc/threads", test_launch_threads);
    return g_test_run();
}
