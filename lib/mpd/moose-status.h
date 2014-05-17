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

typedef enum _MooseState {
	/** no information available */
	MOOSE_STATE_UNKNOWN = 0,

	/** not playing */
	MOOSE_STATE_STOP = 1,

	/** playing */
	MOOSE_STATE_PLAY = 2,

	/** playing, but paused */
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

///////////////////////////////

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
 * moose_status_free:
 * @self: a #MooseStatus
 *
 * Unrefs a #MooseStatus
 */
void moose_status_free(MooseStatus * self);



int moose_status_get_volume(const MooseStatus *status);

/**
 * Returns true if repeat mode is on.
 */
bool moose_status_get_repeat(const MooseStatus *status);

/**
 * Returns true if random mode is on.
 */
bool moose_status_get_random(const MooseStatus *status);

/**
 * Returns true if single mode is on.
 */
bool moose_status_get_single(const MooseStatus *status);

/**
 * Returns true if consume mode is on.
 */
bool moose_status_get_consume(const MooseStatus *status);

/**
 * Returns the number of songs in the queue.  If MPD did not
 * specify that, this function returns 0.
 */
unsigned moose_status_get_queue_length(const MooseStatus *status);

/**
 * Returns queue version number.  You may use this to determine
 * when the queue has changed since you have last queried it.
 */
unsigned moose_status_get_queue_version(const MooseStatus *status);

/**
 * Returns the state of the player: either stopped, playing or paused.
 */
MooseState moose_status_get_state(const MooseStatus *status);

/**
 * Returns crossfade setting in seconds.  0 means crossfading is
 * disabled.
 */
unsigned moose_status_get_crossfade(const MooseStatus *status);

/**
 * Returns mixrampdb setting in db.
 */
float moose_status_get_mixrampdb(const MooseStatus *status);

/**
 * Returns mixrampdelay setting in seconds.  Negative means mixramp is
 * disabled.
 */
float moose_status_get_mixrampdelay(const MooseStatus *status);

/**
 * Returns the position of the currently playing song in the queue
 * (beginning with 0) if a song is currently selected (always the case when
 * state is PLAY or PAUSE).  If there is no current song, -1 is returned.
 */
int moose_status_get_song_pos(const MooseStatus *status);

/**
 * Returns the id of the current song.  If there is no current song,
 * -1 is returned.
 */
int moose_status_get_song_id(const MooseStatus *status);

/**
 * The same as moose_status_get_next_song_pos, but for the next song to be
 * played.
 */
int moose_status_get_next_song_pos(const MooseStatus *status);

/**
 * Returns the id of the next song to be played.  If it is not known, -1 is
 * returned.
 */
int moose_status_get_next_song_id(const MooseStatus *status);

/**
 * Returns time in seconds that have elapsed in the currently playing/paused
 * song
 */
unsigned moose_status_get_elapsed_time(const MooseStatus *status);

/**
 * Returns time in milliseconds that have elapsed in the currently
 * playing/paused song.
 */
unsigned moose_status_get_elapsed_ms(const MooseStatus *status);

/**
 * Returns the length in seconds of the currently playing/paused song
 */
unsigned moose_status_get_total_time(const MooseStatus *status);

/**
 * Returns current bit rate in kbps.  0 means unknown.
 */
unsigned moose_status_get_kbit_rate(const MooseStatus *status);

const char * moose_status_get_last_error(const MooseStatus * self);

/**
 * Returns 1 if mpd is updating, 0 otherwise
 */
unsigned moose_status_get_update_id(const MooseStatus *status);

uint32_t moose_status_get_audio_sample_rate(const MooseStatus *self);

uint8_t moose_status_get_audio_bits(const MooseStatus *self);

uint8_t moose_status_get_audio_channels(const MooseStatus *self);


const MooseSong * moose_status_get_current_song(const MooseStatus * self);

unsigned moose_status_stats_get_number_of_artists(const MooseStatus *self);
unsigned moose_status_stats_get_number_of_albums(const MooseStatus *self);
unsigned moose_status_stats_get_number_of_songs(const MooseStatus *self);
unsigned long moose_status_stats_get_uptime(const MooseStatus *self);
unsigned long moose_status_stats_get_db_update_time(const MooseStatus *self);
unsigned long moose_status_stats_get_play_time(const MooseStatus *self);
unsigned long moose_status_stats_get_db_play_time(const MooseStatus *self);
void moose_status_update_stats(const MooseStatus *self, const struct mpd_stats *stats);

const char * moose_status_get_replay_gain_mode(const MooseStatus * self);
void moose_status_set_replay_gain_mode(const MooseStatus * self, const char *mode);

G_END_DECLS

#endif /* end of include guard: MOOSE_STATUS_H */
