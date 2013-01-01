#include "client-private.h"
#include "signal-helper.h"
#include "outputs.h"

void mc_proto_outputs_init (mc_Client * self)
{
    g_assert (self);

    self->outputs.list = NULL;
}

///////////////////

void mc_proto_outputs_update (mc_Client * self, enum mpd_idle event, void * unused)
{
    (void) unused;
    g_assert (self);

    if ( (event & MPD_IDLE_OUTPUT) == 0)
        return /* because of no relevant event */;

    BEGIN_COMMAND {
        if (mpd_send_outputs (conn) == false) {
            mc_shelper_report_error (self, conn);
        } else {
            struct mpd_output * output = NULL;
            while ( (output = mpd_recv_output (conn) ) != NULL) {
                self->outputs.list = g_list_prepend (self->outputs.list, output);
            }
        }

    } END_COMMAND;
}

///////////////////

static struct mpd_output * mc_proto_outputs_find (mc_Client * self, const char * output_name)
{
    g_assert (self);

    for (GList * iter = self->outputs.list; iter; iter = iter->next) {
        struct mpd_output * output = iter->data;
        if (g_strcmp0 (mpd_output_get_name (output), output_name) == 0) {
            return output;
        }
    }
    return NULL;
}

///////////////////

int mc_proto_outputs_name_to_id (mc_Client * self, const char * output_name)
{
    g_assert (self);

    struct mpd_output * op = mc_proto_outputs_find (self, output_name);
    if (op == NULL) {
        return -1;
    } else {
        return mpd_output_get_id(op);
    }
}

///////////////////

int mc_proto_outputs_is_enabled (mc_Client * self, const char * output_name)
{
    g_assert (self);

    struct mpd_output * op = mc_proto_outputs_find (self, output_name);
    if (op == NULL) {
        return -1;
    } else {
        return CLAMP(mpd_output_get_enabled(op), 0, 1);
    }
}

///////////////////

void mc_proto_outputs_free (mc_Client * self)
{
    g_assert (self);

    if (self->outputs.list != NULL) {
        g_list_free_full (self->outputs.list, (GDestroyNotify) mpd_output_free);
    }
}
