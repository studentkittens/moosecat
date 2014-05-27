#include <stdbool.h>
#include <string.h>

#include "moose-song.h"

typedef struct _MooseSongPrivate {
    char * uri;
    char * tags[MOOSE_TAG_COUNT];

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
    unsigned pos;

    /**
     * The id of this song within the queue.
     */
    unsigned id;

    /**
     * The priority of this song within the queue.
     */
    unsigned prio;
} MooseSongPrivate;


G_DEFINE_TYPE_WITH_PRIVATE(MooseSong, moose_song, G_TYPE_OBJECT);



static void moose_song_finalize(GObject * gobject) {
    MooseSong * self = MOOSE_SONG(gobject);

    if (self == NULL) {
        return;
    }

    for (size_t i = 0; i < MOOSE_TAG_COUNT; ++i) {
        g_free(self->priv->tags[i]);
    }
    g_free(self->priv->uri);

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}



static void moose_song_class_init(MooseSongClass * klass) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_song_finalize;
}



static void moose_song_init(MooseSong * self) {
    self->priv = moose_song_get_instance_private(self);
}


//          PUBLIC           //


MooseSong * moose_song_new(void) {
    return g_object_new(MOOSE_TYPE_SONG, NULL);
}

void moose_song_unref(MooseSong * self) {
    if (self != NULL) {
        g_object_unref(self);
    }
}



char * moose_song_get_tag(MooseSong * self, MooseTagType tag) {
    g_return_val_if_fail(tag >= 0 && tag < MOOSE_TAG_COUNT, NULL);
    return self->priv->tags[tag];
}

void moose_song_set_tag(MooseSong * self, MooseTagType tag, const char * value) {
    g_return_if_fail(tag >= 0 && tag < MOOSE_TAG_COUNT);

    if (self->priv->tags[tag]) {
        g_free(self->priv->tags[tag]);
    }
    self->priv->tags[tag] = g_strdup(value);
}



const char * moose_song_get_uri(MooseSong * self) {
    g_assert(self);
    return self->priv->uri;
}

void moose_song_set_uri(MooseSong * self, const char * uri) {
    g_assert(self);
    if (self->priv->uri) {
        g_free(self->priv->uri);
    }
    self->priv->uri = g_strdup(uri);
}



unsigned moose_song_get_duration(MooseSong * self) {
    g_assert(self);
    return self->priv->duration;
}

void moose_song_set_duration(MooseSong * self, unsigned duration) {
    g_assert(self);
    self->priv->duration = duration;
}



time_t moose_song_get_last_modified(MooseSong * self) {
    g_assert(self);
    return self->priv->last_modified;
}

void moose_song_set_last_modified(MooseSong * self, time_t last_modified) {
    g_assert(self);
    self->priv->last_modified = last_modified;
}



unsigned moose_song_get_pos(MooseSong * self) {
    g_assert(self);
    return self->priv->pos;
}

void moose_song_set_pos(MooseSong * self, unsigned pos) {
    g_assert(self);
    self->priv->pos = pos;
}



unsigned moose_song_get_id(MooseSong * self) {
    g_assert(self);
    return self->priv->id;
}

void moose_song_set_id(MooseSong * self, unsigned id) {
    g_assert(self);
    self->priv->id = id;
}



unsigned moose_song_get_prio(MooseSong * self) {
    g_assert(self);
    return self->priv->prio;
}

void moose_song_set_prio(MooseSong * self, unsigned prio) {
    g_assert(self);
    self->priv->prio = prio;
}



void moose_song_convert(MooseSong * self, struct mpd_song * song) {
    g_assert(self);
    g_assert(song);

    for (size_t i = 0; i < MOOSE_TAG_COUNT; ++i) {
        const char * value = mpd_song_get_tag(song, i, 0);
        if (value != NULL) {
            moose_song_set_tag(self, i, value);
        }
    }

    moose_song_set_uri(self, mpd_song_get_uri(song));
    moose_song_set_pos(self, mpd_song_get_pos(song));
    moose_song_set_id(self, mpd_song_get_id(song));
    moose_song_set_last_modified(self, mpd_song_get_last_modified(song));
    moose_song_set_duration(self, mpd_song_get_duration(song));
    moose_song_set_prio(self, mpd_song_get_prio(song));
}

MooseSong * moose_song_new_from_struct(struct mpd_song * song) {
    if (song == NULL) {
        return NULL;
    }

    MooseSong * self = moose_song_new();
    moose_song_convert(self, song);
    return self;
}
