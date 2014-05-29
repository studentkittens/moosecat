/* External */
#include <glib.h>
#include <glib-object.h>
#include <string.h>

/* Internal */
#include "moose-store-completion.h"
#include "moose-store.h"

/* Internal, but not mine */
#include "libart/art.h"

typedef struct _MooseStoreCompletionPrivate {
    MooseStore * store;
    art_tree * trees[MPD_TAG_COUNT];
} MooseStoreCompletionPrivate;

typedef struct _MooseStoreCompletionIterTag {
    MooseStoreCompletion * self;
    char * result;
    MooseTagType tag;
} MooseStoreCompletionIterTag;

enum {
    PROP_STORE = 1,
    PROP_N
};

G_DEFINE_TYPE_WITH_PRIVATE(
    MooseStoreCompletion, moose_store_completion, G_TYPE_OBJECT
);

static void moose_store_completion_clear(MooseStoreCompletion * self) {
    g_assert(self);

    for (size_t i = 0; i < MPD_TAG_COUNT; ++i) {
        art_tree * tree = self->priv->trees[i];
        self->priv->trees[i] = NULL;

        if (tree != NULL) {
            destroy_art_tree(tree);
            g_slice_free(art_tree, tree);
        }
    }
}

static void moose_store_completion_client_event(
    G_GNUC_UNUSED MooseClient * client,
    G_GNUC_UNUSED MooseIdle event,
    void * user_data
) {
    if ((event & MOOSE_IDLE_DATABASE) == 0) {
        return;  /* Not interesting */
    }
    MooseStoreCompletion * self = user_data;
    moose_store_completion_clear(self);
}

static void moose_store_completion_finalize(GObject * gobject) {
    MooseStoreCompletion * self = MOOSE_STORE_COMPLETION(gobject);
    if (self == NULL) {
        return;
    }

    moose_store_completion_clear(self);

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

static void moose_store_completion_get_property(
    GObject * object,
    guint property_id,
    GValue * value,
    GParamSpec * pspec) {
    MooseStoreCompletionPrivate * priv = MOOSE_STORE_COMPLETION(object)->priv;

    switch(property_id) {
    case PROP_STORE:
        g_value_set_object(value, priv->store);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_store_completion_set_property(
    GObject * object,
    guint property_id,
    const GValue * value,
    GParamSpec * pspec) {
    MooseStoreCompletionPrivate* priv = MOOSE_STORE_COMPLETION(object)->priv;

    switch(property_id) {
    case PROP_STORE:
        priv->store = g_value_get_object(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_store_completion_class_init(MooseStoreCompletionClass * klass) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_store_completion_finalize;
    gobject_class->get_property = moose_store_completion_get_property;
    gobject_class->set_property = moose_store_completion_set_property;

    GParamSpec * pspec = NULL;
    pspec = g_param_spec_object(
                "store",
                "Store",
                "Store, used to read the completion from",
                MOOSE_TYPE_STORE,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
            );

    /**
     * MooseStoreCompletion:store: (type MOOSE_TYPE_STORE)
     *
     * The store to read the completion from.
     */
    g_object_class_install_property(gobject_class, PROP_STORE, pspec);
}

static void moose_store_completion_init(MooseStoreCompletion * self) {
    self->priv = moose_store_completion_get_instance_private(self);

    MooseStore *store = NULL;
    MooseClient *client = NULL;

    g_object_get(self, "store", &store, NULL);
    g_object_get(store, "client", &client, NULL);

    g_assert(store);
    g_assert(client);

    g_signal_connect(
        client,
        "client-event",
        G_CALLBACK(moose_store_completion_client_event),
        self
    );
    g_object_unref(store);
    g_object_unref(client);
}

static char * moose_store_completion_normalize_string(const char * input) {
    char * utf8_lower = g_utf8_strdown(input, -1);
    if (utf8_lower != NULL) {
        char * normalized = g_strstrip(
                                g_utf8_normalize(utf8_lower, -1, G_NORMALIZE_DEFAULT)
                            );

        g_free(utf8_lower);
        return normalized;
    }
    return NULL;
}

static art_tree * moose_store_completion_create_index(MooseStoreCompletion * self, MooseTagType tag) {
    g_assert(self);
    g_assert(tag < MOOSE_TAG_COUNT);

    MoosePlaylist * copy = moose_playlist_new();
    moose_store_gw(self->priv->store,
                   moose_store_search_to_stack(
                       self->priv->store, "*", FALSE, copy, -1
                   )
                  );

    art_tree * tree = g_slice_new0(art_tree);
    init_art_tree(tree);

    size_t copylen = moose_playlist_length(copy);
    for (size_t i = 0; i < copylen; ++i) {
        MooseSong * song = moose_playlist_at(copy, i);
        if (song != NULL) {
            const char * word = moose_song_get_tag(song, tag);
            if (word != NULL) {
                char * normalized = moose_store_completion_normalize_string(word);
                art_insert(
                    tree, (unsigned char *)normalized, strlen(normalized),
                    GINT_TO_POINTER(i)
                );
                g_free(normalized);
            }
        }
    }
    self->priv->trees[tag] = tree;
    moose_store_release(self->priv->store);
    g_object_unref(copy);

    return tree;
}

static int moose_store_completion_art_callback(void * user_data, const unsigned char * key, uint32_t key_len, void * value) {
    /* copy the first suggestion, key is not nul-terminated. */
    MooseStoreCompletionIterTag * data = user_data;
    int song_idx = GPOINTER_TO_INT(value);

    MoosePlaylist *full_playlist = NULL;

    g_object_get(data->self->priv->store, "full-playlist", &full_playlist, NULL);
    if(full_playlist != NULL) {
        return 1;
    }

    MooseSong * song = moose_playlist_at(full_playlist, song_idx);
    if (song != NULL) {
        /* Try to retrieve the original song */
        data->result = g_strdup(moose_song_get_tag(song, data->tag));
    }

    if (data->result == NULL) {
        /* Fallback */
        data->result = g_strndup((const char *)key, key_len);
    }

    g_object_unref(full_playlist);

    /* stop right after the first suggestion */
    return 1;
}

void moose_store_completion_unref(MooseStoreCompletion * self) {
    g_assert(self);

    if (self != NULL) {
        g_object_unref(self);
    }
}

char * moose_store_completion_lookup(MooseStoreCompletion * self, MooseTagType tag, const char * key) {
    g_assert(self);
    g_assert(tag < MOOSE_TAG_COUNT);

    /* Get an Index (the patricia tree) */
    art_tree * tree = self->priv->trees[tag];
    if (tree == NULL) {
        tree = moose_store_completion_create_index(self, tag);
    }

    /* NULL-keys are okay, those pre-compute the index. */
    if (key == NULL) {
        return NULL;
    }

    char * normalized_key = moose_store_completion_normalize_string(key);
    if (normalized_key == NULL) {
        return NULL;
    }

    /* libart copies the chars, so we do not need to lock the database here */
    MooseStoreCompletionIterTag data = {.self = self, .tag = tag, .result = NULL};
    art_iter_prefix(
        tree, (unsigned char *)normalized_key, strlen(key),
        moose_store_completion_art_callback, &data
    );

    g_free(normalized_key);
    return data.result;
}
