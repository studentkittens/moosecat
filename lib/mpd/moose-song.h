#ifndef MOOSE_SONG_H
#define MOOSE_SONG_H

/**
 * SECTION: moose-song
 * @short_description: Wrapper around Songdata.
 *
 * This basically is a wrapper around #GPtrArray.
 */
#include <glib-object.h>
#include <mpd/client.h>

G_BEGIN_DECLS

/* This reflects mpd_tag_type */
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

///////////////////////////////

/**
 * moose_song_new:
 *
 * Allocates a new #MooseSong
 *
 * Return value: a new #MooseSong.
 */
MooseSong * moose_song_new(void);
MooseSong * moose_song_new_from_struct(struct mpd_song * song);

void moose_song_convert(MooseSong * self, struct mpd_song * song);

void moose_song_free(MooseSong * self);

char * moose_song_get_tag(MooseSong * self, MooseTagType tag);
void moose_song_set_tag(MooseSong * self, MooseTagType tag, const char * value);
const char * moose_song_get_uri(MooseSong * self);
void moose_song_set_uri(MooseSong * self, const char * uri);
unsigned moose_song_get_duration(MooseSong * self);
void moose_song_set_duration(MooseSong * self, unsigned duration);
time_t moose_song_get_last_modified(MooseSong * self);
void moose_song_set_last_modified(MooseSong * self, time_t last_modified);
unsigned moose_song_get_pos(MooseSong * self);
void moose_song_set_pos(MooseSong * self, unsigned pos);
unsigned moose_song_get_id(MooseSong * self);
void moose_song_set_id(MooseSong * self, unsigned id);
unsigned moose_song_get_prio(MooseSong * self);
void moose_song_set_prio(MooseSong * self, unsigned prio);

G_END_DECLS

#endif /* end of include guard: MOOSE_SONG_H */
