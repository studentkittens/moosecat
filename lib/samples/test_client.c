#include "../api.h"
#include "../util/job-manager.h"


static void send_some_commands(mc_Client *client)
{
    for(int i = 0; i < 100; ++i) {
        mc_client_send(client, "pause");
        mc_client_send(client, "pause");
    }
}

static void * execute(struct mc_JobManager *jm, volatile bool *cancel, void *user_data, void *job_data)
{
    mc_Client *client = user_data;
    mc_signal_dispatch(client, "logging", client, "test123", MC_LOG_ERROR, FALSE);
    send_some_commands(client);
    return NULL;
}

static void event_cb(mc_Client *client, enum mpd_idle events, gpointer user_data)
{
    g_printerr("Client Event = %u\n", events);
}

static void logging_cb(mc_Client *client, const char *message, mc_LogLevel level, gpointer user_data)
{
    g_printerr("Logging: %s on %p\n", message, g_thread_self());
}

static gboolean timeout_cb(gpointer user_data)
{
    GMainLoop *loop = user_data;
    g_printerr("LOOP STOP\n");
    g_main_loop_quit(loop);
    return FALSE;
}

int main(int argc, char const *argv[])
{
    mc_Client * client = NULL;
    if(argc > 1 && g_strcmp0(argv[1], "-c") == 0)  {
        g_printerr("Using CMND ProtocolMachine.\n");
        client = mc_create(MC_PM_COMMAND);
    } else {
        g_printerr("Using IDLE ProtocolMachine.\n");
        client = mc_create(MC_PM_IDLE);
    }

    struct mc_JobManager *jm = mc_jm_create(execute, client);
    mc_signal_add(client, "logging", logging_cb, NULL);
    mc_signal_add(client, "client-event", event_cb, NULL);

    char * error = mc_connect(client, NULL, "localhost", 6666, 2.0);
    if(error == NULL) {
        // long job = mc_jm_send(jm, 0, NULL);
        // send_some_commands(client);
        // mc_jm_wait_for_id(jm, job);
        // g_print("After wait.\n");
    } else {
        g_printerr("Uh, cannot connect: %s\n", error);
    }

    GMainLoop *loop = g_main_loop_new(NULL, TRUE);
    g_timeout_add(10000, timeout_cb, loop);
    g_main_loop_run(loop);
    g_printerr("MAIN LOOP RUN OVER\n");

    mc_jm_wait(client->jm);
    mc_disconnect(client);
    mc_free(client);
    g_main_loop_unref(loop);
    mc_jm_close(jm);
    return 0;
}
