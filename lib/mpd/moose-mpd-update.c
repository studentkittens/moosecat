#include "moose-mpd-update.h"
#include "moose-mpd-protocol.h"
#include "moose-mpd-signal-helper.h"
#include "moose-status-private.h"

////////////////////////

const enum mpd_idle on_status_update = (0
                                        | MPD_IDLE_PLAYER
                                        | MPD_IDLE_OPTIONS
                                        | MPD_IDLE_MIXER
                                        | MPD_IDLE_OUTPUT
                                        | MPD_IDLE_QUEUE);

const enum mpd_idle on_stats_update = (0
                                       | MPD_IDLE_UPDATE
                                       | MPD_IDLE_DATABASE);

const enum mpd_idle on_song_update = (0 | MPD_IDLE_PLAYER);
const enum mpd_idle on_rg_status_update = (0 | MPD_IDLE_OPTIONS);

////////////////////////

/* This will be sended (as Integer)
 * to the Queue to break out of the poll loop
 */
#define THREAD_TERMINATOR (MPD_IDLE_MESSAGE)

/* Little hack:
 *
 * Set a very high bit of the enum bitmask and take it
 * as a flag to indicate a status timer indcued trigger.
 *
 * If so, we may decide to not call the client-event callback.
 */
#define IS_STATUS_TIMER_FLAG (MPD_IDLE_SUBSCRIPTION)


////////////////////////

static void moose_update_data_push_full(MooseUpdateData * data, enum mpd_idle event, bool is_status_timer)
{
    g_assert(data);

    /* We may not push 0 - that would cause the event_queue to shutdown */
    if (event != 0) {
        enum mpd_idle send_event = event;
        if (is_status_timer) {
            event |= IS_STATUS_TIMER_FLAG;
        }

        g_printerr("SENDING EVENT: %d\n", send_event);
        g_async_queue_push(data->event_queue, GINT_TO_POINTER(send_event));
    }
}

////////////////////////

static void moose_update_context_info_cb(struct MooseClient * self, enum mpd_idle events)
{
    if (self == NULL || events == 0 || moose_client_is_connected(self) == false) {
        return;
    }

    struct mpd_connection * conn = moose_client_get(self);
    MooseUpdateData * data = self->_update_data;

    if (conn == NULL) {
        moose_client_put(self);
        return;
    }

    g_printerr("UPDATE CONTEXT\n");

    const bool update_status = (events & on_status_update);
    const bool update_stats = (events & on_stats_update);
    const bool update_song = (events & on_song_update);
    const bool update_rg = (events & on_rg_status_update);

    if (!(update_status || update_stats || update_song || update_rg)) {
        return;
    }

    /* Send a block of commands, speeds the thing up by 2x */
    mpd_command_list_begin(conn, true);
    {
        /* Note: order of recv == order of send. */
        if (update_status)
            mpd_send_status(conn);

        if (update_stats)
            mpd_send_stats(conn);

        if (update_rg)
            mpd_send_command(conn, "replay_gain_status", NULL);

        if (update_song)
            mpd_send_current_song(conn);
    }
    mpd_command_list_end(conn);
    moose_shelper_report_error(self, conn);

    /* Try to receive status */
    if (update_status) {
        struct mpd_status * tmp_status_struct;
        tmp_status_struct = mpd_recv_status(conn);

        if (data->status_timer.last_update != NULL) {
            /* Reset the status timer to 0 */
            g_timer_start(data->status_timer.last_update);
        }

        if (tmp_status_struct) {
            MooseStatus * status = moose_ref_status(self);

            if (data->status != NULL) {
                data->last_song_data.id = moose_status_get_song_id(data->status);
                data->last_song_data.state = moose_status_get_state(data->status);
            } else {
                data->last_song_data.id = -1;
                data->last_song_data.state = MOOSE_STATE_UNKNOWN;
            }

            moose_status_unref(data->status);
            data->status = moose_status_new_from_struct(tmp_status_struct);
            mpd_status_free(tmp_status_struct);
            moose_status_unref(status);
        }

        mpd_response_next(conn);
        moose_shelper_report_error(self, conn);
    }

    /* Try to receive statistics as last */
    if (update_stats) {
        struct mpd_stats * tmp_stats_struct;
        tmp_stats_struct = mpd_recv_stats(conn);

        if (tmp_stats_struct) {
            MooseStatus * status = moose_ref_status(self);
            moose_status_update_stats(status, tmp_stats_struct);
            moose_status_unref(status);
            mpd_stats_free(tmp_stats_struct);
        }

        mpd_response_next(conn);
        moose_shelper_report_error(self, conn);
    }

    /* Read the current replay gain status */
    if (update_rg) {
        struct mpd_pair * mode = mpd_recv_pair_named(conn, "replay_gain_mode");
        if (mode != NULL) {
            MooseStatus * status = moose_ref_status(self);
            if (status != NULL) {
                moose_status_set_replay_gain_mode(status, mode->value);
            }

            moose_status_unref(status);
            mpd_return_pair(conn, mode);
        }

        mpd_response_next(conn);
        moose_shelper_report_error(self, conn);
    }

    /* Try to receive the current song */
    if (update_song) {
        struct mpd_song * new_song_struct = mpd_recv_song(conn);
        MooseSong * new_song = moose_song_new_from_struct(new_song_struct);
        if (new_song_struct != NULL) {
            mpd_song_free(new_song_struct);
        }

        /* We need to call recv() one more time
         * so we end the songlist,
         * it should only return  NULL
         * */
        if (new_song_struct != NULL) {
            struct mpd_song * empty = mpd_recv_song(conn);
            g_assert(empty == NULL);
        }

        MooseStatus * status = moose_ref_status(self);
        moose_status_set_current_song(status, new_song);
        moose_status_unref(status);

        moose_shelper_report_error(self, conn);
    }

    /* Finish repsonse */
    if (update_song || update_stats || update_status || update_rg) {
        mpd_response_finish(conn);
        moose_shelper_report_error(self, conn);
    }

    moose_client_put(self);
}

////////////////////////

void moose_priv_outputs_update(MooseUpdateData * data, MooseClient * client, enum mpd_idle event)
{
    g_assert(data);
    g_assert(data->client);

    if ((event & MPD_IDLE_OUTPUT) == 0)
        return /* because of no relevant event */;

    struct mpd_connection * conn = moose_client_get(data->client);

    if (conn != NULL) {
        if (mpd_send_outputs(conn) == false) {
            moose_shelper_report_error(data->client, conn);
        } else {
            struct mpd_output * output = NULL;
            MooseStatus * status = moose_ref_status(client);
            moose_status_outputs_clear(status);

            while ((output = mpd_recv_output(conn)) != NULL) {
                moose_status_outputs_add(status, output);
                mpd_output_free(output);
            }
        }

        if (mpd_response_finish(conn) == false) {
            moose_shelper_report_error(data->client, conn);
        }
    }
    moose_client_put(data->client);
}

////////////////////////

static bool moose_update_is_a_seek_event(MooseUpdateData * data, enum mpd_idle event_mask)
{
    if (event_mask & MPD_IDLE_PLAYER) {
        long curr_song_id = -1;
        enum mpd_state curr_song_state = MOOSE_STATE_UNKNOWN;

        /* Get the current data */
        MooseStatus * status = moose_ref_status(data->client);
        curr_song_id = moose_status_get_song_id(status);
        curr_song_state = moose_status_get_state(status);
        moose_status_unref(status);

        if (curr_song_id != -1 && data->last_song_data.id == curr_song_id) {
            if (data->last_song_data.state == curr_song_state) {
                return true;
            }
        }
    }
    return false;
}

////////////////////////

static gpointer moose_update_thread(gpointer user_data)
{
    g_assert(user_data);

    MooseUpdateData * data = user_data;

    enum mpd_idle event_mask = 0;

    while ((event_mask = GPOINTER_TO_INT(g_async_queue_pop(data->event_queue))) != THREAD_TERMINATOR) {
        moose_update_context_info_cb(data->client, event_mask);
        moose_priv_outputs_update(data, data->client, event_mask);
        g_printerr("EVENT %d\n", event_mask);

        /* Lookup if we need to trigger a client-event (maybe not if * auto-update)*/
        bool trigger_it = true;
        if (event_mask & IS_STATUS_TIMER_FLAG && data->status_timer.trigger_event) {
            trigger_it = false;
        }

        /* Maybe we should make this configurable? */
        if (moose_update_is_a_seek_event(data, event_mask)) {
            /* Set the PLAYER bit to 0 */
            event_mask &= ~MPD_IDLE_PLAYER;

            /* and add the SEEK bit intead */
            event_mask |= MPD_IDLE_SEEK;
        }

        if (trigger_it) {
            moose_client_signal_dispatch(data->client, "client-event", data->client, event_mask);
        }
    }

    g_printerr("UPDATE EXIT\n");
    return NULL;
}

////////////////////////
// STATUS TIMER STUFF //
////////////////////////

static gboolean moose_update_status_timer_cb(gpointer user_data)
{
    g_assert(user_data);
    struct MooseClient * self = user_data;
    MooseUpdateData * data = self->_update_data;

    if (moose_client_is_connected(self) == false) {
        return false;
    }

    MooseState state = MOOSE_STATE_UNKNOWN;
    MooseStatus * status = moose_ref_status(self);
    if (status != NULL) {
        state = moose_status_get_state(status);
    }
    moose_status_unref(status);

    if (status != NULL) {
        if (state == MOOSE_STATE_PLAY) {

            /* Substract a small amount to include the network latency - a bit of a hack
             * but worst thing that could happen: you miss one status update.
             * */
            float compare = MAX(data->status_timer.interval - data->status_timer.interval / 10, 0);
            float elapsed = g_timer_elapsed(data->status_timer.last_update, NULL) * 1000;

            if (elapsed >= compare) {
                /* MIXER is a harmless event, but it causes status to update */
                moose_update_data_push_full(self->_update_data, MPD_IDLE_MIXER, true);
            }
        }
    }

    return moose_update_status_timer_is_active(self);
}

////////////////////////

// TODO Fix status timer.
void moose_update_register_status_timer(
    struct MooseClient * self,
    int repeat_ms,
    bool trigger_event)
{
    g_assert(self);

    MooseUpdateData * data = self->_update_data;

    g_mutex_lock(&data->status_timer.mutex);
    data->status_timer.trigger_event = trigger_event;
    data->status_timer.last_update = g_timer_new();
    data->status_timer.interval = repeat_ms;
    data->status_timer.timeout_id =
        g_timeout_add(repeat_ms, moose_update_status_timer_cb, self);
    g_mutex_unlock(&data->status_timer.mutex);
}

////////////////////////

void moose_update_unregister_status_timer(
    struct MooseClient * self)
{
    g_assert(self);

    if (moose_update_status_timer_is_active(self)) {
        MooseUpdateData * data = self->_update_data;
        g_mutex_lock(&data->status_timer.mutex);

        if (data->status_timer.timeout_id > 0) {
            g_source_remove(data->status_timer.timeout_id);
        }

        data->status_timer.timeout_id = -1;
        data->status_timer.interval = 0;

        if (data->status_timer.last_update != NULL) {
            g_timer_destroy(data->status_timer.last_update);
        }
        g_mutex_unlock(&data->status_timer.mutex);
    }
}

////////////////////////

bool moose_update_status_timer_is_active(struct MooseClient * self)
{
    g_assert(self);

    MooseUpdateData * data = self->_update_data;
    g_mutex_lock(&data->status_timer.mutex);
    bool result = (self->_update_data->status_timer.timeout_id != -1);
    g_mutex_unlock(&data->status_timer.mutex);

    return result;
}

////////////////////////
//     PUBLIC API     //
////////////////////////

MooseUpdateData * moose_update_data_new(struct MooseClient * self)
{
    g_assert(self);

    MooseUpdateData * data = g_slice_new0(MooseUpdateData);
    data->client = self;
    data->event_queue = g_async_queue_new();

    g_mutex_init(&data->status_timer.mutex);

    data->last_song_data.id = -1;
    data->last_song_data.state = MOOSE_STATE_UNKNOWN;

    data->update_thread = g_thread_new(
        "data-update-thread",
        moose_update_thread,
        data
        );

    return data;
}

////////////////////////

void moose_update_data_destroy(MooseUpdateData * data)
{
    g_assert(data);

    /* Push the termination sign to the Queue */
    g_async_queue_push(data->event_queue, GINT_TO_POINTER(THREAD_TERMINATOR));

    /* Wait for the thread to finish */
    g_thread_join(data->update_thread);

    moose_update_reset(data);

    /* Destroy the Queue */
    g_async_queue_unref(data->event_queue);

    data->event_queue = NULL;
    data->update_thread = NULL;

    g_mutex_clear(&data->status_timer.mutex);

    g_slice_free(MooseUpdateData, data);
}

////////////////////////

void moose_update_data_push(MooseUpdateData * data, enum mpd_idle event)
{
    moose_update_data_push_full(data, event, false);
}

////////////////////////

void moose_update_reset(MooseUpdateData * data)
{
    if (data->status != NULL) {
        moose_status_unref(data->status);
    }
    data->status = NULL;
}
