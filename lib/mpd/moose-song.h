#ifndef MOOSE_SONG_H
#define MOOSE_SONG_H

/**
 * SECTION: moose-song
 * @short_description: Wrapper around Songdata.
 *
 * This is similar to struct mpd_song from libmpdclient,
 * but has setter for most attributes and is reference counted.
 * Setters allow a faster deserialization from the disk.
 * Reference counting allows sharing the instances without the fear
 * of deleting it while the user still uses it.
 */

#include <glib-object.h>
#include <mpd/client.h>

G_BEGIN_DECLS

/**
 * MooseZeroconfState:
 *
 * Possible tags stored by MooseSong.
 * This reflects mpd_tag_type.
 */
typedef enum _MooseTagType {
    MOOSE_TAG_UNKNOWN = MPD_TAG_UNKNOWN,

    MOOSE_TAG_ARTIST = MPD_TAG_ARTIST,
    MOOSE_TAG_ALBUM = MPD_TAG_ALBUM,
    MOOSE_TAG_ALBUM_ARTIST = MPD_TAG_ALBUM_ARTIST,
    MOOSE_TAG_TITLE = MPD_TAG_TITLE,
    MOOSE_TAG_TRACK = MPD_TAG_TRACK,
    MOOSE_TAG_NAME = MPD_TAG_NAME,
    MOOSE_TAG_GENRE = MPD_TAG_GENRE,
    MOOSE_TAG_DATE = MPD_TAG_DATE,
    MOOSE_TAG_COMPOSER = MPD_TAG_COMPOSER,
    MOOSE_TAG_PERFORMER = MPD_TAG_PERFORMER,
    MOOSE_TAG_COMMENT = MPD_TAG_COMMENT,
    MOOSE_TAG_DISC = MPD_TAG_DISC,

    MOOSE_TAG_OPTIONAL_COUNT = MPD_TAG_MUSICBRAINZ_ARTISTID,

    MOOSE_TAG_MUSICBRAINZ_ARTISTID = MPD_TAG_MUSICBRAINZ_ARTISTID,
    MOOSE_TAG_MUSICBRAINZ_ALBUMID = MPD_TAG_MUSICBRAINZ_ALBUMID,
    MOOSE_TAG_MUSICBRAINZ_ALBUMARTISTID = MPD_TAG_MUSICBRAINZ_ALBUMARTISTID,
    MOOSE_TAG_MUSICBRAINZ_TRACKID = MPD_TAG_MUSICBRAINZ_TRACKID,

    MOOSE_TAG_COUNT = MPD_TAG_COUNT
} MooseTagType;

/*
 * Type macros.
 */
#define MOOSE_TYPE_SONG \
    (moose_song_get_type())
#define MOOSE_SONG(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_SONG, MooseSong))
#define MOOSE_IS_SONG(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_SONG))
#define MOOSE_SONG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_SONG, MooseSongClass))
#define MOOSE_IS_SONG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_SONG))
#define MOOSE_SONG_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_SONG, MooseSongClass))

struct _MooseSongPrivate;

typedef struct _MooseSong {
    GObject parent;
    struct _MooseSongPrivate * priv;
} MooseSong;

typedef struct _MooseSongClass {
    GObjectClass parent_class;
} MooseSongClass;

GType moose_song_get_type(void);

/**
 * moose_song_new:
 *
 * Allocates a new #MooseSong
 *
 * Return value: a new #MooseSong.
 */
MooseSong * moose_song_new(void);

/* Create a MooseSong from a mpd_song */
MooseSong * moose_song_new_from_struct(struct mpd_song * song);

/* Convert an existing MooseSong from a mpd_song */
void moose_song_convert(MooseSong * self, struct mpd_song * song);

/**
 * moose_song_unref:
 * @self: a #MooseSong
 *
 * Unrefs a #MooseSong
 */
void moose_song_unref(MooseSong * self);

/**
 * moose_song_get_tag:
 * @self: a #MooseSong
 * @tag: one of #MooseTagType
 *
 * Get a certain tag from the song.
 *
 * Returns: (transfer none): The tag as string.
 */
char * moose_song_get_tag(MooseSong * self, MooseTagType tag);

/**
 * moose_song_get_uri:
 * @self: a #MooseSong
 *
 * Get the Uri (i.e. Filename of the song, if ^file://) of the Song.
 *
 * Returns: (transfer none): The uri as string.
 */
const char * moose_song_get_uri(MooseSong * self);

/**
 * moose_song_get_duration:
 * @self: a #MooseSong
 *
 * Get the Duration of the song.
 *
 * Returns: The Duration
 */
unsigned moose_song_get_duration(MooseSong * self);

/**
 * moose_song_get_last_modified:
 * @self: a #MooseSong
 *
 * Get the last_modified of the song.
 *
 * Returns: The last_modified
 */
time_t moose_song_get_last_modified(MooseSong * self);

/**
 * moose_song_get_pos:
 * @self: a #MooseSong
 *
 * Get the pos of the song.
 *
 * Returns: The pos
 */
unsigned moose_song_get_pos(MooseSong * self);

/**
 * moose_song_get_id:
 * @self: a #MooseSong
 *
 * Get the id of the song.
 *
 * Returns: The id
 */
unsigned moose_song_get_id(MooseSong * self);

/**
 * moose_song_get_prio:
 * @self: a #MooseSong
 *
 * Get the prio of the song.
 *
 * Returns: The prio
 */
unsigned moose_song_get_prio(MooseSong * self);

G_END_DECLS

#endif /* end of include guard: MOOSE_SONG_H */
