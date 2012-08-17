#include "update.h"
#include "protocol.h"

////////////////////////

const enum mpd_idle on_status_update = (MPD_IDLE_PLAYER | MPD_IDLE_OPTIONS | MPD_IDLE_MIXER | MPD_IDLE_OUTPUT | MPD_IDLE_QUEUE);
const enum mpd_idle on_stats_update  = (MPD_IDLE_UPDATE | MPD_IDLE_DATABASE);
const enum mpd_idle on_song_update   = (MPD_IDLE_PLAYER);

////////////////////////

#define free_if_not_null(var, func) if(var != NULL) func(var)

////////////////////////

void proto_update_callback (enum mpd_idle events, void * user_data)
{
    Proto_Connector * self = user_data;
    if (self != NULL)
    {
        mpd_connection * conn = proto_get (self);
        if (conn != NULL)
        {
            free_if_not_null (self->status, mpd_status_free);
            if (events & on_status_update)
                self->status = mpd_run_status (conn);

            free_if_not_null (self->stats, mpd_stats_free);
            if (events & on_stats_update)
                self->stats = mpd_run_stats (conn);

            free_if_not_null (self->song, mpd_song_free);
            if (events & on_song_update)
                self->song = mpd_run_current_song (conn);

            proto_put (self);
        }
    }
}

////////////////////////
