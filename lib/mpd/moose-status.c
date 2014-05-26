#include <stdbool.h>
#include <string.h>

#include "moose-status.h"

typedef struct _MooseStatusPrivate {
    /** Locked if the Status is "in use", i.e. attributes are being set */
    GRWLock ref_lock;

    /** 0-100, or MOOSE_STATUS_NO_VOLUME when there is no volume support */
    int volume;

    /** Queue repeat mode enabled? */
    bool repeat;

    /** Random mode enabled? */
    bool random;

    /** Single song mode enabled? */
    bool single;

    /** Song consume mode enabled? */
    bool consume;

    /** Number of songs in the queue */
    unsigned queue_length;

    /** Queue version, use this to determine when the playlist has changed. */
    unsigned queue_version;

    /** MPD's current playback state */
    MooseState state;

    /** crossfade setting in seconds */
    unsigned crossfade;

    /** Mixramp threshold in dB */
    float mixrampdb;

    /** Mixramp extra delay in seconds */
    float mixrampdelay;

    /**
     * If a song is currently selected (always the case when state
     * is PLAY or PAUSE), this is the position of the currently
     * playing song in the queue, beginning with 0.
     */
    int song_pos;

    /** Song ID of the currently selected song */
    int song_id;

    /** The same as song_pos, but for the next song to be played */
    int next_song_pos;

    /** Song ID of the next song to be played */
    int next_song_id;

    /**
     * Time in seconds that have elapsed in the currently
     * playing/paused song.
     */
    unsigned elapsed_time;

    /**
     * Time in milliseconds that have elapsed in the currently
     * playing/paused song.
     */
    unsigned elapsed_ms;

    /** length in seconds of the currently playing/paused song */
    unsigned total_time;

    /** current bit rate in kbps */
    unsigned kbit_rate;

    struct {
        /**
         * The sample rate in Hz.  A better name for this attribute is
         * "frame rate", because technically, you have two samples per
         * frame in stereo sound.
         */
        uint32_t sample_rate;

        /**
         * The number of significant bits per sample.  Samples are
         * currently always signed.  Supported values are 8, 16, 24,
         * 32.  24 bit samples are packed in 32 bit integers.
         */
        uint8_t bits;

        /**
         * The number of channels.  Only mono (1) and stereo (2) are
         * fully supported currently.
         */
        uint8_t channels;
    } audio;

    /** non-zero if MPD is updating, 0 otherwise */
    unsigned update_id;

    /** error message */
    const char * last_error;

    const MooseSong * current_song;

    struct {
        unsigned number_of_artists;
        unsigned number_of_albums;
        unsigned number_of_songs;
        unsigned long uptime;
        unsigned long db_update_time;
        unsigned long play_time;
        unsigned long db_play_time;
    } stats;


    char replay_gain_mode[64];

    GHashTable * outputs;

} MooseStatusPrivate;


G_DEFINE_TYPE_WITH_PRIVATE(MooseStatus, moose_status, G_TYPE_OBJECT);


#define READ(self, return_name, return_type, default_val) { \
        g_return_val_if_fail(self, default_val);                \
        return_type rval;                                       \
        g_rw_lock_reader_lock(&self->priv->ref_lock); {         \
            rval = self->priv->return_name;                     \
        }                                                       \
        g_rw_lock_reader_unlock(&self->priv->ref_lock);         \
        return rval;                                            \
}                                                           \
 

///////////////////////////////

static void moose_status_finalize(GObject * gobject) {
    MooseStatus * self = MOOSE_STATUS(gobject);

    if (self == NULL) {
        return;
    }

    if (self->priv->current_song != NULL) {
        g_object_unref(MOOSE_SONG(self->priv->current_song));
    }

    g_rw_lock_clear(&self->priv->ref_lock);
    g_hash_table_destroy(self->priv->outputs);
    self->priv->outputs = NULL;

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

///////////////////////////////

static void moose_status_class_init(MooseStatusClass * klass) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_status_finalize;
}

///////////////////////////////

static void moose_status_init(MooseStatus * self) {
    self->priv = moose_status_get_instance_private(self);
    self->priv->state = MOOSE_STATE_UNKNOWN;
    self->priv->volume = -1;
    self->priv->mixrampdb = 0.0;
    self->priv->mixrampdelay = 0.0;
    self->priv->song_pos = -1;
    self->priv->song_id = -1;
    self->priv->next_song_pos = -1;
    self->priv->next_song_id = -1;

    g_rw_lock_init(&self->priv->ref_lock);
    self->priv->outputs = g_hash_table_new_full(
                              g_str_hash,
                              g_direct_equal,
                              NULL,
                              (GDestroyNotify)g_variant_unref
                          );
}

///////////////////////////////
//          PUBLIC           //
///////////////////////////////

MooseStatus * moose_status_new(void) {
    return g_object_new(MOOSE_TYPE_STATUS, NULL);
}

void moose_status_unref(MooseStatus * self) {
    if (self != NULL) {
        g_object_unref(self);
    }
}

///////////////////////////////

int moose_status_get_volume(const MooseStatus * self) {
    READ(self, volume, int, -1)
}

bool moose_status_get_repeat(const MooseStatus * self) {
    READ(self, repeat, bool, false)
}

bool moose_status_get_random(const MooseStatus * self) {
    READ(self, random, bool, false)
}

bool moose_status_get_single(const MooseStatus * self) {
    READ(self, single, bool, false)
}

bool moose_status_get_consume(const MooseStatus * self) {
    READ(self, consume, bool, false)
}

unsigned moose_status_get_queue_length(const MooseStatus * self) {
    READ(self, queue_length, unsigned, 0)
}

unsigned moose_status_get_queue_version(const MooseStatus * self) {
    READ(self, queue_version, unsigned, 0)
}

MooseState moose_status_get_state(const MooseStatus * self) {
    READ(self, state, MooseState, MOOSE_STATE_UNKNOWN)
}

unsigned moose_status_get_crossfade(const MooseStatus * self) {
    READ(self, crossfade, unsigned, 0)
}

float moose_status_get_mixrampdb(const MooseStatus * self) {
    READ(self, mixrampdb, float, 0)
}

float moose_status_get_mixrampdelay(const MooseStatus * self) {
    READ(self, mixrampdelay, float, 0)
}

int moose_status_get_song_pos(const MooseStatus * self) {
    READ(self, song_pos, int, -1)
}

int moose_status_get_song_id(const MooseStatus * self) {
    READ(self, song_id, int, -1)
}

int moose_status_get_next_song_pos(const MooseStatus * self) {
    READ(self, next_song_pos, int, -1)
}

int moose_status_get_next_song_id(const MooseStatus * self) {
    READ(self, next_song_id, int, -1)
}

unsigned moose_status_get_elapsed_time(const MooseStatus * self) {
    READ(self, elapsed_time, unsigned, 0)
}

unsigned moose_status_get_elapsed_ms(const MooseStatus * self) {
    READ(self, elapsed_ms, unsigned, 0)
}

unsigned moose_status_get_total_time(const MooseStatus * self) {
    READ(self, total_time, unsigned, 0)
}

unsigned moose_status_get_kbit_rate(const MooseStatus * self) {
    READ(self, kbit_rate, unsigned, 0)
}

unsigned moose_status_get_update_id(const MooseStatus * self) {
    READ(self, update_id, unsigned, 0)
}

const char * moose_status_get_last_error(const MooseStatus * self) {
    READ(self, last_error, const char *, NULL)
}

/////////////////////////

uint32_t moose_status_get_audio_sample_rate(const MooseStatus * self) {
    READ(self, audio.sample_rate, uint32_t, 0)
}

uint8_t moose_status_get_audio_bits(const MooseStatus * self) {
    READ(self, audio.bits, uint8_t, 0)
}

uint8_t moose_status_get_audio_channels(const MooseStatus * self) {
    READ(self, audio.channels, uint8_t, 0)
}

/////////////////////////

void moose_status_convert(MooseStatus * self, const struct mpd_status * status) {
    g_assert(self);
    g_assert(status);

    g_rw_lock_writer_lock(&self->priv->ref_lock);
    {
        self->priv->volume = mpd_status_get_volume(status);
        self->priv->repeat = mpd_status_get_repeat(status);
        self->priv->random = mpd_status_get_random(status);
        self->priv->single = mpd_status_get_single(status);
        self->priv->consume = mpd_status_get_consume(status);
        self->priv->queue_length = mpd_status_get_queue_length(status);
        self->priv->queue_version = mpd_status_get_queue_version(status);
        self->priv->state = (MooseState)mpd_status_get_state(status);
        self->priv->crossfade = mpd_status_get_crossfade(status);
        self->priv->mixrampdb = mpd_status_get_mixrampdb(status);
        self->priv->mixrampdelay = mpd_status_get_mixrampdelay(status);
        self->priv->song_pos = mpd_status_get_song_pos(status);
        self->priv->song_id = mpd_status_get_song_id(status);
        self->priv->next_song_pos = mpd_status_get_next_song_pos(status);
        self->priv->next_song_id = mpd_status_get_next_song_id(status);
        self->priv->elapsed_time = mpd_status_get_elapsed_time(status);
        self->priv->elapsed_ms = mpd_status_get_elapsed_ms(status);
        self->priv->total_time = mpd_status_get_total_time(status);
        self->priv->kbit_rate = mpd_status_get_kbit_rate(status);
        self->priv->update_id = mpd_status_get_update_id(status);
        self->priv->last_error = mpd_status_get_error(status);

        const struct mpd_audio_format * audio = mpd_status_get_audio_format(status);
        if (audio != NULL) {
            self->priv->audio.sample_rate = audio->sample_rate;
            self->priv->audio.channels = audio->channels;
            self->priv->audio.bits = audio->bits;
        }
    }
    g_rw_lock_writer_unlock(&self->priv->ref_lock);
}

/////////////////////////

MooseStatus * moose_status_new_from_struct(const struct mpd_status * status) {
    g_return_val_if_fail(status, NULL);

    MooseStatus * self = moose_status_new();
    moose_status_convert(self, status);
    return self;
}

/////////////////////////

MooseSong * moose_status_get_current_song(const MooseStatus * self) {
    g_return_val_if_fail(self, NULL);
    if (self->priv->current_song) {
        g_object_ref(MOOSE_SONG(self->priv->current_song));
    }
    return (MooseSong *)self->priv->current_song;
}

/////////////////////////

void moose_status_set_current_song(MooseStatus * self, const MooseSong * song) {
    g_rw_lock_writer_lock(&self->priv->ref_lock);
    {
        if (self->priv->current_song) {
            g_object_unref(MOOSE_SONG(song));
            self->priv->current_song = NULL;
        }

        if (self != NULL) {
            if (song != NULL) {
                g_object_ref(MOOSE_SONG(song));
            }
            self->priv->current_song = song;
        }
    }
    g_rw_lock_writer_unlock(&self->priv->ref_lock);
}

////////////// STATS /////////////////

unsigned moose_status_stats_get_number_of_artists(const MooseStatus * self) {
    READ(self, stats.number_of_artists, unsigned, 0)
}

unsigned moose_status_stats_get_number_of_albums(const MooseStatus * self) {
    READ(self, stats.number_of_albums, unsigned, 0)
}

unsigned moose_status_stats_get_number_of_songs(const MooseStatus * self) {
    READ(self, stats.number_of_songs, unsigned, 0)
}

unsigned long moose_status_stats_get_uptime(const MooseStatus * self) {
    READ(self, stats.uptime, unsigned long, 0)
}

unsigned long moose_status_stats_get_db_update_time(const MooseStatus * self) {
    READ(self, stats.db_update_time, unsigned long, 0)
}

unsigned long moose_status_stats_get_play_time(const MooseStatus * self) {
    READ(self, stats.play_time, unsigned long, 0)
}

unsigned long moose_status_stats_get_db_play_time(const MooseStatus * self) {
    READ(self, stats.db_play_time, unsigned long, 0)
}

/////////////////////////

void moose_status_update_stats(const MooseStatus * self, const struct mpd_stats * stats) {
    g_assert(self);
    g_assert(stats);

    g_rw_lock_writer_lock(&self->priv->ref_lock);
    {
        self->priv->stats.number_of_artists  = mpd_stats_get_number_of_artists(stats);
        self->priv->stats.number_of_albums = mpd_stats_get_number_of_albums(stats);
        self->priv->stats.number_of_songs = mpd_stats_get_number_of_songs(stats);
        self->priv->stats.uptime = mpd_stats_get_uptime(stats);
        self->priv->stats.db_update_time = mpd_stats_get_db_update_time(stats);
        self->priv->stats.play_time = mpd_stats_get_play_time(stats);
        self->priv->stats.db_play_time = mpd_stats_get_db_play_time(stats);
    }
    g_rw_lock_writer_unlock(&self->priv->ref_lock);
}

///////////////// REPLAY GAIN /////////////////////

const char * moose_status_get_replay_gain_mode(const MooseStatus * self) {
    READ(self, replay_gain_mode, const char *, "off")
}

void moose_status_set_replay_gain_mode(const MooseStatus * self, const char * mode) {
    g_return_if_fail(self);

    if (mode != NULL) {
        g_rw_lock_writer_lock(&self->priv->ref_lock);
        {
            strncpy(
                self->priv->replay_gain_mode,
                mode,
                sizeof(self->priv->replay_gain_mode) - 1
            );
        }
        g_rw_lock_writer_unlock(&self->priv->ref_lock);
    }
}

//////////////////// OUTPUTS //////////////////////

void moose_status_outputs_clear(const MooseStatus * self) {
    g_assert(self);

    g_rw_lock_writer_lock(&self->priv->ref_lock);
    {
        GHashTable * outputs = self->priv->outputs;
        if (outputs != NULL) {
            self->priv->outputs = NULL;
            g_hash_table_unref(outputs);
            self->priv->outputs = g_hash_table_new_full(
                                      g_str_hash,
                                      g_direct_equal,
                                      NULL,
                                      (GDestroyNotify)g_variant_unref
                                  );
        }
    }
    g_rw_lock_writer_unlock(&self->priv->ref_lock);
}

void moose_status_outputs_add(const MooseStatus * self, struct mpd_output * output) {
    g_assert(self);
    g_assert(output);

    g_rw_lock_writer_lock(&self->priv->ref_lock);
    {
        g_hash_table_insert(
            self->priv->outputs,
            (gpointer)mpd_output_get_name(output),
            g_variant_new(
                "(sib)",
                mpd_output_get_name(output),
                mpd_output_get_id(output),
                mpd_output_get_enabled(output)
            )
        );
    }
    g_rw_lock_writer_unlock(&self->priv->ref_lock);
}

GHashTable * moose_status_outputs_get(const MooseStatus * self) {
    g_return_val_if_fail(self, NULL);

    GHashTable * ref = NULL;
    g_rw_lock_reader_lock(&self->priv->ref_lock);
    {
        ref = g_hash_table_ref(self->priv->outputs);
    }
    g_rw_lock_reader_unlock(&self->priv->ref_lock);
    return ref;
}
