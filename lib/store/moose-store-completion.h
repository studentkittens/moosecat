#ifndef MOOSE_DB_COMPLETION_H
#define MOOSE_DB_COMPLETION_H

G_BEGIN_DECLS

/**
 * SECTION: moose-status
 * @short_description: Wrapper around Statusdata.
 *
 * This is similar to struct moose_song from libmpdclient,
 * but has setter for most attributes and is reference counted.
 * Setters allow a faster deserialization from the disk.
 * Reference counting allows sharing the instances without the fear
 * of deleting it while the user still uses it.
 */

#include "../mpd/moose-mpd-client.h"
#include <glib-object.h>

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

#endif /* end of include guard: MOOSE_DB_COMPLETION_H */
