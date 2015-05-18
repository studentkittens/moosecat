#ifndef MOOSE_METADATA_QUERY_H
#define MOOSE_METADATA_QUERY_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MOOSE_METADATA_QUERY(obj)                                       \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), moose_metadata_query_get_type(), \
                                MooseMetadataQuery))

typedef struct _MooseMetadataQuery { GObject parent; } MooseMetadataQuery;

typedef struct _MooseMetadataQueryClass { GObjectClass parent; } MooseMetadataQueryClass;

GType moose_metadata_query_get_type(void);

/* There is no _new for MooseMetadataQuery.  Use g_object_new() with the
 * */

/**
 * moose_metadata_query_get_results:
 * @query: A #MooseMetadataQuery
 *
 * Retrieve the result list of finished items.
 * If nothing was found or an error happened, NULL is returned.
 *
 * Returns: (element-type MooseMetadataCache) (transfer container): List of
 *#MooseMetadataCache
 */
GList *moose_metadata_query_get_results(MooseMetadataQuery *query);

/**
 * moose_metadata_query_commit:
 * @query: A #MooseMetadataQuery
 *
 */
void moose_metadata_query_commit(MooseMetadataQuery *query);

/**
 * moose_metadata_query_cancel:
 * @query: A #MooseMetadataQuery
 *
 * Try to cancel the query early. This function will return immediately,
 * and there is no guarantee that the query will actually cancel soon.
 */
void moose_metadata_query_cancel(MooseMetadataQuery *query);

/**
 * moose_metadata_get_error:
 * @query: A #MooseMetadataQuery
 *
 * Get the last happened error or NULL if none happened.
 */
const char *moose_metadata_query_get_error(MooseMetadataQuery *query);

/**
 * moose_metadata_query_unref:
 * @query: A #MooseMetadataQuery
 *
 * Unref a query.
 */
void moose_metadata_query_unref(MooseMetadataQuery *query);

/**
 * moose_metadata_query_enable_debug:
 * @query: A #MooseMetadataQuery
 *
 * Make the query print what it's doing to stdout.
 */
void moose_metadata_query_enable_debug(MooseMetadataQuery *query);

/**
 * moose_metadata_query_equal:
 * @query: A #MooseMetadataQuery
 * @other: Another #MooseMetadataQuery
 *
 * Checks for the two queries to be *essentially* the same. This does not mean
 * that they are same in all aspects but are likely to give the same result
 * when committed. This is useful to check if the query needs an update.
 *
 * Returns: TRUE if they are essentially the same.
 */
gboolean moose_metadata_query_equal(MooseMetadataQuery *query, MooseMetadataQuery *other);

G_END_DECLS

#endif
