#include "../moose-api.h"
#include "../misc/moose-misc-job-manager.h"
#include <glib.h>

static void send_some_commands(MooseClient * client) {
    for (int i = 0; i < 100; ++i) {
        moose_client_send(client, "pause");
        moose_client_send(client, "pause");
    }
}

static void * execute(
    G_GNUC_UNUSED MooseJobManager * jm,
    G_GNUC_UNUSED volatile gboolean * cancel,
    G_GNUC_UNUSED void * job_data,
    void * user_data) {
    MooseClient * client = user_data;
    moose_warning("test.");
    send_some_commands(client);
    return NULL;
}

static void event_cb(
    G_GNUC_UNUSED MooseClient * client,
    MooseIdle events,
    G_GNUC_UNUSED gpointer user_data) {
    g_printerr("Client Event = %u\n", events);
}

static gboolean timeout_cb(gpointer user_data) {
    GMainLoop * loop = user_data;
    g_printerr("LOOP STOP\n");
    g_main_loop_quit(loop);
    return FALSE;
}

int main(int argc, char const * argv[]) {
    MooseClient * client = NULL;
    if (argc > 1 && g_strcmp0(argv[1], "-c") == 0) {
        g_printerr("Using CMND ProtocolMachine.\n");
        client = moose_client_new(MOOSE_PROTOCOL_IDLE);
    } else {
        g_printerr("Using IDLE ProtocolMachine.\n");
        client = moose_client_new(MOOSE_PROTOCOL_IDLE);
    }

    MooseJobManager * jm = moose_job_manager_new();
    g_signal_connect(jm, "dispatch", G_CALLBACK(execute), client);
    g_signal_connect(client, "client-event", G_CALLBACK(event_cb), NULL);

    char * error = moose_client_connect(client, NULL, "localhost", 6600, 2.0);
    if (error == NULL) {
        // long job = moose_job_manager_send(jm, 0, NULL);
        // send_some_commands(client);
        // moose_job_manager_wait_for_id(jm, job);
        // g_print("After wait.\n");
    } else {
        g_printerr("Uh, cannot connect: %s\n", error);
    }

    GMainLoop * loop = g_main_loop_new(NULL, TRUE);
    g_timeout_add(10000, timeout_cb, loop);
    g_main_loop_run(loop);
    g_printerr("MAIN LOOP RUN OVER\n");

    moose_job_manager_wait(jm);
    moose_client_disconnect(client);
    moose_client_unref(client);
    g_main_loop_unref(loop);
    moose_job_manager_unref(jm);
    return 0;
}
