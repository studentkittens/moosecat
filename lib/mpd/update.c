#include "update.h"
#include "protocol.h"
#include "signal_helper.h"

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

void mc_proto_update_context_info_cb (enum mpd_idle events, void * user_data)
{
    mc_Client * self = user_data;
    if (self != NULL && events != 0)
    {
        mpd_connection * conn = mc_proto_get (self);
        if (conn != NULL)
        {
            bool update_status = (events & on_status_update);
            bool update_stats = (events & on_stats_update);
            bool update_song = (events & on_song_update);

            /* Send a block of commands, speeds the thing up by 2x */
            mpd_command_list_begin (conn, true);
            {
                /* Note: order of recv == order of send. */
                if (update_status)
                    mpd_send_status (conn);

                if (update_song)
                    mpd_send_current_song (conn);

                if (update_stats)
                    mpd_send_stats (conn);
            }
            mpd_command_list_end (conn);
            mc_shelper_report_error (self, conn);

            /* Try to receive status */
            if (update_status)
            {
                free_if_not_null (self->status, mpd_status_free);
                self->status = mpd_recv_status (conn);
                mpd_response_next (conn);
                mc_shelper_report_error (self, conn);
            }

            /* Try to receive the current song */
            if (update_song)
            {
                free_if_not_null (self->song, mpd_song_free);
                self->song = mpd_recv_song (conn);

                /* We need to call recv() one more time
                 * so we end the songlist,
                 * it should only return  NULL
                 * */
                struct mpd_song * empty = mpd_recv_song (conn);
                g_assert (empty == NULL);
                mpd_response_next (conn);
                mc_shelper_report_error (self, conn);
            }

            /* Try to receive statistics as last */
            if (update_stats)
            {
                /* Always last, no next() needed */
                free_if_not_null (self->stats, mpd_stats_free);
                self->stats = mpd_recv_stats (conn);
                mc_shelper_report_error (self, conn);
            }

            /* Finish repsonse */
            if (update_song || update_stats || update_status)
            {
                mpd_response_finish (conn);
                mc_shelper_report_error (self, conn);
            }

            mc_proto_put (self);
        }
    }
}

////////////////////////
