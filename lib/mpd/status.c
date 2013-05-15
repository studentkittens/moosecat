#include "status.h"
#include "update.h"


#define STATUS_FUNC(NAME, RTYPE)                       \
    RTYPE mc_status_get_  ## NAME (mc_Client *self)    \
    {                                                  \
        RTYPE rc = 0;                                  \
        mc_lock_status(self);                          \
        rc = mpd_status_get_ ## NAME (self->status);   \
        mc_unlock_status(self);                        \
        return rc;                                     \
    }                                                  \

STATUS_FUNC(volume, int)
STATUS_FUNC(repeat, bool)
STATUS_FUNC(random, bool)
STATUS_FUNC(single, bool)
STATUS_FUNC(consume, bool)
STATUS_FUNC(queue_length, unsigned)
STATUS_FUNC(queue_version, unsigned)
STATUS_FUNC(state, enum mpd_state)
STATUS_FUNC(crossfade, unsigned)
STATUS_FUNC(mixrampdb, float)
STATUS_FUNC(mixrampdelay, float)
STATUS_FUNC(song_pos, int)
STATUS_FUNC(song_id, int)
STATUS_FUNC(next_song_pos, int)
STATUS_FUNC(next_song_id, int)
STATUS_FUNC(elapsed_time, unsigned)
STATUS_FUNC(elapsed_ms, unsigned)
STATUS_FUNC(total_time, unsigned)
STATUS_FUNC(kbit_rate, unsigned)


#define STATUS_AUDIO_FUNC(NAME)                                        \
    unsigned mc_status_get_status_audio_get_ ## NAME (mc_Client *self) \
    {                                                                  \
        unsigned rc = 0;                                               \
        mc_lock_status(self);                                          \
        const struct mpd_audio_format * audio;                         \
        audio = mpd_status_get_audio_format(self->status);             \
        if(audio != NULL) {                                            \
            rc = audio-> NAME;                                         \
        }                                                              \
        mc_unlock_status(self);                                        \
        return rc;                                                     \
    }                                                                  \

STATUS_AUDIO_FUNC(sample_rate);
STATUS_AUDIO_FUNC(bits);
STATUS_AUDIO_FUNC(channels);


const char *mc_status_get_replay_gain_status(mc_Client *self)
{
    const char * rc = NULL;
    mc_lock_status(self);
    rc = g_strdup(self->replay_gain_status);
    mc_unlock_status(self);
    return rc;
}
