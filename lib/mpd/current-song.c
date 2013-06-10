#include "current-song.h"
#include "update.h"

#define SONG_FUNC(NAME, RTYPE)                           \
    RTYPE mc_current_song_get_ ## NAME (mc_Client *self) \
    {                                                    \
        RTYPE rc = 0;                                    \
        if(self && self->song) {                         \
            mc_lock_current_song(self);                  \
            if(self->song) {                             \
                rc = mpd_song_get_ ## NAME (self->song); \
            }                                            \
            mc_unlock_current_song(self);                \
        }                                                \
        return rc;                                       \
    }                                                    \


SONG_FUNC(uri, const char *)
SONG_FUNC(duration, unsigned)
SONG_FUNC(start, unsigned)
SONG_FUNC(end, unsigned)
SONG_FUNC(last_modified, time_t)
SONG_FUNC(pos, unsigned)
SONG_FUNC(id, unsigned)



const char * mc_current_song_get_tag(
        mc_Client *self,
        enum mpd_tag_type type,
        unsigned tag_pos)
{
    const char * rc = NULL;

    if(self && self->song) {                       
        mc_lock_current_song(self);
        if(self->song) {
            rc = mpd_song_get_tag(self->song, type, tag_pos);
        }
        mc_unlock_current_song(self);
    }

    return rc;
}
