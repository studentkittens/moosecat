#ifndef MOOSE_STATUS_H
#define MOOSE_STATUS_H

/**
 * SECTION: moose-status
 * @short_description: Wrapper around Statusdata.
 *
 * This is similar to struct moose_song from libmpdclient,
 * but has setter for most attributes and is reference counted.
 * Setters allow a faster deserialization from the disk.
 * Reference counting allows sharing the instances without the fear
 * of deleting it while the user still uses it.
 */

#include <glib-object.h>
#include <mpd/client.h>

#include "moose-song.h"

G_BEGIN_DECLS

/**
 * MooseState:
 *
 * Possible playing states.
 */
typedef enum _MooseState {
    /** no information available */
    MOOSE_STATE_UNKNOWN = 0,

    /** not playing */
    MOOSE_STATE_STOP = 1,

    /** playing */
    MOOSE_STATE_PLAY = 2,

    /** playing, but currently paused */
    MOOSE_STATE_PAUSE = 3,
} MooseState;

/*
 * Type macros.
 */
#define MOOSE_TYPE_STATUS \
    (moose_status_get_type())
#define MOOSE_STATUS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_STATUS, MooseStatus))
#define MOOSE_IS_STATUS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_STATUS))
#define MOOSE_STATUS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_STATUS, MooseStatusClass))
#define MOOSE_IS_STATUS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_STATUS))
#define MOOSE_STATUS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_STATUS, MooseStatusClass))

struct _MooseStatusPrivate;

typedef struct _MooseStatus {
    GObject parent;
    struct _MooseStatusPrivate * priv;
} MooseStatus;

typedef struct _MooseStatusClass {
    GObjectClass parent_class;
} MooseStatusClass;

GType moose_status_get_type(void);

/**
 * moose_status_new:
 *
 * Allocates a new #MooseStatus
 *
 * Return value: a new #MooseStatus.
 */
MooseStatus * moose_status_new(void);

/* Create a MooseStatus from a moose_status */
MooseStatus * moose_status_new_from_struct(const struct mpd_status * status);

/* Convert an existing MooseStatus from a moose_status */
void moose_status_convert(MooseStatus * self, const struct mpd_status * status);

/**
 * moose_status_unref:
 * @self: a #MooseStatus
 *
 * Unrefs a #MooseStatus
 */
void moose_status_unref(MooseStatus * self);

/**
 * moose_status_get_volume:
 * @self: a #MooseStatus
 *
 * Returns: The volume in percent from 1 to 100
 */
int moose_status_get_volume(const MooseStatus * status);

/**
 * moose_status_get_repeat:
 * @self: a #MooseStatus
 *
 * Returns: If the repeat mode is enabled
 */
bool moose_status_get_repeat(const MooseStatus * status);

/**
 * moose_status_get_random:
 * @self: a #MooseStatus
 *
 * Returns: If the random mode is enabled
 */
bool moose_status_get_random(const MooseStatus * status);

/**
 * moose_status_get_single:
 * @self: a #MooseStatus
 *
 * Returns: If the single mode is enabled
 */
bool moose_status_get_single(const MooseStatus * status);

/**
 * moose_status_get_consume:
 * @self: a #MooseStatus
 *
 * Returns: If the consume mode is enabled
 */
bool moose_status_get_consume(const MooseStatus * status);

/**
 * moose_status_get_queue_length:
 * @self: a #MooseStatus
 *
 * Returns: The amount of songs that are in the Queue 
 */
unsigned moose_status_get_queue_length(const MooseStatus * status);

/**
 * moose_status_get_queue_version:
 * @self: a #MooseStatus
 *
 * A version that indicates changes in the queue. The integer is 
 * incremented on every change.
 *
 * Returns: The Queue version.
 */
unsigned moose_status_get_queue_version(const MooseStatus * status);

/**
 * moose_status_get_state:
 * @self: a #MooseStatus
 *
 * The playing state of the server of MOOSE_STATE_UNKNOWN on error.
 *
 * Returns: one of #MooseState
 */
MooseState moose_status_get_state(const MooseStatus * status);

/**
 * moose_status_get_crossfade:
 * @self: a #MooseStatus
 *
 * The amount of time in seconds to blend between songs.
 *
 * Returns: Seconds of crossfade.
 */
unsigned moose_status_get_crossfade(const MooseStatus * status);

/**
 * moose_status_get_mixrampdb:
 * @self: a #MooseStatus
 *
 * Returns: Mixramdb in decibel
 */
float moose_status_get_mixrampdb(const MooseStatus * status);

/**
 * moose_status_get_mixrampdelay:
 * @self: a #MooseStatus
 *
 * Returns: Mixramdelay in seconds
 */
float moose_status_get_mixrampdelay(const MooseStatus * status);

/**
 * moose_status_get_song_pos:
 * @self: a #MooseStatus
 *
 * Returns: The position of the current song in the Queue, starting from 1, or -1
 */
int moose_status_get_song_pos(const MooseStatus * status);

/**
 * moose_status_get_song_id:
 * @self: a #MooseStatus
 *
 * Returns: The ID of the current song in the queue or -1.
 */
int moose_status_get_song_id(const MooseStatus * status);

/**
 * moose_status_get_next_song_pos:
 * @self: a #MooseStatus
 *
 * Returns: The Position of the next-to-be-played song in the queue or -1
 */
int moose_status_get_next_song_pos(const MooseStatus * status);

/**
 * moose_status_get_next_song_id:
 * @self: a #MooseStatus
 *
 * Returns: The ID of the next-to-be-played song in the queue or -1
 */
int moose_status_get_next_song_id(const MooseStatus * status);

/**
 * moose_status_get_elapsed_time:
 * @self: a #MooseStatus
 *
 * Returns: The amount of time in seconds the current song has elapsed or 0
 */
unsigned moose_status_get_elapsed_time(const MooseStatus * status);

/**
 * moose_status_get_elapsed_ms:
 * @self: a #MooseStatus
 *
 * Returns: The amount of time in milliseconds the current song has elapsed or 0
 */
unsigned moose_status_get_elapsed_ms(const MooseStatus * status);

/**
 * moose_status_get_total_time:
 * @self: a #MooseStatus
 *
 * Returns: How many seconds the song has to play totally.
 */
unsigned moose_status_get_total_time(const MooseStatus * status);

/**
 * moose_status_get_kbit_rate:
 * @self: a #MooseStatus
 *
 * Note: Normally the kbit rate will only update only on every relevant event.
 * Therefore this won't give correct results for dynamically encoded audio
 * always.  If you want real-time updates, you gonna have to use
 * moose_client_timer_set_active() 
 *
 * Returns: The kbit-rate of the currenlty playing song.
 */
unsigned moose_status_get_kbit_rate(const MooseStatus * status);

/**
 * moose_status_get_last_error:
 * @self: a #MooseStatus
 *
 * If any error happended, the last one is stored here.
 * Normally, this function is not needed, since you're supposed
 * to use g_log_set_handler()
 *
 * Returns: (transfer: none): A descriptive error message.
 */
const char * moose_status_get_last_error(const MooseStatus * self);

/**
 * moose_status_get_update_id:
 * @self: a #MooseStatus
 *
 * Get the Update-ID, if an MPD-Upate is running. The exact value is normally
 * uninteresting.
 *
 * Returns: A unique ID of the update, or 0 if one is running.
 */
unsigned moose_status_get_update_id(const MooseStatus * status);

/**
 * moose_status_get_audio_sample_rate:
 * @self: a #MooseStatus
 *
 * Returns: The sample rate of the currenty playing audio (like 44000) or 0 
 */
uint32_t moose_status_get_audio_sample_rate(const MooseStatus * self);

/**
 * moose_status_get_audio_bits:
 * @self: a #MooseStatus
 *
 * Returns: How many bits were used in the audio :-)
 */
uint8_t moose_status_get_audio_bits(const MooseStatus * self);

/**
 * moose_status_get_audio_channels:
 * @self: a #MooseStatus
 *
 * Note: In most cases this will return 2. 
 *
 * Returns: The number of audio channels the current song has or 0
 */
uint8_t moose_status_get_audio_channels(const MooseStatus * self);

/**
 * moose_status_stats_get_number_of_artists:
 * @self: a #MooseStatus
 *
 * Returns: Total number of artists in the database
 */
unsigned moose_status_stats_get_number_of_artists(const MooseStatus * self);

/**
 * moose_status_stats_get_number_of_albums:
 * @self: a #MooseStatus
 *
 * Returns: Total number of albums in the database
 */
unsigned moose_status_stats_get_number_of_albums(const MooseStatus * self);

/**
 * moose_status_stats_get_number_of_songs:
 * @self: a #MooseStatus
 *
 * Returns: Total number of songs in the database
 */
unsigned moose_status_stats_get_number_of_songs(const MooseStatus * self);

/**
 * moose_status_stats_get_uptime:
 * @self: a #MooseStatus
 *
 * Returns: Total number of seconds the server is already running.
 */
unsigned long moose_status_stats_get_uptime(const MooseStatus * self);

/**
 * moose_status_stats_get_db_update_time:
 * @self: a #MooseStatus
 *
 * Returns: Total number of seconds since the last Database update.
 */
unsigned long moose_status_stats_get_db_update_time(const MooseStatus * self);

/**
 * moose_status_stats_get_play_time:
 * @self: a #MooseStatus
 *
 * Returns: Total number of seconds this mpd server was playing.
 */
unsigned long moose_status_stats_get_play_time(const MooseStatus * self);

/**
 * moose_status_stats_get_db_play_time:
 * @self: a #MooseStatus
 *
 * Returns: Sum of seconds the songs in the database have.
 */
unsigned long moose_status_stats_get_db_play_time(const MooseStatus * self);

/**
 * moose_status_get_replay_gain_mode:
 * @self: a #MooseStatus
 * 
 * Might be one of "off", "album", "artist"
 *
 * Returns: (transfer none): The replay gain mode.
 */
const char * moose_status_get_replay_gain_mode(const MooseStatus * self);

/**
 * moose_status_get_current_song:
 * @self: a #MooseStatus
 * 
 * Returns a  #MooseSong. A ref is taken, use g_object_unref() when done.
 *
 * Returns: a reffed #MooseSong
 */
MooseSong * moose_status_get_current_song(const MooseStatus * self);

/**
 * moose_status_outputs_get:
 * @self: a #MooseStatus
 * 
 * Returns a reffed #GHashTable, which maps output names to 
 * a #GVariant of the output-name, the output-id and an boolean, indicating
 * if the output is enabled. The format string you can use to unpack this
 * #GVariant is "(sib)". A ref was taken with g_hash_table_ref(), use
 * g_hash_table_unref() when done.
 *
 * Hint: You can use something like this to switch the output-state:
 *
 * moose_client_send(client, "('output-switch', 'name of the output', TRUE)");
 *
 * Returns: (type (s(sib))): a reffed #GHashTable 
 */
GHashTable * moose_status_outputs_get(const MooseStatus * self);

/** 
 * moose_status_output_lookup_id: skip:
 * @self: a #MooseStatus
 * 
 * This is used internally.
 *
 * Returns: The Output-ID of the specified song or -1
 */
int moose_status_output_lookup_id(const MooseStatus * self, const char * name);

G_END_DECLS

#endif /* end of include guard: MOOSE_STATUS_H */
