#include <stdbool.h>
#include <string.h>

#include "moose-song.h"

typedef struct _MooseSongPrivate {
    char* uri;
    char* tags[MOOSE_TAG_COUNT];

    /* In seconds */
    unsigned duration;

    /**
     * The POSIX UTC time stamp of the last modification, or 0 if
     * that is unknown.
     */
    time_t last_modified;

    /**
     * The position of this song within the queue.
     */
    int pos;

    /**
     * The id of this song within the queue.
     */
    int id;

    /**
     * The priority of this song within the queue.
     */
    unsigned prio;
} MooseSongPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(MooseSong, moose_song, G_TYPE_OBJECT);

enum {
    PROP_URI = 1,
    /* Not all tags have properties yet. */
    PROP_ARTIST,
    PROP_ALBUM,
    PROP_ALBUM_ARTIST,
    PROP_TITLE,
    PROP_TRACK,
    PROP_GENRE,
    PROP_DATE,
    PROP_LAST_MODIFIED,
    PROP_POS,
    PROP_ID,
    PROP_PRIO,
    N_PROPS
};

/* This is implemented for easier introspection support */
static void moose_song_get_property(GObject* object,
                                    guint property_id,
                                    GValue* value,
                                    GParamSpec* pspec) {
    MooseSong* self = MOOSE_SONG(object);
    MooseSongPrivate* priv = self->priv;
    switch(property_id) {
    case PROP_URI:
        g_value_set_string(value, priv->uri);
        break;
    case PROP_ARTIST:
        g_value_set_string(value, priv->tags[MOOSE_TAG_ARTIST]);
        break;
    case PROP_ALBUM:
        g_value_set_string(value, priv->tags[MOOSE_TAG_ALBUM]);
        break;
    case PROP_ALBUM_ARTIST:
        g_value_set_string(value, priv->tags[MOOSE_TAG_ALBUM_ARTIST]);
        break;
    case PROP_TITLE:
        g_value_set_string(value, priv->tags[MOOSE_TAG_TITLE]);
        break;
    case PROP_TRACK:
        g_value_set_string(value, priv->tags[MOOSE_TAG_TRACK]);
        break;
    case PROP_GENRE:
        g_value_set_string(value, priv->tags[MOOSE_TAG_GENRE]);
        break;
    case PROP_DATE:
        g_value_set_string(value, priv->tags[MOOSE_TAG_DATE]);
        break;
    case PROP_LAST_MODIFIED:
        g_value_set_uint(value, priv->last_modified);
        break;
    case PROP_POS:
        g_value_set_int(value, priv->pos);
        break;
    case PROP_ID:
        g_value_set_int(value, priv->id);
        break;
    case PROP_PRIO:
        g_value_set_uint(value, priv->prio);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_song_finalize(GObject* gobject) {
    MooseSong* self = MOOSE_SONG(gobject);

    if(self == NULL) {
        return;
    }

    for(size_t i = 0; i < MOOSE_TAG_COUNT; ++i) {
        g_free(self->priv->tags[i]);
    }
    g_free(self->priv->uri);

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

static const GParamFlags DEFAULT_FLAGS = 0 | G_PARAM_READABLE | G_PARAM_STATIC_NICK |
                                         G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NAME;

static GParamSpec* moose_status_prop_int(const char* name) {
    return g_param_spec_int(name, name, name, G_MININT, G_MAXINT, 0, DEFAULT_FLAGS);
}

static GParamSpec* moose_status_prop_uint(const char* name) {
    return g_param_spec_uint(name, name, name, 0, G_MAXUINT, 0, DEFAULT_FLAGS);
}

static GParamSpec* moose_status_prop_string(const char* name) {
    return g_param_spec_string(name, name, name, NULL, DEFAULT_FLAGS);
}

static void moose_song_class_init(MooseSongClass* klass) {
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_song_finalize;
    gobject_class->get_property = moose_song_get_property;

    static GParamSpec* props[N_PROPS] = {NULL};

    props[PROP_URI] = moose_status_prop_string("uri");
    props[PROP_ARTIST] = moose_status_prop_string("artist");
    props[PROP_ALBUM] = moose_status_prop_string("album");
    props[PROP_ALBUM_ARTIST] = moose_status_prop_string("album-artist");
    props[PROP_TITLE] = moose_status_prop_string("title");
    props[PROP_TRACK] = moose_status_prop_string("track");
    props[PROP_GENRE] = moose_status_prop_string("genre");
    props[PROP_DATE] = moose_status_prop_string("date");

    props[PROP_LAST_MODIFIED] = moose_status_prop_uint("last-modified");
    props[PROP_POS] = moose_status_prop_int("pos");
    props[PROP_ID] = moose_status_prop_int("id");
    props[PROP_PRIO] = moose_status_prop_uint("prio");

    g_object_class_install_properties(G_OBJECT_CLASS(klass), N_PROPS, props);
}

static void moose_song_init(MooseSong* self) {
    self->priv = moose_song_get_instance_private(self);
    memset(self->priv->tags, 0, sizeof(self->priv->tags));
}

MooseSong* moose_song_new(void) {
    return g_object_new(MOOSE_TYPE_SONG, NULL);
}

void moose_song_unref(MooseSong* self) {
    if(self != NULL) {
        g_object_unref(self);
    }
}

char* moose_song_get_tag(MooseSong* self, MooseTagType tag) {
    g_return_val_if_fail(tag >= 0 && tag < MOOSE_TAG_COUNT, NULL);
    return self->priv->tags[tag];
}

void moose_song_set_tag(MooseSong* self, MooseTagType tag, const char* value) {
    g_return_if_fail(tag >= 0 && tag < MOOSE_TAG_COUNT);

    if(self->priv->tags[tag]) {
        g_free(self->priv->tags[tag]);
    }
    self->priv->tags[tag] = g_strdup(value);
}

const char* moose_song_get_uri(MooseSong* self) {
    g_assert(self);
    return self->priv->uri;
}

void moose_song_set_uri(MooseSong* self, const char* uri) {
    g_assert(self);
    if(self->priv->uri) {
        g_free(self->priv->uri);
    }
    self->priv->uri = g_strdup(uri);
}

unsigned moose_song_get_duration(MooseSong* self) {
    g_assert(self);
    return self->priv->duration;
}

void moose_song_set_duration(MooseSong* self, unsigned duration) {
    g_assert(self);
    self->priv->duration = duration;
}

time_t moose_song_get_last_modified(MooseSong* self) {
    g_assert(self);
    return self->priv->last_modified;
}

void moose_song_set_last_modified(MooseSong* self, time_t last_modified) {
    g_assert(self);
    self->priv->last_modified = last_modified;
}

int moose_song_get_pos(MooseSong* self) {
    g_assert(self);
    return self->priv->pos;
}

void moose_song_set_pos(MooseSong* self, int pos) {
    g_assert(self);
    self->priv->pos = pos;
}

int moose_song_get_id(MooseSong* self) {
    g_assert(self);
    return self->priv->id;
}

void moose_song_set_id(MooseSong* self, int id) {
    g_assert(self);
    self->priv->id = id;
}

unsigned moose_song_get_prio(MooseSong* self) {
    g_assert(self);
    return self->priv->prio;
}

void moose_song_set_prio(MooseSong* self, unsigned prio) {
    g_assert(self);
    self->priv->prio = prio;
}

void moose_song_convert(MooseSong* self, struct mpd_song* song) {
    g_assert(self);
    g_assert(song);

    for(size_t i = 0; i < MOOSE_TAG_COUNT; ++i) {
        const char* value = mpd_song_get_tag(song, i, 0);
        if(value != NULL) {
            moose_song_set_tag(self, i, value);
        }
    }

    moose_song_set_uri(self, mpd_song_get_uri(song));
    moose_song_set_pos(self, mpd_song_get_pos(song));
    moose_song_set_id(self, mpd_song_get_id(song));
    moose_song_set_last_modified(self, mpd_song_get_last_modified(song));
    moose_song_set_duration(self, mpd_song_get_duration(song));
#if LIBMPDCLIENT_CHECK_VERSION(2, 9, 0)
    moose_song_set_prio(self, mpd_song_get_prio(song));
#else
    moose_song_set_prio(self, 0);
#endif
}

MooseSong* moose_song_new_from_struct(struct mpd_song* song) {
    if(song == NULL) {
        return NULL;
    }

    MooseSong* self = moose_song_new();
    moose_song_convert(self, song);
    return self;
}

GType moose_tag_type_get_type(void) {
    static GType enum_type = 0;

    if(enum_type == 0) {
        static GEnumValue tag_types[] = {
            {MOOSE_TAG_UNKNOWN, "MOOSE_TAG_UNKNOWN", ""},
            {MOOSE_TAG_ARTIST, "MOOSE_TAG_ARTIST", "artist"},
            {MOOSE_TAG_ALBUM, "MOOSE_TAG_ALBUM", "album"},
            {MOOSE_TAG_ALBUM_ARTIST, "MOOSE_TAG_ALBUM_ARTIST", "album-artist"},
            {MOOSE_TAG_TITLE, "MOOSE_TAG_TITLE", "title"},
            {MOOSE_TAG_TRACK, "MOOSE_TAG_TRACK", "track"},
            {MOOSE_TAG_NAME, "MOOSE_TAG_NAME", "name"},
            {MOOSE_TAG_GENRE, "MOOSE_TAG_GENRE", "genre"},
            {MOOSE_TAG_DATE, "MOOSE_TAG_DATE", "date"},
            {MOOSE_TAG_COMPOSER, "MOOSE_TAG_COMPOSER", "composer"},
            {MOOSE_TAG_PERFORMER, "MOOSE_TAG_PERFORMER", "performer"},
            {MOOSE_TAG_COMMENT, "MOOSE_TAG_COMMENT", "comment"},
            {MOOSE_TAG_DISC, "MOOSE_TAG_DISC", "disc"},
            {MOOSE_TAG_MUSICBRAINZ_ARTISTID, "MOOSE_TAG_MUSICBRAINZ_ARTISTID",
             "musicbrainz-artist-id"},
            {MOOSE_TAG_MUSICBRAINZ_ALBUMID, "MOOSE_TAG_MUSICBRAINZ_ALBUMID",
             "musicbrainz-album-id"},
            {MOOSE_TAG_MUSICBRAINZ_ALBUMARTISTID, "MOOSE_TAG_MUSICBRAINZ_ALBUMARTISTID",
             "muscibrainz-albumartist-id"},
            {MOOSE_TAG_MUSICBRAINZ_TRACKID, "MOOSE_TAG_MUSICBRAINZ_TRACKID",
             "musicbrainz-track-id"},
            {0, NULL, NULL}};

        enum_type = g_enum_register_static("MooseTagType", tag_types);
    }

    return enum_type;
}
