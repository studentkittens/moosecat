#ifndef MOOSE_STORE_COMPLETION_H
#define MOOSE_STORE_COMPLETION_H

/**
 * SECTION: moose-store-completion
 * @short_description: Utility for completing incomplete strings to their full version. 
 *
 * This uses internally Radix-Tree-Caches to make this operation very efficient.
 * The caches will be created on their first access, so no memory is wasted when
 * you do not use this feature. If the store changes, all caches are
 * invalidated.
 *
 * The Store offers with moose_store_get_completion() a convinient instancing
 * method for this. 
 */

#include <glib-object.h>
#include "../mpd/moose-mpd-client.h"

G_BEGIN_DECLS
/*
 * Type macros.
 */
#define MOOSE_TYPE_STORE_COMPLETION \
    (moose_store_completion_get_type())
#define MOOSE_STORE_COMPLETION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_STORE_COMPLETION, MooseStoreCompletion))
#define MOOSE_IS_STORE_COMPLETION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_STORE_COMPLETION))
#define MOOSE_STORE_COMPLETION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_STORE_COMPLETION, MooseStoreCompletionClass))
#define MOOSE_IS_STORE_COMPLETION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_STORE_COMPLETION))
#define MOOSE_STORE_COMPLETION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_STORE_COMPLETION, MooseStoreCompletionClass))

struct _MooseStoreCompletionPrivate;

typedef struct _MooseStoreCompletion {
    GObject parent;
    struct _MooseStoreCompletionPrivate * priv;
} MooseStoreCompletion;

typedef struct _MooseStoreCompletionClass {
    GObjectClass parent_class;
} MooseStoreCompletionClass;

GType moose_store_completion_get_type(void);

/**
 * moose_store_completion_lookup:
 * @self: a #MooseStoreCompletion
 * @tag: a #MooseTagType
 * @key: The key to complete for the appropiate tag.
 *
 * If there are more than possiblities, the first alphabetically matching is
 * taken. E.g. when completing "Ab", "Abba" is returned before "Abel".
 *
 * Returns: (transfer full): The most matching full version or NULL.
 */
char * moose_store_completion_lookup(
    MooseStoreCompletion * self,
    MooseTagType tag,
    const char * key
);

/**
 * moose_store_completion_unref:
 * @self: a #MooseStoreCompletion
 *
 * Unrefs a #MooseStoreCompletion
 */
void moose_store_completion_unref(MooseStoreCompletion * self);

G_END_DECLS

#endif /* end of include guard: MOOSE_STORE_COMPLETION_H */
