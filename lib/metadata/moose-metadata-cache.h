#ifndef MOOSE_METADATA_CACHE_H
#define MOOSE_METADATA_CACHE_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MOOSE_METADATA_CACHE(obj)                                       \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), moose_metadata_cache_get_type(), \
                                MooseMetadataCache))

typedef struct _MooseMetadataCache { GObject parent; } MooseMetadataCache;

typedef struct _MooseMetadataCacheClass { GObjectClass parent; } MooseMetadataCacheClass;

GType moose_metadata_cache_get_type(void);

G_END_DECLS

#endif
