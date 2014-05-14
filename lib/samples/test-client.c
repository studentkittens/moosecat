#include "../moose-api.h"
#include "../util/moose-util-job-manager.h"

static void send_some_commands(MooseClient * client)
{
    for (int i = 0; i < 100; ++i) {
        moose_client_send(client, "pause");
        moose_client_send(client, "pause");
    }
}

static void * execute(
    G_GNUC_UNUSED struct MooseJobManager * jm,
    G_GNUC_UNUSED volatile bool * cancel,
    void * user_data,
    G_GNUC_UNUSED void * job_data)
{
    MooseClient * client = user_data;
    moose_signal_dispatch(client, "logging", client, "test123", MOOSE_LOG_ERROR, FALSE);
    send_some_commands(client);
    return NULL;
}

static void event_cb(
    G_GNUC_UNUSED MooseClient * client,
    enum mpd_idle events,
    G_GNUC_UNUSED gpointer user_data)
{
    g_printerr("Client Event = %u\n", events);
}

static void logging_cb(
    G_GNUC_UNUSED MooseClient * client,
    const char * message,
    G_GNUC_UNUSED MooseLogLevel level,
    G_GNUC_UNUSED gpointer user_data)
{
    g_printerr("Logging: %s on %p\n", message, g_thread_self());
}

static gboolean timeout_cb(gpointer user_data)
{
    GMainLoop * loop = user_data;
    g_printerr("LOOP STOP\n");
    g_main_loop_quit(loop);
    return FALSE;
}

int main(int argc, char const * argv[])
{
    MooseClient * client = NULL;
    if (argc > 1 && g_strcmp0(argv[1], "-c") == 0) {
        g_printerr("Using CMND ProtocolMachine.\n");
        client = moose_create(MOOSE_PM_COMMAND);
    } else {
        g_printerr("Using IDLE ProtocolMachine.\n");
        client = moose_create(MOOSE_PM_IDLE);
    }

    struct MooseJobManager * jm = moose_jm_create(execute, client);
    moose_signal_add(client, "logging", logging_cb, NULL);
    moose_signal_add(client, "client-event", event_cb, NULL);

    char * error = moose_connect(client, NULL, "localhost", 6666, 2.0);
    if (error == NULL) {
        // long job = moose_jm_send(jm, 0, NULL);
        // send_some_commands(client);
        // moose_jm_wait_for_id(jm, job);
        // g_print("After wait.\n");
    } else {
        g_printerr("Uh, cannot connect: %s\n", error);
    }

    GMainLoop * loop = g_main_loop_new(NULL, TRUE);
    g_timeout_add(10000, timeout_cb, loop);
    g_main_loop_run(loop);
    g_printerr("MAIN LOOP RUN OVER\n");

    moose_jm_wait(client->jm);
    moose_disconnect(client);
    moose_free(client);
    g_main_loop_unref(loop);
    moose_jm_close(jm);
    return 0;
}
