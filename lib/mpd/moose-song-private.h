#ifndef MOOSE_SONG_PRIVATE_H
#define MOOSE_SONG_PRIVATE_H

#include "moose-song.h"

G_BEGIN_DECLS

/* moose_song_new_from_struct: skip:
 * @song: The mpd_song to convert.
 *
 * Create a MooseSong from a mpd_song 
 *
 * Returns: a newly allocated #MooseSong with a refcount of 1
 * */
MooseSong * moose_song_new_from_struct(struct mpd_song * song);

/* moose_song_convert: skip:
 * @self: an empty #MooseSong
 * @song: The mpd_song to convert.
 *
 * Fills in the values of the mpd_song into an empty #MooseSong
 * */
void moose_song_convert(MooseSong * self, struct mpd_song * song);


/**
 * moose_song_set_prio:
 * @self: a #MooseSong
 * @prio: The prio to set.
 *
 * Set the prio.
 */
void moose_song_set_prio(MooseSong * self, unsigned prio);

/**
 * moose_song_set_id:
 * @self: a #MooseSong
 * @id: The id to set.
 *
 * Set the id.
 */
void moose_song_set_id(MooseSong * self, unsigned id);

/**
 * moose_song_set_pos:
 * @self: a #MooseSong
 * @pos: The pos to set.
 *
 * Set the pos.
 */
void moose_song_set_pos(MooseSong * self, unsigned pos);

/**
 * moose_song_set_duration:
 * @self: a #MooseSong
 * @duration: The duration to set.
 *
 * Set the duration.
 */
void moose_song_set_last_modified(MooseSong * self, time_t last_modified);

/**
 * moose_song_set_duration:
 * @self: a #MooseSong
 * @duration: The duration to set.
 *
 * Set the duration.
 */
void moose_song_set_duration(MooseSong * self, unsigned duration);

/**
 * moose_song_set_uri:
 * @self: a #MooseSong
 * @uri: The Uri to set.
 *
 * Get the Uri (i.e. Filename of the song, if ^file://) of the Song.
 *
 * Returns: (transfer none): The uri as string.
 */
void moose_song_set_uri(MooseSong * self, const char * uri);

/**
 * moose_song_set_tag:
 * @self: a #MooseSong
 * @tag: one of #MooseTagType
 * @value: The string to set.
 *
 * Set a certain tag to the song.
 */
void moose_song_set_tag(MooseSong * self, MooseTagType tag, const char * value);

G_END_DECLS

#endif /* end of include guard: MOOSE_SONG_PRIVATE_H */
