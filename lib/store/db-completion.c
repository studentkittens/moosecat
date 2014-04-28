/* External */
#include <glib.h>
#include <string.h>

/* Internal */
#include "db.h"
#include "db-private.h"
#include "db-completion.h"

/* Internal, but not mine */
#include "libart/art.h"

typedef struct mc_StoreCompletion {
    mc_Store *store;
    art_tree *trees[MPD_TAG_COUNT];
} mc_StoreCompletion;

////////////////////////////////////////////
//            IMPLEMENTATION              //
////////////////////////////////////////////

static art_tree* mc_store_cmpl_create_index(mc_StoreCompletion *self, enum mpd_tag_type tag)
{
    g_assert(self);
    g_assert(tag < MPD_TAG_COUNT);

    mc_Stack * copy = mc_stack_create(1000, NULL);
    mc_store_gw(self->store,
        mc_store_search_to_stack(
            self->store, "*", FALSE, copy, -1
        )
    );

    art_tree *tree = g_slice_new0(art_tree);
    init_art_tree(tree);

    size_t copylen = mc_stack_length(copy);
    for(size_t i = 0; i < copylen; ++i) {
        struct mpd_song *song = mc_stack_at(copy, i);
        if(song != NULL) {
            const char *word = mpd_song_get_tag(song, tag, 0);
            if(word != NULL) {
                art_insert(tree, (unsigned char *)word, strlen(word), song);
            }
        }
    }
    self->trees[tag] = tree;
    mc_store_release(self->store);
    mc_stack_free(copy);

    return tree;
}

////////////////////////////////////////////

static int mc_store_cmpl_art_callback(void *data, const unsigned char *key, uint32_t key_len, void *value)
{
    /* copy the first suggestion, key is not nul-terminated. */
    *((char **)data) = g_strndup((const char *)key, key_len);

    /* stop right after the first suggestion */
    return 1;
}

////////////////////////////////////////////

static void mc_store_cmpl_clear(mc_StoreCompletion *self)
{
    g_assert(self);

    for(size_t i = 0; i < MPD_TAG_COUNT; ++i) {
        art_tree *tree = self->trees[i];
        self->trees[i] = NULL;

        if(tree != NULL) {
            destroy_art_tree(tree);
            g_slice_free(art_tree, tree);
        }
    }
}

////////////////////////////////////////////

static void mc_store_cmpl_client_event(mc_Client *client, enum mpd_idle event, void *user_data)
{
    mc_StoreCompletion *self = user_data;
    mc_store_cmpl_clear(self);
}

////////////////////////////////////////////
//               PUBLIC API               //
////////////////////////////////////////////

mc_StoreCompletion * mc_store_cmpl_new(struct mc_Store *store)
{
    g_assert(store);

    mc_StoreCompletion * self = g_slice_new0(mc_StoreCompletion);
    self->store = store;
    mc_signal_add_masked(
        self->store->client, "client-event",
        mc_store_cmpl_client_event, self, 
        MPD_IDLE_DATABASE
    );
    return self;
}

////////////////////////////////////////////

void mc_store_cmpl_free(mc_StoreCompletion *self)
{
    g_assert(self);

    mc_store_cmpl_clear(self);

    self->store = NULL;
    g_slice_free(mc_StoreCompletion, self);
}

////////////////////////////////////////////

char * mc_store_cmpl_lookup(mc_StoreCompletion *self, enum mpd_tag_type tag, const char *key) 
{
    g_assert(self);
    g_assert(tag < MPD_TAG_COUNT);

    /* Get an Index (the patricia tree) */
    art_tree *tree = self->trees[tag];
    if(tree == NULL) {
        tree = mc_store_cmpl_create_index(self, tag);
    }

    /* NULL-keys are okay, those pre-compute the index. */
    if(key == NULL) {
        return NULL;
    }

    /* libart copies the chars, so we do not need to lock the database here */
    char *suggestion = NULL;
    art_iter_prefix(
        tree, (unsigned char *)key, strlen(key),
        mc_store_cmpl_art_callback, &suggestion
    );

    return suggestion;
}
