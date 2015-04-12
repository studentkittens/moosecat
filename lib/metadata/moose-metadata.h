#ifndef MOOSE_METADATA_H
#define MOOSE_METADATA_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#include "moose-metadata-query.h"
#include "moose-metadata-cache.h"

#define MOOSE_METADATA(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), moose_metadata_get_type(), MooseMetadata))

GType moose_metadata_get_type(void);

typedef struct _MooseMetadata { GObject parent; } MooseMetadata;

typedef struct _MooseMetadataClass { GObjectClass parent; } MooseMetadataClass;

/**
 * moose_metadata_new:
 *
 * Returns: A newly allocated threads-helper, with a refcount of 1
 */
MooseMetadata *moose_metadata_new(void);

/**
 * moose_metadata_commit:
 * @self: a #MooseMetadata retriever
 * @qry: (transfer full): a #MooseMetadataQuery
 *
 * Push a query onto the retriever.
 */
void moose_metadata_commit(MooseMetadata *self, MooseMetadataQuery *qry);

/**
 * moose_metadata_cancel:
 * @self: a #MooseMetadata retriever
 *
 * Cancel all currently running queries.
 * They will still generate a signal.
 */
void moose_metadata_cancel(MooseMetadata *self);

/**
 * moose_metadata_unref:
 * @self: a #MooseMetadata retriever
 *
 * Unref a retriever.
 */
void moose_metadata_unref(MooseMetadata *self);

//////////////////////////

#endif
