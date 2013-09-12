#include "client.h"
#include "signal-helper.h"
#include "update.h"
#include "outputs.h"

mc_OutputsData * mc_priv_outputs_new(mc_Client *self)
{
    g_assert(self);

    mc_OutputsData * data = g_slice_new0(mc_OutputsData);
    data->client = self;
    data->outputs = g_hash_table_new_full(
            g_str_hash,
            g_direct_equal,
            NULL,
            (GDestroyNotify) mpd_output_free
    );
    
    return data;
}

///////////////////

void mc_priv_outputs_update(mc_OutputsData *data, enum mpd_idle event)
{
    g_assert(data);
    g_assert(data->client);

    if ((event & MPD_IDLE_OUTPUT) == 0)
        return /* because of no relevant event */;

    struct mpd_connection *conn = mc_get(data->client);

    if(conn != NULL) {
        if (mpd_send_outputs(conn) == false) {
            mc_shelper_report_error(data->client, conn);
        } else {
            struct mpd_output *output = NULL;
            GHashTable * new_table = g_hash_table_new_full(
                g_str_hash,
                g_direct_equal,
                NULL,
                (GDestroyNotify) mpd_output_free
            );

            while ((output = mpd_recv_output(conn)) != NULL) {
                g_hash_table_insert(
                        new_table,
                        (gpointer)mpd_output_get_name(output),
                        output
                );
            }
    
            /* Lock before setting the actual data
             * (in case someone is reading it right now)
             */
            mc_lock_outputs(data->client);
            {
                g_hash_table_destroy(data->outputs);
                data->outputs = new_table;
            }
            mc_unlock_outputs(data->client);
        }

        if(mpd_response_finish(conn) == false) {
            mc_shelper_report_error(data->client, conn);
        }
    }
    mc_put(data->client);
}

///////////////////

static struct mpd_output *mc_priv_outputs_find(mc_OutputsData *data, const char *output_name) 
{
    g_assert(data);

    return g_hash_table_lookup(data->outputs, output_name);
}

///////////////////

int mc_priv_outputs_name_to_id(mc_OutputsData *data, const char *output_name)
{
    g_assert(data);

    struct mpd_output *op = mc_priv_outputs_find(data, output_name);
    int result = -1;
    if (op != NULL) {
        result = mpd_output_get_id(op);
    }


    return result;
}

///////////////////

bool mc_priv_outputs_get_state(mc_OutputsData *data, const char *output_name)
{
    g_assert(data);

    struct mpd_output *op = mc_priv_outputs_find(data, output_name);
    int result = -1;

    if (op != NULL) {
        result = CLAMP(mpd_output_get_enabled(op), 0, 1);
    }

    return result;
}

///////////////////

const char ** mc_priv_outputs_get_names(mc_OutputsData *data) 
{
    g_assert(data);

    /* Get a list of keys in the dictionary (all names) */
    GList *keys = g_hash_table_get_keys(data->outputs);
    
    /* Allocate enough memory to conver the glist in keys to a 
     * normally malloc nullterminated block */
    const char **outputs = g_malloc0(
            (g_hash_table_size(data->outputs) + 1) * sizeof(const char *)
    );

    int output_cursor = 0;

    for(GList *iter = keys; iter; iter = iter->next) {
        struct mpd_output * output = g_hash_table_lookup(data->outputs, iter->data);
        if(output != NULL) {
            outputs[output_cursor++] = mpd_output_get_name(output);
        }
    }

    g_list_free(keys);

    return outputs;
}

///////////////////

bool mc_priv_outputs_set_state(mc_OutputsData *data, const char *output_name, bool state)
{
    g_assert(data);

    bool found = false;
    struct mpd_output *op = mc_priv_outputs_find(data, output_name);
    if(op != NULL) {
        if(!!mpd_output_get_enabled(op) != !!state) {
            found = true;
            char * output_switch_cmd = g_strdup_printf(
                    "output_switch %s %d", output_name, (state) ? 1 : 0
            );
            mc_client_send(data->client, output_switch_cmd);
            g_free(output_switch_cmd);
        }
    }

    return found;
}

///////////////////

void mc_priv_outputs_destroy(mc_OutputsData *data)
{
    g_assert(data);

    mc_lock_outputs(data->client);

    g_hash_table_destroy(data->outputs);
    data->outputs = NULL;

    mc_unlock_outputs(data->client);

    g_slice_free(mc_OutputsData, data);
}
