#ifndef MOOSE_METADATA_QUERY_PRIVATE_H
#define MOOSE_METADATA_QUERY_PRIVATE_H

#include <glyr/glyr.h>
#include "moose-metadata-query.h"

G_BEGIN_DECLS

/**
 * moose_metadata_query_get_internal: skip:
 * @query: A #MooseMetadataQuery
 *
 * Returns: the internal GlyrQuery
 */
GlyrQuery *moose_metadata_query_get_internal(MooseMetadataQuery *query);

G_END_DECLS

#endif
