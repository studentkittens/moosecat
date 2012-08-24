#include "update.h"
#include "protocol.h"

////////////////////////

const enum mpd_idle on_status_update = (MPD_IDLE_PLAYER | MPD_IDLE_OPTIONS | MPD_IDLE_MIXER | MPD_IDLE_OUTPUT | MPD_IDLE_QUEUE);
const enum mpd_idle on_stats_update  = (MPD_IDLE_UPDATE | MPD_IDLE_DATABASE);
const enum mpd_idle on_song_update   = (MPD_IDLE_PLAYER);

////////////////////////

#define UPDATE_FIELD(on_event, var, free_func, update_func)               \
    if(events & on_event)                                                 \
    {                                                                     \
        if(var != NULL)                                                   \
           free_func (var);                                               \
                                                                          \
        var = update_func (conn);                                         \
        if (!var && mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS)  \
            g_print("Unable to send status/song/stats: %s\n",             \
               mpd_connection_get_error_message(conn));                   \
    }                                                                     \

////////////////////////

void mc_proto_update_context_info_cb (enum mpd_idle events, void * user_data)
{
    mc_Client * self = user_data;
    if (self != NULL)
    {
        mpd_connection * conn = mc_proto_get (self);
        if (conn != NULL)
        {
            UPDATE_FIELD (on_status_update, self->status, mpd_status_free, mpd_run_status);
            UPDATE_FIELD (on_stats_update, self->stats, mpd_stats_free, mpd_run_stats);
            UPDATE_FIELD (on_song_update, self->song, mpd_song_free, mpd_run_current_song);

            mc_proto_put (self);
        }
    }
}

////////////////////////
