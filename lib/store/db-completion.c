/* External */
#include <glib.h>

/* Internal */
#include "db.h"
#include "db-completion.h"
#include "patricia/patricia.h"


typedef struct mc_StoreCompletion {
    mc_Store *store;
    patricia_tree *trees[MPD_TAG_COUNT];
} mc_StoreCompletion;

////////////////////////////////////////////
//            IMPLEMENTATION              //
////////////////////////////////////////////

static void mc_store_cmpl_create_index(mc_StoreCompletion *self, enum mpd_tag_type tag)
{
    g_assert(self);
    g_assert(tag < MPD_TAG_COUNT);

    mc_Stack *copy = mc_store_get_full_stack_copy(self->store);
    size_t copylen = mc_stack_length(copy);

    patricia_tree *tree = g_slice_new0(patricia_tree);
    patricia_create(tree, patricia_nocase_translator);

    for(size_t i = 0; i < copylen; ++i) {
        struct mpd_song *song = mc_stack_at(copy, i);
        const char *word = mpd_song_get_tag(song, tag, 0);
        patricia_insert(tree, (unsigned char *)word, song);
    }
    self->trees[tag] = tree;

    mc_store_release(self->store);
    mc_stack_free(copy);
}

////////////////////////////////////////////

static struct mpd_song *mc_store_cmpl_complete_node(mc_StoreCompletion *self, patricia_node *node)
{
    g_assert(self); 
    g_assert(node);

    if(node->edges == NULL)
        return NULL;

    while(node->edges != NULL) {
        node = node->edges[0].child;
    }
    
    return (node) ? node->data : NULL;
}

////////////////////////////////////////////
//               PUBLIC API               //
////////////////////////////////////////////

mc_StoreCompletion * mc_store_cmpl_new(struct mc_Store *store)
{
    g_assert(store);

    mc_StoreCompletion * self = g_slice_new0(mc_StoreCompletion);
    self->store = store;
    return self;
}

////////////////////////////////////////////

void mc_store_cmpl_free(mc_StoreCompletion *self)
{
    g_assert(self);

    for(size_t i = 0; i < MPD_TAG_COUNT; ++i) {
        patricia_tree *tree = self->trees[i];
        self->trees[i] = NULL;

        if(tree != NULL) {
            patricia_cleanup(tree);
            g_slice_free(patricia_tree, tree);
        }
    }

    self->store = NULL;
    g_slice_free(mc_StoreCompletion, self);
}

////////////////////////////////////////////


char * mc_store_cmpl_lookup(mc_StoreCompletion *self, enum mpd_tag_type tag, const char *key) 
{
    g_assert(self);

    return NULL;    
}
