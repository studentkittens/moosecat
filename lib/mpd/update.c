#include "update.h"
#include "protocol.h"
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

const enum mpd_idle on_song_update = (0
                                      | MPD_IDLE_PLAYER);

////////////////////////

#define free_if_not_null(var, func) if(var != NULL) func(var)

////////////////////////

void mc_proto_update_context_info_cb (
    struct mc_Client * self,
    enum mpd_idle events,
    mc_cc_unused void * user_data)
{
    if (self != NULL && events != 0 && mc_proto_is_connected (self) ) {
        struct mpd_connection * conn = mc_proto_get (self);
        if (conn != NULL) {
            const bool update_status = (events & on_status_update);
            const bool update_stats = (events & on_stats_update);
            const bool update_song = (events & on_song_update);

            if (update_status || update_stats || update_song) {

                /* Send a block of commands, speeds the thing up by 2x */
                mpd_command_list_begin (conn, true);
                {
                    /* Note: order of recv == order of send. */
                    if (update_status)
                        mpd_send_status (conn);

                    if (update_stats)
                        mpd_send_stats (conn);

                    if (update_song)
                        mpd_send_current_song (conn);
                }
                mpd_command_list_end (conn);
                mc_shelper_report_error (self, conn);

                /* Try to receive status */
                if (update_status) {
                    struct mpd_status * tmp_status;
                    tmp_status = mpd_recv_status (conn);

                    /* Be error tolerant, and keep at least the last status */
                    if (tmp_status) {
                        free_if_not_null (self->status, mpd_status_free);
                        self->status = tmp_status;
                    }

                    mpd_response_next (conn);
                    mc_shelper_report_error (self, conn);
                }

                /* Try to receive statistics as last */
                if (update_stats) {
                    struct mpd_stats * tmp_stats;
                    tmp_stats = mpd_recv_stats (conn);

                    if (tmp_stats) {
                        free_if_not_null (self->stats, mpd_stats_free);
                        self->stats = tmp_stats;
                    }

                    mpd_response_next (conn);
                    mc_shelper_report_error (self, conn);
                }

                /* Try to receive the current song */
                if (update_song) {
                    free_if_not_null (self->song, mpd_song_free);
                    self->song = mpd_recv_song (conn);

                    /* We need to call recv() one more time
                     * so we end the songlist,
                     * it should only return  NULL
                     * */
                    if (self->song != NULL) {
                        struct mpd_song * empty = mpd_recv_song (conn);
                        g_assert (empty == NULL);
                    }

                    mc_shelper_report_error (self, conn);
                }

                /* Finish repsonse */
                if (update_song || update_stats || update_status) {
                    mpd_response_finish (conn);
                    mc_shelper_report_error (self, conn);
                }
            }

            mc_proto_put (self);
        }
    }
}

////////////////////////
