#include "update.h"
#include "protocol.h"
#include "outputs.h"
#include "signal-helper.h"
#include "../compiler.h"

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

#define free_if_not_null(var, func) if(var != NULL) func((void*)var)
#define THREAD_TERMINATOR 0xDEADBEEF

////////////////////////

static void mc_proto_update_context_info_cb( struct mc_Client *self, enum mpd_idle events)
{
    if (self != NULL && events != 0 && mc_proto_is_connected(self)) {
        struct mpd_connection *conn = mc_proto_get(self);

        if (conn != NULL) {
            const bool update_status = (events & on_status_update);
            const bool update_stats = (events & on_stats_update);
            const bool update_song = (events & on_song_update);
            const bool update_rg = (events & on_rg_status_update);

            if (update_status || update_stats || update_song || update_rg) {
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
                mc_shelper_report_error(self, conn);

                /* Try to receive status */
                if (update_status) {
                    struct mpd_status *tmp_status;
                    tmp_status = mpd_recv_status(conn);

                    if (self->status_timer.last_update != NULL && self->status_timer.reset_timer) {
                        /* Reset the status timer to 0 */
                        g_timer_start(self->status_timer.last_update);
                    }

                    /* Be error tolerant, and keep at least the last status */
                    if (tmp_status) {
                        mc_update_data_open(self);
                        mc_lock_status(self);
                        free_if_not_null(self->status, mpd_status_free);
                        self->status = tmp_status;
                        mc_unlock_status(self);
                        mc_update_data_close(self);
                    }

                    mpd_response_next(conn);
                    mc_shelper_report_error(self, conn);
                }

                /* Try to receive statistics as last */
                if (update_stats) {
                    struct mpd_stats *tmp_stats;
                    tmp_stats = mpd_recv_stats(conn);

                    if (tmp_stats) {
                        mc_update_data_open(self);
                        mc_lock_stats(self);
                        free_if_not_null(self->stats, mpd_stats_free);
                        self->stats = tmp_stats;
                        mc_unlock_stats(self);
                        mc_update_data_close(self);
                    }

                    mpd_response_next(conn);
                    mc_shelper_report_error(self, conn);
                }

                /* Read the current replay gain status */
                if (update_rg) {
                    free_if_not_null(self->replay_gain_status, g_free);
                    struct mpd_pair *mode = mpd_recv_pair_named(conn, "replay_gain_mode");

                    if (mode != NULL) {
                        mc_update_data_open(self);
                        mc_lock_status(self);
                        self->replay_gain_status = g_strdup(mode->value);
                        mc_unlock_status(self);
                        mc_update_data_close(self);

                        mpd_return_pair(conn, mode);
                    }

                    mpd_response_next(conn);
                    mc_shelper_report_error(self, conn);
                }

                /* Try to receive the current song */
                if (update_song) {
                    struct mpd_song * new_song = mpd_recv_song(conn);

                    mc_update_data_open(self);
                    mc_lock_current_song(self);
                    free_if_not_null(self->song, mpd_song_free);
                    self->song = new_song;

                    /* We need to call recv() one more time
                     * so we end the songlist,
                     * it should only return  NULL
                     * */
                    if (self->song != NULL) {
                        struct mpd_song *empty = mpd_recv_song(conn);
                        g_assert(empty == NULL);
                    }

                    mc_unlock_current_song(self);
                    mc_update_data_close(self);
                    mc_shelper_report_error(self, conn);
                }

                /* Finish repsonse */
                if (update_song || update_stats || update_status || update_rg) {
                    mpd_response_finish(conn);
                    mc_shelper_report_error(self, conn);
                }
            }

            mc_proto_put(self);
        }
    }
}

////////////////////////

gpointer mc_proto_update_thread(gpointer user_data)
{
    g_assert(user_data);

    mc_UpdateData *data = user_data;
    
    enum mpd_idle event_mask = 0;

    while((event_mask = GPOINTER_TO_INT(g_async_queue_pop(data->event_queue))) != THREAD_TERMINATOR) {
        mc_proto_update_context_info_cb(data->client, event_mask);
        mc_proto_outputs_update(data->client->_outputs, event_mask);
    }

    return NULL;
}

////////////////////////

static gboolean mc_proto_update_status_timer_cb(gpointer data)
{
    g_assert(data);
    struct mc_Client *self = data;

    if (mc_proto_is_connected(self) == false) {
        return false;
    }

    if (mpd_status_get_state(self->status) == MPD_STATE_PLAY) {

        /* TODO: Is this still valid? */
        /* Substract a small amount to include the network latency - a bit of a hack
         * but worst thing that could happen: you miss one status update.
         * */
        float compare = MAX(self->status_timer.interval - self->status_timer.interval / 10, 0);
        float elapsed = g_timer_elapsed(self->status_timer.last_update, NULL) * 1000;

        if (elapsed >= compare) {
            /* MIXER is a harmless event, but it causes status to update */
            enum mpd_idle on_status_only = MPD_IDLE_MIXER;

            self->status_timer.reset_timer = false;
            mc_proto_update_data_push(self->_update_data, on_status_only);
            self->status_timer.reset_timer = true;

            if (self->status_timer.trigger_event) {
                mc_shelper_report_client_event(self, on_status_only);
            }
        }
    }

    return mc_proto_update_status_timer_is_active(self);
}

////////////////////////

void mc_proto_update_register_status_timer(
    struct mc_Client *self,
    int repeat_ms,
    bool trigger_event)
{
    g_assert(self);

    self->status_timer.trigger_event = trigger_event;
    self->status_timer.last_update = g_timer_new();
    self->status_timer.reset_timer = true;
    self->status_timer.interval = repeat_ms;
    self->status_timer.timeout_id =
        g_timeout_add(repeat_ms, mc_proto_update_status_timer_cb, self);
}

////////////////////////

void mc_proto_update_unregister_status_timer(
    struct mc_Client *self)
{
    g_assert(self);

    if (mc_proto_update_status_timer_is_active(self)) {
        if (self->status_timer.timeout_id > 0) {
            g_source_remove(self->status_timer.timeout_id);
        }

        self->status_timer.timeout_id = -1;
        self->status_timer.interval = 0;
        self->status_timer.reset_timer = true;

        if (self->status_timer.last_update != NULL) {
            g_timer_destroy(self->status_timer.last_update);
        }
    }
}

////////////////////////

bool mc_proto_update_status_timer_is_active(struct mc_Client *self)
{
    g_assert(self);
    return (self->status_timer.timeout_id != -1);
}

////////////////////////

void mc_lock_status(struct mc_Client *self)
{
    g_mutex_lock(&self->update_mtx.status);
}

////////////////////////

void mc_unlock_status(struct mc_Client *self)
{
    g_mutex_unlock(&self->update_mtx.status);
}

////////////////////////

void mc_lock_stats(struct mc_Client *self)
{
    g_mutex_lock(&self->update_mtx.stats);
}

////////////////////////

void mc_unlock_stats(struct mc_Client *self)
{
    g_mutex_unlock(&self->update_mtx.stats);
}

////////////////////////

void mc_lock_current_song(struct mc_Client *self)
{
    g_mutex_lock(&self->update_mtx.song);
}

////////////////////////

void mc_unlock_current_song(struct mc_Client *self)
{
    g_mutex_unlock(&self->update_mtx.song);
}

////////////////////////
////////////////////////
////////////////////////

mc_UpdateData * mc_proto_update_data_new(struct mc_Client * self)
{
    g_assert(self);

    g_printerr("Creating Update Data\n");

    mc_UpdateData *data = g_slice_new0(mc_UpdateData);
    data->client = self;
    data->event_queue = g_async_queue_new();

    g_rec_mutex_init(&data->data_set_mutex);

    data->update_thread = g_thread_new(
            "StatusUpdateThread",
            mc_proto_update_thread,
            data
    );
    return data;
}

////////////////////////

void mc_proto_update_data_destroy(mc_UpdateData * data)
{
    g_assert(data);

    /* Push the termination sign to the Queue */
    g_async_queue_push(data->event_queue, GINT_TO_POINTER(THREAD_TERMINATOR));

    /* Wait for the thread to finish */
    g_thread_join(data->update_thread);

    /* Destroy the Queue */
    g_async_queue_unref(data->event_queue);

    data->event_queue = NULL;
    data->update_thread = NULL;
    g_rec_mutex_clear(&data->data_set_mutex);

    g_slice_free(mc_UpdateData, data);
}

////////////////////////

void mc_proto_update_data_push(mc_UpdateData *data, enum mpd_idle event)
{
    g_assert(data);

    /* We may not push 0 - that would cause the event_queue to shutdown */
    if(event != 0) {
        g_async_queue_push(data->event_queue, GINT_TO_POINTER(event));
    }
}

////////////////////////

void mc_update_data_open(struct mc_Client *self)
{
    g_assert(self);
    g_rec_mutex_lock(&self->_update_data->data_set_mutex);
}

////////////////////////

void mc_update_data_close(struct mc_Client *self)
{
    g_assert(self);
    g_rec_mutex_unlock(&self->_update_data->data_set_mutex);
}
