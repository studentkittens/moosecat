#include "client-private.h"
#include "signal-helper.h"
#include "outputs.h"

void mc_proto_outputs_init (mc_Client * self)
{
    g_assert (self);

    self->outputs.size = 0;
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
            if (self->outputs.list != NULL) {
                g_free (self->outputs.list);
                self->outputs.list = NULL;
                self->outputs.size = 0;
            }
            struct mpd_output * output = NULL;
            while ( (output = mpd_recv_output (conn) ) != NULL) {
                self->outputs.size += 1;
                self->outputs.list = g_realloc_n(
                        self->outputs.list,
                        self->outputs.size,
                        sizeof(struct mpd_output *));

                self->outputs.list[self->outputs.size - 1] = output;
            }
        }

    } END_COMMAND;
}

///////////////////

static struct mpd_output * mc_proto_outputs_find (mc_Client * self, const char * output_name)
{
    g_assert (self);

    if (self->outputs.list != NULL) {
        for (int i = 0; i< self->outputs.size; ++i) {
            struct mpd_output * output = self->outputs.list[i];
            if (g_strcmp0 (mpd_output_get_name (output), output_name) == 0) {
                return output;
            }
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
        for (int i = 0; i< self->outputs.size; ++i) {
            mpd_output_free(self->outputs.list[i]);
        }
        g_free(self->outputs.list);
    }
}
