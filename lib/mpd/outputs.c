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

    if ((event & MPD_IDLE_OUTPUT) == 0)
        return /* because no relevant event */;

    BEGIN_COMMAND {
        if (mpd_send_outputs (conn) == false) {
            mc_shelper_report_error (self, conn);
        } else {
            struct mpd_output * output = NULL;
            while ( (output = mpd_recv_output (conn)) != NULL) {
                self->outputs.list = g_list_prepend (self->outputs.list, output);
            }
        }

    } END_COMMAND;
}

///////////////////

int mc_proto_outputs_id_to_name (mc_Client * self, const char * output_name)
{
    g_assert (self);

    int rc = -1;

    for (GList * iter = self->outputs.list; iter; iter = iter->next) {
        struct mpd_output * output = iter->data;
        if (g_strcmp0(mpd_output_get_name (output), output_name) == 0) {
            rc = mpd_output_get_id (output);
        }
    }

    return rc;
}

///////////////////

void mc_proto_outputs_free (mc_Client * self)
{
    g_assert (self);

    for (GList * iter = self->outputs.list; iter; iter = iter->next) {
        struct mpd_output * output = iter->data;
        if (output != NULL) {
            mpd_output_free (output);
        }
    }
}
