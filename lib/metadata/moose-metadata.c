#include "../misc/moose-misc-threads.h"
#include "../moose-config.h"
#include "moose-metadata.h"
#include "moose-metadata-query-private.h"

#include <glyr/glyr.h>
#include <glyr/cache.h>

enum { SIGNAL_QUERY_DONE, SIGNAL_ITEM_RECV, N_SIGNALS };
enum { PROP_DATABASE_LOCATION = 1, N_PROPS };

static int SIGNALS[N_SIGNALS];

////////////////////////////////

typedef struct _MooseMetadataPrivate {
    MooseThreads *pool;
    GlyrDatabase *database;
    GQueue *running_queries;
    GMutex query_mtx;
} MooseMetadataPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(MooseMetadata, moose_metadata, G_TYPE_OBJECT);

MooseMetadata *moose_metadata_new(void) {
    return g_object_new(moose_metadata_get_type(), NULL);
}

static void moose_metadata_finalize(GObject *gobject) {
    MooseMetadata *self = MOOSE_METADATA(gobject);
    g_assert(self);

    MooseMetadataPrivate *priv = moose_metadata_get_instance_private(self);
    glyr_db_destroy(priv->database);
    moose_threads_unref(priv->pool);
    g_queue_free_full(priv->running_queries, g_object_unref);
    g_mutex_clear(&priv->query_mtx);

    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

static void moose_metadata_get_property(GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec) {
    MooseMetadataPrivate *priv =
        moose_metadata_get_instance_private(MOOSE_METADATA(object));

    switch(property_id) {
    case PROP_DATABASE_LOCATION:
        if(priv->database) {
            g_value_set_string(value, priv->database->root_path);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_metadata_set_property(GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec) {
    MooseMetadataPrivate *priv =
        moose_metadata_get_instance_private(MOOSE_METADATA(object));

    switch(property_id) {
    case PROP_DATABASE_LOCATION:
        if(priv->database) {
            glyr_db_destroy(priv->database);
            priv->database = NULL;
        }
        priv->database = glyr_db_init(g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_metadata_class_init(MooseMetadataClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_metadata_finalize;
    gobject_class->get_property = moose_metadata_get_property;
    gobject_class->set_property = moose_metadata_set_property;

    glyr_init();
    atexit(glyr_cleanup);

    /**
     * MooseMetadata:query-done:
     * @query: The query that has finished.
     *
     * Emitted once the query was processed (succesful or not)
     */
    SIGNALS[SIGNAL_QUERY_DONE] = g_signal_new(
        "query-done", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
        G_TYPE_NONE /* return_type */, 1 /* n_params */, moose_metadata_query_get_type());

    /**
     * MooseMetadata:item-received:
     * @cache: The item we just received.
     *
     * Connect to this signal when you want to be notified on individual items.
     * This is moslty useful for download many images and wanting to display
     * them immediately. For number=1 just use query-done.
     */
    SIGNALS[SIGNAL_ITEM_RECV] = g_signal_new(
        "item-received", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
        G_TYPE_NONE /* return_type */, 1 /* n_params */, moose_metadata_cache_get_type());

    g_object_class_install_property(
        gobject_class, PROP_DATABASE_LOCATION,
        g_param_spec_string("database-location", "database-location",
                            "Where to put the database", g_get_user_cache_dir(),
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static GLYR_ERROR moose_metadata_on_item_received(GlyrMemCache *item,
                                                  GlyrQuery *internal) {
    MooseMetadata *self = MOOSE_METADATA(internal->callback.user_pointer);

    MooseMetadataCache *cache = g_object_new(moose_metadata_cache_get_type(), "pointer",
                                             item, "owns-pointer", FALSE, NULL);

    g_signal_emit(self, SIGNALS[SIGNAL_ITEM_RECV], 0, cache);
    g_object_unref(cache);

    return GLYRE_OK;
}

static gpointer moose_metadata_on_thread(G_GNUC_UNUSED MooseThreads *threads,
                                         MooseMetadataQuery *qry,
                                         G_GNUC_UNUSED MooseMetadata *self) {
    moose_metadata_query_commit(qry);
    return qry;
}

static void moose_metadata_on_deliver(G_GNUC_UNUSED MooseThreads *threads, gpointer data,
                                      MooseMetadata *self) {
    MooseMetadataQuery *qry = MOOSE_METADATA_QUERY(data);
    MooseMetadataPrivate *priv = moose_metadata_get_instance_private(self);

    g_mutex_lock(&priv->query_mtx);
    { g_queue_remove(priv->running_queries, qry); }
    g_mutex_unlock(&priv->query_mtx);

    g_signal_emit(self, SIGNALS[SIGNAL_QUERY_DONE], 0, qry);
    g_object_unref(qry);
}

static void moose_metadata_init(MooseMetadata *self) {
    MooseMetadataPrivate *priv = moose_metadata_get_instance_private(self);

    priv->pool = moose_threads_new(10);
    g_mutex_init(&priv->query_mtx);
    priv->running_queries = g_queue_new();

    g_signal_connect(priv->pool, "thread", G_CALLBACK(moose_metadata_on_thread), self);
    g_signal_connect(priv->pool, "deliver", G_CALLBACK(moose_metadata_on_deliver), self);
}

////////////////////////

void moose_metadata_commit(MooseMetadata *self, MooseMetadataQuery *qry) {
    g_assert(self);
    g_return_if_fail(qry);

    MooseMetadataPrivate *priv = moose_metadata_get_instance_private(self);
    g_mutex_lock(&priv->query_mtx);
    { g_queue_push_tail(priv->running_queries, qry); }
    g_mutex_unlock(&priv->query_mtx);

    g_object_ref(qry);

    GlyrQuery *internal_query = moose_metadata_query_get_internal(qry);
    if(priv->database) {
        glyr_opt_lookup_db(internal_query, priv->database);
        glyr_opt_db_autowrite(internal_query, TRUE);
    }
    glyr_opt_dlcallback(internal_query, moose_metadata_on_item_received, self);
    moose_threads_push(priv->pool, qry);
}

void moose_metadata_cancel(MooseMetadata *self) {
    g_assert(self);

    MooseMetadataPrivate *priv = moose_metadata_get_instance_private(self);

    /* Delegate the cancel forth */
    for(GList *iter = priv->running_queries->head; iter; iter = iter->next) {
        MooseMetadataQuery *query = iter->data;
        moose_metadata_query_cancel(query);
    }
}

void moose_metadata_unref(MooseMetadata *self) {
    g_assert(self);

    moose_metadata_cancel(self);
    g_object_unref(self);
}
