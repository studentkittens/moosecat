#include "moose-metadata-cache.h"
#include "../moose-config.h"

#include <glyr/glyr.h>

enum {
    PROP_POINTER = 1,
    PROP_OWNS_POINTER,
    PROP_DATA,
    PROP_SOURCE,
    PROP_PROVIDER,
    PROP_IS_IMAGE,
    PROP_CHECKSUM,
    N_PROPS
};

typedef struct _MooseMetadataCachePrivate {
    /* Actual metadata cache */
    GlyrMemCache *cache;

    /* Wether we own this instance */
    gboolean owns_pointer;
} MooseMetadataCachePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(MooseMetadataCache, moose_metadata_cache, G_TYPE_OBJECT);

static void moose_metadata_cache_finalize(GObject *gobject) {
    MooseMetadataCache *self = MOOSE_METADATA_CACHE(gobject);
    g_assert(self);

    MooseMetadataCachePrivate *priv = moose_metadata_cache_get_instance_private(self);

    if(priv->owns_pointer) {
        glyr_cache_free(priv->cache);
        priv->cache = NULL;
    }

    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

static void moose_metadata_cache_get_property(GObject *object,
                                              guint property_id,
                                              GValue *value,
                                              GParamSpec *pspec) {
    MooseMetadataCachePrivate *priv =
        moose_metadata_cache_get_instance_private(MOOSE_METADATA_CACHE(object));

    if(priv->cache == NULL) {
        moose_critical("cache pointer is none for MooseMetadataCache %p\n", priv->cache);
        return;
    }

    switch(property_id) {
    case PROP_DATA:
        g_value_take_boxed(value,
                           g_bytes_new_static(priv->cache->data, priv->cache->size));
        break;
    case PROP_SOURCE:
        g_value_set_string(value, priv->cache->dsrc);
        break;
    case PROP_PROVIDER:
        g_value_set_string(value, priv->cache->prov);
        break;
    case PROP_IS_IMAGE:
        g_value_set_boolean(value, priv->cache->is_image);
        break;
    case PROP_CHECKSUM:
        g_value_take_string(value, glyr_md5sum_to_string(priv->cache->md5sum));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_metadata_cache_set_property(GObject *object,
                                              guint property_id,
                                              const GValue *value,
                                              GParamSpec *pspec) {
    MooseMetadataCachePrivate *priv =
        moose_metadata_cache_get_instance_private(MOOSE_METADATA_CACHE(object));

    switch(property_id) {
    case PROP_POINTER:
        priv->cache = g_value_get_pointer(value);
        break;
    case PROP_OWNS_POINTER:
        priv->owns_pointer = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_metadata_cache_class_init(MooseMetadataCacheClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_metadata_cache_finalize;
    gobject_class->get_property = moose_metadata_cache_get_property;
    gobject_class->set_property = moose_metadata_cache_set_property;

    /**
     * MooseMetadataCache:pointer:
     *
     * Pointer to glyr's GlyrMemCache.
     */
    g_object_class_install_property(
        gobject_class, PROP_POINTER,
        g_param_spec_pointer("pointer", "pointer", "Internal datapointer",
                             G_PARAM_READWRITE));

    /**
     * MooseMetadataCache:owns-pointer: (type boolean)
     *
     * Wether we own the GlyrMemCache pointer or if we just a view on it.
     */
    g_object_class_install_property(
        gobject_class, PROP_OWNS_POINTER,
        g_param_spec_boolean("owns-pointer", "own pointer",
                             "Own pointer to GlyrMemCache?", TRUE, G_PARAM_READWRITE));

    /**
     * MooseMetadataCache:data: (type GBytes)
     *
     * The cache data as gbytes.
     */
    g_object_class_install_property(
        gobject_class, PROP_DATA, g_param_spec_boxed("data", "bytedata", "The cachedata",
                                                     G_TYPE_BYTES, G_PARAM_READABLE));

    /**
     * MooseMetadataCache:source-url: (type utf8)
     *
     * Source of the data.
     */
    g_object_class_install_property(
        gobject_class, PROP_SOURCE,
        g_param_spec_string("source-url", "source-url",
                            "Where to find the data in the web", NULL, G_PARAM_READABLE));

    /**
     * MooseMetadataCache:provider: (type utf8)
     *
     * Provider of the data.
     */
    g_object_class_install_property(
        gobject_class, PROP_PROVIDER,
        g_param_spec_string("provider", "Provider", "Which provder delivered the data?",
                            NULL, G_PARAM_READABLE));

    /**
     * MooseMetadataCache:is_image: (type boolean)
     *
     * Check if the item is an image and contains image data in the data
     * property.
     */
    g_object_class_install_property(
        gobject_class, PROP_IS_IMAGE,
        g_param_spec_boolean("is-image", "Is an image?",
                             "True fro covers, artistphoto etc.", FALSE,
                             G_PARAM_READABLE));

    /**
     * MooseMetadataCache:md5sum: (type utf8)
     *
     * Checksum of this item.
     */
    g_object_class_install_property(
        gobject_class, PROP_CHECKSUM,
        g_param_spec_string("checksum", "Checksum", "Checksum of the item (MD5)", NULL,
                            G_PARAM_READABLE));
}

static void moose_metadata_cache_init(MooseMetadataCache *self) {
    MooseMetadataCachePrivate *priv = moose_metadata_cache_get_instance_private(self);
    priv->cache = NULL;
}
