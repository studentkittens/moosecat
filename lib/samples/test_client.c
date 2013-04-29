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
    send_some_commands(client);
    return NULL;
}

int main(int argc, char const *argv[])
{
    mc_Client *client = mc_proto_create(MC_PM_COMMAND);
    struct mc_JobManager *jm = mc_jm_create(execute, client);

    char * error = mc_proto_connect(client, NULL, "localhost", 6600, 2.0);
    if(error == NULL) {
        int job = mc_jm_send(jm, 0, NULL);
        send_some_commands(client);
        mc_jm_wait_for_id(jm, job);
    } else {
        g_print("Uh, cannot connect: %s\n", error);
    }

    mc_jm_wait(client->jm);
    mc_proto_disconnect(client);
    mc_proto_free(client);

    return 0;
}
