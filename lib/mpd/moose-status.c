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
    const char *last_error;

    const MooseSong *current_song;

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

    GHashTable *outputs;

} MooseStatusPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(MooseStatus, moose_status, G_TYPE_OBJECT);

#define READ(self, return_name, return_type, default_val) \
    {                                                     \
        g_return_val_if_fail(self, default_val);          \
        return_type rval;                                 \
        g_rw_lock_reader_lock(&self->priv->ref_lock);     \
        { rval = self->priv->return_name; }               \
        g_rw_lock_reader_unlock(&self->priv->ref_lock);   \
        return rval;                                      \
    }

/* This feels a bit like doing taxes, but well. */
enum {
    PROP_VOLUME = 1,
    PROP_REPEAT,
    PROP_RANDOM,
    PROP_SINGLE,
    PROP_CONSUME,
    PROP_QUEUE_LENGTH,
    PROP_QUEUE_VERSION,
    PROP_STATE,
    PROP_CROSSFADE,
    PROP_MIXRAMPDB,
    PROP_MIXRAMPDELAY,
    PROP_SONG_POS,
    PROP_SONG_ID,
    PROP_NEXT_SONG_POS,
    PROP_NEXT_SONG_ID,
    PROP_ELAPSED_TIME,
    PROP_ELAPSED_MS,
    PROP_TOTAL_TIME,
    PROP_KBIT_RATE,
    PROP_UPDATE_ID,
    PROP_AUDIO_SAMPLE_RATE,
    PROP_AUDIO_BITS,
    PROP_AUDIO_CHANNELS,
    PROP_NUMBER_OF_ARTISTS,
    PROP_NUMBER_OF_ALBUMS,
    PROP_NUMBER_OF_SONGS,
    PROP_UP_TIME,
    PROP_DB_UPDATE_TIME,
    PROP_PLAY_TIME,
    PROP_REPLAY_GAIN_MODE,
    PROP_LAST_ERROR,
    N_PROPS
};

/* This is implemented for easier introspection support */
static void moose_status_get_property(GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec) {
    MooseStatus *self = MOOSE_STATUS(object);
    MooseStatusPrivate *priv = self->priv;
    g_rw_lock_reader_lock(&priv->ref_lock);
    {
        switch(property_id) {
        case PROP_VOLUME:
            g_value_set_int(value, priv->volume);
            break;
        case PROP_REPEAT:
            g_value_set_boolean(value, priv->repeat);
            break;
        case PROP_RANDOM:
            g_value_set_boolean(value, priv->random);
            break;
        case PROP_SINGLE:
            g_value_set_boolean(value, priv->single);
            break;
        case PROP_CONSUME:
            g_value_set_boolean(value, priv->consume);
            break;
        case PROP_QUEUE_LENGTH:
            g_value_set_int(value, priv->queue_length);
            break;
        case PROP_QUEUE_VERSION:
            g_value_set_int(value, priv->queue_version);
            break;
        case PROP_STATE:
            g_value_set_enum(value, priv->state);
            break;
        case PROP_CROSSFADE:
            g_value_set_int(value, priv->crossfade);
            break;
        case PROP_MIXRAMPDB:
            g_value_set_float(value, priv->mixrampdb);
            break;
        case PROP_MIXRAMPDELAY:
            g_value_set_float(value, priv->mixrampdelay);
            break;
        case PROP_SONG_POS:
            g_value_set_int(value, priv->song_pos);
            break;
        case PROP_SONG_ID:
            g_value_set_int(value, priv->song_id);
            break;
        case PROP_NEXT_SONG_POS:
            g_value_set_int(value, priv->next_song_pos);
            break;
        case PROP_NEXT_SONG_ID:
            g_value_set_int(value, priv->next_song_id);
            break;
        case PROP_ELAPSED_TIME:
            g_value_set_int(value, priv->elapsed_time);
            break;
        case PROP_ELAPSED_MS:
            g_value_set_int(value, priv->elapsed_ms);
            break;
        case PROP_TOTAL_TIME:
            g_value_set_int(value, priv->total_time);
            break;
        case PROP_KBIT_RATE:
            g_value_set_int(value, priv->kbit_rate);
            break;
        case PROP_UPDATE_ID:
            g_value_set_int(value, priv->update_id);
            break;
        case PROP_AUDIO_SAMPLE_RATE:
            g_value_set_int(value, priv->audio.sample_rate);
            break;
        case PROP_AUDIO_BITS:
            g_value_set_int(value, priv->audio.bits);
            break;
        case PROP_AUDIO_CHANNELS:
            g_value_set_int(value, priv->audio.channels);
            break;
        case PROP_NUMBER_OF_ARTISTS:
            g_value_set_int(value, priv->stats.number_of_artists);
            break;
        case PROP_NUMBER_OF_ALBUMS:
            g_value_set_int(value, priv->stats.number_of_albums);
            break;
        case PROP_NUMBER_OF_SONGS:
            g_value_set_int(value, priv->stats.number_of_songs);
            break;
        case PROP_UP_TIME:
            g_value_set_int(value, priv->stats.uptime);
            break;
        case PROP_DB_UPDATE_TIME:
            g_value_set_int(value, priv->stats.db_update_time);
            break;
        case PROP_PLAY_TIME:
            g_value_set_int(value, priv->stats.play_time);
            break;
        case PROP_REPLAY_GAIN_MODE:
            g_value_set_string(value, priv->replay_gain_mode);
            break;
        case PROP_LAST_ERROR:
            g_value_set_string(value, priv->last_error);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
        }
    }
    g_rw_lock_reader_unlock(&self->priv->ref_lock);
}

static void moose_status_finalize(GObject *gobject) {
    MooseStatus *self = MOOSE_STATUS(gobject);

    if(self == NULL) {
        return;
    }

    if(self->priv->current_song != NULL) {
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

static const GParamFlags DEFAULT_FLAGS = 0 | G_PARAM_READABLE | G_PARAM_STATIC_NICK |
                                         G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NAME;

static GParamSpec *moose_status_prop_int(const char *name) {
    return g_param_spec_int(name, name, name, G_MININT, G_MAXINT, 0, DEFAULT_FLAGS);
}

static GParamSpec *moose_status_prop_boolean(const char *name) {
    return g_param_spec_boolean(name, name, name, FALSE, DEFAULT_FLAGS);
}

static GParamSpec *moose_status_prop_string(const char *name) {
    return g_param_spec_string(name, name, name, NULL, DEFAULT_FLAGS);
}

static GParamSpec *moose_status_prop_enum(const char *name, GType enum_type) {
    return g_param_spec_enum(
        name, name, name, enum_type, MOOSE_STATE_UNKNOWN, DEFAULT_FLAGS);
}

static void moose_status_class_init(MooseStatusClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_status_finalize;
    gobject_class->get_property = moose_status_get_property;

    static GParamSpec *props[N_PROPS] = {NULL};
    props[PROP_VOLUME] = moose_status_prop_int("volume");
    props[PROP_REPEAT] = moose_status_prop_boolean("repeat");
    props[PROP_RANDOM] = moose_status_prop_boolean("random");
    props[PROP_SINGLE] = moose_status_prop_boolean("single");
    props[PROP_CONSUME] = moose_status_prop_boolean("consume");
    props[PROP_QUEUE_LENGTH] = moose_status_prop_int("queue-length");
    props[PROP_QUEUE_VERSION] = moose_status_prop_int("queue-version");
    props[PROP_STATE] = moose_status_prop_enum("state", MOOSE_TYPE_STATE);
    props[PROP_CROSSFADE] = moose_status_prop_int("crossfade");
    props[PROP_MIXRAMPDB] = moose_status_prop_int("mixrampdb");
    props[PROP_MIXRAMPDELAY] = moose_status_prop_int("mixrampdelay");
    props[PROP_SONG_POS] = moose_status_prop_int("song-pos");
    props[PROP_SONG_ID] = moose_status_prop_int("song-id");
    props[PROP_NEXT_SONG_POS] = moose_status_prop_int("next-song-pos");
    props[PROP_NEXT_SONG_ID] = moose_status_prop_int("next-song-id");
    props[PROP_ELAPSED_TIME] = moose_status_prop_int("elapsed-time");
    props[PROP_ELAPSED_MS] = moose_status_prop_int("elapsed-ms");
    props[PROP_TOTAL_TIME] = moose_status_prop_int("total-time");
    props[PROP_KBIT_RATE] = moose_status_prop_int("kbit-rate");
    props[PROP_UPDATE_ID] = moose_status_prop_int("update-id");
    props[PROP_AUDIO_SAMPLE_RATE] = moose_status_prop_int("audio-sample-rate");
    props[PROP_AUDIO_BITS] = moose_status_prop_int("audio-bits");
    props[PROP_AUDIO_CHANNELS] = moose_status_prop_int("audio-channels");
    props[PROP_NUMBER_OF_ARTISTS] = moose_status_prop_int("number-of-artists");
    props[PROP_NUMBER_OF_ALBUMS] = moose_status_prop_int("number-of-albums");
    props[PROP_NUMBER_OF_SONGS] = moose_status_prop_int("number-of-songs");
    props[PROP_UP_TIME] = moose_status_prop_int("uptime");
    props[PROP_DB_UPDATE_TIME] = moose_status_prop_int("db-update-time");
    props[PROP_PLAY_TIME] = moose_status_prop_int("play-time");
    props[PROP_REPLAY_GAIN_MODE] = moose_status_prop_string("replay-gain-mode");
    props[PROP_LAST_ERROR] = moose_status_prop_string("last-error");

    g_object_class_install_properties(G_OBJECT_CLASS(klass), N_PROPS, props);
}

static void moose_status_init(MooseStatus *self) {
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
        g_str_hash, g_direct_equal, NULL, (GDestroyNotify)g_variant_unref);
}

MooseStatus *moose_status_new(void) {
    return g_object_new(MOOSE_TYPE_STATUS, NULL);
}

void moose_status_unref(MooseStatus *self) {
    if(self != NULL) {
        g_object_unref(self);
    }
}

int moose_status_get_volume(const MooseStatus *self) {
    READ(self, volume, int, -1)
}

gboolean moose_status_get_repeat(const MooseStatus *self) {
    READ(self, repeat, bool, false)
}

gboolean moose_status_get_random(const MooseStatus *self) {
    READ(self, random, bool, false)
}

gboolean moose_status_get_single(const MooseStatus *self) {
    READ(self, single, bool, false)
}

gboolean moose_status_get_consume(const MooseStatus *self) {
    READ(self, consume, bool, false)
}

unsigned moose_status_get_queue_length(const MooseStatus *self) {
    READ(self, queue_length, unsigned, 0)
}

unsigned moose_status_get_queue_version(const MooseStatus *self) {
    READ(self, queue_version, unsigned, 0)
}

MooseState moose_status_get_state(const MooseStatus *self) {
    READ(self, state, MooseState, MOOSE_STATE_UNKNOWN)
}

unsigned moose_status_get_crossfade(const MooseStatus *self) {
    READ(self, crossfade, unsigned, 0)
}

float moose_status_get_mixrampdb(const MooseStatus *self) {
    READ(self, mixrampdb, float, 0)
}

float moose_status_get_mixrampdelay(const MooseStatus *self) {
    READ(self, mixrampdelay, float, 0)
}

int moose_status_get_song_pos(const MooseStatus *self) {
    READ(self, song_pos, int, -1)
}

int moose_status_get_song_id(const MooseStatus *self) {
    READ(self, song_id, int, -1)
}

int moose_status_get_next_song_pos(const MooseStatus *self) {
    READ(self, next_song_pos, int, -1)
}

int moose_status_get_next_song_id(const MooseStatus *self) {
    READ(self, next_song_id, int, -1)
}

unsigned moose_status_get_elapsed_time(const MooseStatus *self) {
    READ(self, elapsed_time, unsigned, 0)
}

unsigned moose_status_get_elapsed_ms(const MooseStatus *self) {
    READ(self, elapsed_ms, unsigned, 0)
}

unsigned moose_status_get_total_time(const MooseStatus *self) {
    READ(self, total_time, unsigned, 0)
}

unsigned moose_status_get_kbit_rate(const MooseStatus *self) {
    READ(self, kbit_rate, unsigned, 0)
}

unsigned moose_status_get_update_id(const MooseStatus *self) {
    READ(self, update_id, unsigned, 0)
}

const char *moose_status_get_last_error(const MooseStatus *self) {
    READ(self, last_error, const char *, NULL)
}

uint32_t moose_status_get_audio_sample_rate(const MooseStatus *self) {
    READ(self, audio.sample_rate, uint32_t, 0)
}

uint8_t moose_status_get_audio_bits(const MooseStatus *self) {
    READ(self, audio.bits, uint8_t, 0)
}

uint8_t moose_status_get_audio_channels(const MooseStatus *self) {
    READ(self, audio.channels, uint8_t, 0)
}

void moose_status_convert(MooseStatus *self, const struct mpd_status *status) {
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

        const struct mpd_audio_format *audio = mpd_status_get_audio_format(status);
        if(audio != NULL) {
            self->priv->audio.sample_rate = audio->sample_rate;
            self->priv->audio.channels = audio->channels;
            self->priv->audio.bits = audio->bits;
        }
    }
    g_rw_lock_writer_unlock(&self->priv->ref_lock);
}

MooseStatus *moose_status_new_from_struct(const struct mpd_status *status) {
    g_return_val_if_fail(status, NULL);

    MooseStatus *self = moose_status_new();
    moose_status_convert(self, status);
    return self;
}

MooseSong *moose_status_get_current_song(const MooseStatus *self) {
    g_return_val_if_fail(self, NULL);
    if(self->priv->current_song) {
        g_object_ref(MOOSE_SONG(self->priv->current_song));
    }
    return (MooseSong *)self->priv->current_song;
}

void moose_status_set_current_song(MooseStatus *self, const MooseSong *song) {
    g_rw_lock_writer_lock(&self->priv->ref_lock);
    {
        if(self->priv->current_song) {
            g_object_unref(MOOSE_SONG(song));
            self->priv->current_song = NULL;
        }

        if(self != NULL) {
            if(song != NULL) {
                g_object_ref(MOOSE_SONG(song));
            }
            self->priv->current_song = song;
        }
    }
    g_rw_lock_writer_unlock(&self->priv->ref_lock);
}

unsigned moose_status_stats_get_number_of_artists(const MooseStatus *self) {
    READ(self, stats.number_of_artists, unsigned, 0)
}

unsigned moose_status_stats_get_number_of_albums(const MooseStatus *self) {
    READ(self, stats.number_of_albums, unsigned, 0)
}

unsigned moose_status_stats_get_number_of_songs(const MooseStatus *self) {
    READ(self, stats.number_of_songs, unsigned, 0)
}

unsigned long moose_status_stats_get_uptime(const MooseStatus *self) {
    READ(self, stats.uptime, unsigned long, 0)
}

unsigned long moose_status_stats_get_db_update_time(const MooseStatus *self) {
    READ(self, stats.db_update_time, unsigned long, 0)
}

unsigned long moose_status_stats_get_play_time(const MooseStatus *self) {
    READ(self, stats.play_time, unsigned long, 0)
}

unsigned long moose_status_stats_get_db_play_time(const MooseStatus *self) {
    READ(self, stats.db_play_time, unsigned long, 0)
}

void moose_status_update_stats(const MooseStatus *self, const struct mpd_stats *stats) {
    g_assert(self);
    g_assert(stats);

    g_rw_lock_writer_lock(&self->priv->ref_lock);
    {
        self->priv->stats.number_of_artists = mpd_stats_get_number_of_artists(stats);
        self->priv->stats.number_of_albums = mpd_stats_get_number_of_albums(stats);
        self->priv->stats.number_of_songs = mpd_stats_get_number_of_songs(stats);
        self->priv->stats.uptime = mpd_stats_get_uptime(stats);
        self->priv->stats.db_update_time = mpd_stats_get_db_update_time(stats);
        self->priv->stats.play_time = mpd_stats_get_play_time(stats);
        self->priv->stats.db_play_time = mpd_stats_get_db_play_time(stats);
    }
    g_rw_lock_writer_unlock(&self->priv->ref_lock);
}

const char *moose_status_get_replay_gain_mode(const MooseStatus *self) {
    READ(self, replay_gain_mode, const char *, "off")
}

void moose_status_set_replay_gain_mode(const MooseStatus *self, const char *mode) {
    g_return_if_fail(self);

    if(mode != NULL) {
        g_rw_lock_writer_lock(&self->priv->ref_lock);
        {
            strncpy(self->priv->replay_gain_mode,
                    mode,
                    sizeof(self->priv->replay_gain_mode) - 1);
        }
        g_rw_lock_writer_unlock(&self->priv->ref_lock);
    }
}

void moose_status_outputs_clear(const MooseStatus *self) {
    g_assert(self);

    g_rw_lock_writer_lock(&self->priv->ref_lock);
    {
        GHashTable *outputs = self->priv->outputs;
        if(outputs != NULL) {
            self->priv->outputs = NULL;
            g_hash_table_unref(outputs);
            self->priv->outputs = g_hash_table_new_full(
                g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_variant_unref);
        }
    }
    g_rw_lock_writer_unlock(&self->priv->ref_lock);
}

void moose_status_outputs_add(const MooseStatus *self, struct mpd_output *output) {
    g_assert(self);
    g_assert(output);

    g_rw_lock_writer_lock(&self->priv->ref_lock);
    {
        g_hash_table_insert(self->priv->outputs,
                            (gpointer)g_strdup(mpd_output_get_name(output)),
                            g_variant_new("(sib)",
                                          mpd_output_get_name(output),
                                          mpd_output_get_id(output),
                                          mpd_output_get_enabled(output)));
    }
    g_rw_lock_writer_unlock(&self->priv->ref_lock);
}

GHashTable *moose_status_outputs_get(const MooseStatus *self) {
    g_return_val_if_fail(self, NULL);

    GHashTable *ref = NULL;
    g_rw_lock_reader_lock(&self->priv->ref_lock);
    { ref = g_hash_table_ref(self->priv->outputs); }
    g_rw_lock_reader_unlock(&self->priv->ref_lock);
    return ref;
}

int moose_status_output_lookup_id(const MooseStatus *self, const char *name) {
    g_return_val_if_fail(self, -1);
    g_return_val_if_fail(name, -1);

    int id = -1;

    GHashTable *outputs = moose_status_outputs_get(self);
    if(outputs != NULL) {
        GVariant *data_variant = g_hash_table_lookup(outputs, name);
        if(data_variant != NULL) {
            g_variant_get_child(data_variant, 1, "i", &id);
        }
        g_hash_table_unref(outputs);
    }

    return id;
}

GType moose_state_get_type(void) {
    static GType moose_state_type = 0;

    if(moose_state_type == 0) {
        static GEnumValue pattern_types[] = {
            {MOOSE_STATE_UNKNOWN, "Undefined state", "unknown"},
            {MOOSE_STATE_STOP, "Playback stopped", "stopped"},
            {MOOSE_STATE_PLAY, "Playback playing", "playing"},
            {MOOSE_STATE_PAUSE, "Playback paused", "paused"},
            {0, NULL, NULL},
        };

        moose_state_type = g_enum_register_static("MooseState", pattern_types);
    }

    return moose_state_type;
}
