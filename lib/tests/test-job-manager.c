#include <glib.h>
#include "../misc/moose-misc-job-manager.h"
#include "../moose-api.h"

static gpointer _on_execute(MooseJobManager *self, volatile gboolean *cancel, void *job_data, gpointer user_data) {
    g_assert(self == user_data);
    g_assert(cancel);
    g_assert(job_data == GINT_TO_POINTER(0x2));
    g_assert(moose_job_manager_check_cancel(self, cancel) == FALSE);
    return GINT_TO_POINTER(0x1);
}

static void test_launch_job_manager(void) {
    MooseJobManager *self = moose_job_manager_new();
    g_signal_connect(self, "dispatch", G_CALLBACK(_on_execute), self);
    long job_id = moose_job_manager_send(self, 0, GINT_TO_POINTER(0x2));
    moose_job_manager_wait_for_id(self, job_id);
    g_assert(moose_job_manager_get_result(self, job_id) == GINT_TO_POINTER(0x1));
    moose_job_manager_wait(self);
    moose_job_manager_unref(self);
}

int main (int argc, char **argv) {
    moose_debug_install_handler();
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/misc/job_manager", test_launch_job_manager);
    return g_test_run();
}
