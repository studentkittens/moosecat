#include <stdbool.h>
#include <string.h>

#include "moose-status.h"

typedef struct _MooseStatusPrivate {
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

	/** the current audio format */
	
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
	const char *error;
} MooseStatusPrivate;


G_DEFINE_TYPE_WITH_PRIVATE(MooseStatus, moose_status, G_TYPE_OBJECT);

///////////////////////////////

static void moose_status_finalize(GObject * gobject)
{
    MooseStatus * self = MOOSE_STATUS(gobject);

    if (self == NULL) {
        return;
    }

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

///////////////////////////////

static void moose_status_class_init(MooseStatusClass * klass)
{
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_status_finalize;
}

///////////////////////////////

static void moose_status_init(MooseStatus * self)
{
    self->priv = moose_status_get_instance_private(self);
    self->priv->state = MOOSE_STATE_UNKNOWN;
    self->priv->mixrampdb = 0.0;
    self->priv->mixrampdelay = 0.0;
    self->priv->song_pos= -1;
    self->priv->song_id = -1;
    self->priv->next_song_pos= -1;
    self->priv->next_song_id = -1;
}

///////////////////////////////
//          PUBLIC           //
///////////////////////////////

MooseStatus * moose_status_new(void)
{
    return g_object_new(MOOSE_TYPE_STATUS, NULL);
}

void moose_status_free(MooseStatus * self)
{
    g_object_unref(self);
}

///////////////////////////////

int moose_status_get_volume(const MooseStatus *self)
{
    g_return_val_if_fail(self, MOOSE_STATE_NO_VOLUME);
    return self->priv->volume;
}

bool moose_status_get_repeat(const MooseStatus *self)
{
    g_return_val_if_fail(self, false);
    return self->priv->repeat;
}

bool moose_status_get_random(const MooseStatus *self)
{
    g_return_val_if_fail(self, false);
    return self->priv->random;
}

bool moose_status_get_single(const MooseStatus *self)
{
    g_return_val_if_fail(self, false);
    return self->priv->single;
}

bool moose_status_get_consume(const MooseStatus *self)
{
    g_return_val_if_fail(self, false);
    return self->priv->consume;
}

unsigned moose_status_get_queue_length(const MooseStatus *self)
{
    g_return_val_if_fail(self, 0);
    return self->priv->queue_length;
}

unsigned moose_status_get_queue_version(const MooseStatus *self)
{
    g_return_val_if_fail(self, 0);
    return self->priv->queue_version;
}

MooseState moose_status_get_state(const MooseStatus *self)
{
    g_return_val_if_fail(self, MOOSE_STATE_UNKNOWN);
    return self->priv->state;
}

unsigned moose_status_get_crossfade(const MooseStatus *self)
{
    g_return_val_if_fail(self, 0);
    return self->priv->crossfade;
}

float moose_status_get_mixrampdb(const MooseStatus *self)
{
    g_return_val_if_fail(self, 0);
    return self->priv->mixrampdb;
}

float moose_status_get_mixrampdelay(const MooseStatus *self)
{
    g_return_val_if_fail(self, 0);
    return self->priv->mixrampdelay;
}

int moose_status_get_song_pos(const MooseStatus *self)
{
    g_return_val_if_fail(self, 0);
    return self->priv->song_pos;
}

int moose_status_get_song_id(const MooseStatus *self)
{
    g_return_val_if_fail(self, -1);
    return self->priv->song_id;
}

int moose_status_get_next_song_pos(const MooseStatus *self)
{
    g_return_val_if_fail(self, -1);
    return self->priv->next_song_pos;
}

int moose_status_get_next_song_id(const MooseStatus *self)
{
    g_return_val_if_fail(self, -1);
    return self->priv->next_song_id;
}

unsigned moose_status_get_elapsed_time(const MooseStatus *self)
{
    g_return_val_if_fail(self, 0);
    return self->priv->elapsed_time;
}


unsigned moose_status_get_elapsed_ms(const MooseStatus *self)
{
    g_return_val_if_fail(self, -1);
    return self->priv->elapsed_ms;
}


unsigned moose_status_get_total_time(const MooseStatus *self)
{
    g_return_val_if_fail(self, -1);
    return self->priv->total_time;
}


unsigned moose_status_get_kbit_rate(const MooseStatus *self)
{
    g_return_val_if_fail(self, -1);
    return self->priv->kbit_rate;
}


unsigned moose_status_get_update_id(const MooseStatus *self)
{
    g_return_val_if_fail(self, -1);
    return self->priv->update_id;
}

const char * moose_status_get_last_error(const MooseStatus * self)
{
    g_return_val_if_fail(self, NULL);
    return self->priv->error;
}

//////////// AUDIO ///////////////

uint32_t moose_status_get_audio_sample_rate(const MooseStatus *self)
{
    g_return_val_if_fail(self, 0);
    return self->priv->audio.sample_rate;
}

uint8_t moose_status_get_audio_bits(const MooseStatus *self)
{
    g_return_val_if_fail(self, 0);
    return self->priv->audio.bits;
}

uint8_t moose_status_get_audio_channels(const MooseStatus *self)
{
    g_return_val_if_fail(self, 0);
    return self->priv->audio.channels;
}

void moose_status_convert(MooseStatus *self, const struct mpd_status *status)
{
    g_assert(self);
    g_assert(status);

    self->priv->volume = mpd_status_get_volume(status);
    self->priv->repeat = mpd_status_get_repeat(status);
    self->priv->random = mpd_status_get_random(status);
    self->priv->single = mpd_status_get_single(status);
    self->priv->consume = mpd_status_get_consume(status);
    self->priv->queue_length = mpd_status_get_queue_length(status);
    self->priv->queue_version = mpd_status_get_queue_version(status);
    self->priv->state = mpd_status_get_state(status);
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
    self->priv->error = mpd_status_get_error(status);

    const struct mpd_audio_format *audio = mpd_status_get_audio_format(status);
    if(audio != NULL) {
        self->priv->audio.sample_rate = audio->sample_rate;
        self->priv->audio.channels = audio->channels;
        self->priv->audio.bits = audio->bits;
    }
}

MooseStatus * moose_status_new_from_struct(const struct mpd_status * status)
{
    g_return_val_if_fail(status, NULL);

    MooseStatus * self = moose_status_new();
    moose_status_convert(self, status);
    return self;
}
