#include "api.h"

#include <string.h>
#include <stdio.h>

///////////////////////////////

static void print_progress(mc_Client *self, bool print_newline, const char *msg, void *userdata)
{
    (void) self;
    (void) userdata;
    g_print("%s                                     %c", msg, print_newline ? '\n' : '\r');
}

///////////////////////////////

static void print_error(mc_Client *self, enum mpd_error err,  const char *err_msg, bool is_fatal, void *userdata)
{
    (void) self;
    (void) userdata;
    (void) err;
    (void) is_fatal;
    g_print("ERROR: %s\n", err_msg);
}

///////////////////////////////

static void print_event(mc_Client *self, enum mpd_idle event, void *user_data)
{
    g_print("Usercallback: %p %d %p\n", self, event, user_data);
}

///////////////////////////////

int main(void)
{
    mc_Client *client = mc_proto_create(MC_PM_IDLE);
    mc_proto_connect(client, NULL, "localhost", 6600, 10.0);
    mc_proto_signal_add(client, "progress", print_progress, NULL);
    mc_proto_signal_add(client, "error", print_error, NULL);
    mc_misc_register_posix_signal(client);
    mc_proto_signal_add(client, "client-event", print_event, NULL);
    GMainLoop *loop = g_main_loop_new(NULL, true);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    mc_proto_disconnect(client);
    mc_proto_free(client);
}
