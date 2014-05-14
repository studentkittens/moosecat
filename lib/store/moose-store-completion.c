/* External */
#include <glib.h>
#include <string.h>

/* Internal */
#include "moose-store.h"
#include "moose-store-private.h"
#include "moose-store-completion.h"

/* Internal, but not mine */
#include "libart/art.h"

typedef struct MooseStoreCompletion {
    MooseStore * store;
    art_tree * trees[MPD_TAG_COUNT];
} MooseStoreCompletion;

typedef struct MooseStoreCompletionIterTag {
    MooseStoreCompletion * self;
    char * result;
    enum mpd_tag_type tag;
} MooseStoreCompletionIterTag;

////////////////////////////////////////////
//            IMPLEMENTATION              //
////////////////////////////////////////////

static char * moose_store_cmpl_normalize_string(const char * input)
{
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

////////////////////////////////////////////

static art_tree * moose_store_cmpl_create_index(MooseStoreCompletion * self, enum mpd_tag_type tag)
{
    g_assert(self);
    g_assert(tag < MPD_TAG_COUNT);

    MoosePlaylist * copy = moose_playlist_new();
    moose_store_gw(self->store,
                   moose_store_search_to_stack(
                       self->store, "*", FALSE, copy, -1
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
                char * normalized = moose_store_cmpl_normalize_string(word);
                art_insert(
                    tree, (unsigned char *)normalized, strlen(normalized),
                    GINT_TO_POINTER(i)
                    );
                g_free(normalized);
            }
        }
    }
    self->trees[tag] = tree;
    moose_store_release(self->store);
    g_object_unref(copy);

    return tree;
}

////////////////////////////////////////////

static int moose_store_cmpl_art_callback(void * user_data, const unsigned char * key, uint32_t key_len, void * value)
{
    /* copy the first suggestion, key is not nul-terminated. */
    MooseStoreCompletionIterTag * data = user_data;
    int song_idx = GPOINTER_TO_INT(value);

    moose_stprv_lock_attributes(data->self->store); {
        MooseSong * song = moose_playlist_at(data->self->store->stack, song_idx);
        if (song != NULL) {
            /* Try to retrieve the original song */
            data->result = g_strdup(moose_song_get_tag(song, data->tag));
        }
    }
    moose_stprv_unlock_attributes(data->self->store);

    if (data->result == NULL) {
        /* Fallback */
        data->result = g_strndup((const char *)key, key_len);
    }

    /* stop right after the first suggestion */
    return 1;
}

////////////////////////////////////////////

static void moose_store_cmpl_clear(MooseStoreCompletion * self)
{
    g_assert(self);

    for (size_t i = 0; i < MPD_TAG_COUNT; ++i) {
        art_tree * tree = self->trees[i];
        self->trees[i] = NULL;

        if (tree != NULL) {
            destroy_art_tree(tree);
            g_slice_free(art_tree, tree);
        }
    }
}

////////////////////////////////////////////

static void moose_store_cmpl_client_event(
    G_GNUC_UNUSED MooseClient * client,
    G_GNUC_UNUSED enum mpd_idle event,
    void * user_data
    )
{
    MooseStoreCompletion * self = user_data;
    moose_store_cmpl_clear(self);
}

////////////////////////////////////////////
//               PUBLIC API               //
////////////////////////////////////////////

MooseStoreCompletion * moose_store_cmpl_new(struct MooseStore * store)
{
    g_assert(store);

    MooseStoreCompletion * self = g_slice_new0(MooseStoreCompletion);
    self->store = store;
    moose_signal_add_masked(
        self->store->client, "client-event",
        moose_store_cmpl_client_event, self,
        MPD_IDLE_DATABASE
        );
    return self;
}

////////////////////////////////////////////

void moose_store_cmpl_free(MooseStoreCompletion * self)
{
    g_assert(self);

    moose_store_cmpl_clear(self);

    self->store = NULL;
    g_slice_free(MooseStoreCompletion, self);
}

////////////////////////////////////////////

char * moose_store_cmpl_lookup(MooseStoreCompletion * self, enum mpd_tag_type tag, const char * key)
{
    g_assert(self);
    g_assert(tag < MPD_TAG_COUNT);

    /* Get an Index (the patricia tree) */
    art_tree * tree = self->trees[tag];
    if (tree == NULL) {
        tree = moose_store_cmpl_create_index(self, tag);
    }

    /* NULL-keys are okay, those pre-compute the index. */
    if (key == NULL) {
        return NULL;
    }

    char * normalized_key = moose_store_cmpl_normalize_string(key);
    if (normalized_key == NULL) {
        return NULL;
    }

    /* libart copies the chars, so we do not need to lock the database here */
    MooseStoreCompletionIterTag data = {.self = self, .tag = tag, .result = NULL};
    art_iter_prefix(
        tree, (unsigned char *)normalized_key, strlen(key),
        moose_store_cmpl_art_callback, &data
        );

    g_free(normalized_key);
    return data.result;
}
