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

///////////////////////////////

static void moose_song_finalize(GObject * gobject)
{
    MooseSong * self = MOOSE_SONG(gobject);

    if (self == NULL)
        return;

    for(size_t i = 0; i < MOOSE_TAG_COUNT; ++i) {
        g_free(self->priv->tags[i]);
    }
    g_free(self->priv->uri);

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

///////////////////////////////

static void moose_song_class_init(MooseSongClass * klass)
{
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_song_finalize;
}

///////////////////////////////

static void moose_song_init(MooseSong * self)
{
    self->priv = moose_song_get_instance_private(self);
    // memset(self->priv->tags, 0, sizeof(char *) * MOOSE_TAG_COUNT);
    // self->priv->uri = NULL;
    // self->priv->duration = 0;
    // self->priv->last_modified = 0;
    // self->priv->pos = 0;
    // self->priv->id = 0;
    // self->priv->prio = 0;
}

///////////////////////////////
//          PUBLIC           //
///////////////////////////////

MooseSong * moose_song_new(void)
{
    MooseSong * self = g_object_new(MOOSE_TYPE_SONG, NULL);
    return self;
}

void moose_song_destroy(MooseSong * self)
{
    g_object_unref(self);
}

///////////////////////////////

char * moose_song_get_tag(MooseSong * self, MooseTagType tag)
{
    g_return_val_if_fail(tag < 0 || tag >= MOOSE_TAG_COUNT, NULL);
    return self->priv->tags[tag];
}

void moose_song_set_tag(MooseSong * self, MooseTagType tag, const char * value) 
{ 
    g_return_if_fail(tag < 0 || tag >= MOOSE_TAG_COUNT);
    self->priv->tags[tag] = g_strdup(value);
}

///////////////////////////////

const char * moose_song_get_uri(MooseSong * self)
{
    g_assert(self);
    return self->priv->uri;
}

void moose_song_set_uri(MooseSong * self, const char * uri)
{
    g_assert(self);
    self->priv->uri = g_strdup(uri);
}

///////////////////////////////

unsigned moose_song_get_duration(MooseSong * self)
{
    g_assert(self);
    return self->priv->duration;
}

void moose_song_set_duration(MooseSong * self, unsigned duration)
{
    g_assert(self);
    self->priv->duration = duration;
}

///////////////////////////////

time_t moose_song_get_last_modified(MooseSong * self)
{
    g_assert(self);
    return self->priv->last_modified;
}

void moose_song_set_last_modified(MooseSong * self, time_t last_modified)
{
    g_assert(self);
    self->priv->last_modified = last_modified;
}

///////////////////////////////

unsigned moose_song_get_pos(MooseSong * self)
{
    g_assert(self);
    return self->priv->pos;
}

void moose_song_set_pos(MooseSong * self, unsigned pos)
{
    g_assert(self);
    self->priv->pos = pos;
}

///////////////////////////////

unsigned moose_song_get_id(MooseSong * self)
{
    g_assert(self);
    return self->priv->id;
}

void moose_song_set_id(MooseSong * self, unsigned id)
{
    g_assert(self);
    self->priv->id = id;
}

///////////////////////////////

unsigned moose_song_get_prio(MooseSong * self)
{
    g_assert(self);
    return self->priv->prio;
}

void moose_song_set_prio(MooseSong * self, unsigned prio)
{
    g_assert(self);
    self->priv->prio = prio;
}
