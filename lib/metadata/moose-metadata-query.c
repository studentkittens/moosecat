#include "moose-metadata-query.h"
#include "moose-metadata-cache.h"

#include <glyr/glyr.h>

enum {
    PROP_GET_TYPE = 1,
    PROP_ARTIST,
    PROP_ALBUM,
    PROP_TITLE,
    PROP_DOWNLOAD,
    PROP_NUMBER,
    PROP_PROVIDERS,
    PROP_MUSIC_PATH,
    N_PROPS
};

typedef struct _MooseMetadataQueryPrivate {
    GlyrQuery query;
    GList *result_caches;
} MooseMetadataQueryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(MooseMetadataQuery, moose_metadata_query, G_TYPE_OBJECT);

static void moose_metadata_query_finalize(GObject *gobject) {
    MooseMetadataQuery *self = MOOSE_METADATA_QUERY(gobject);
    g_assert(self);

    MooseMetadataQueryPrivate *priv = moose_metadata_query_get_instance_private(self);
    glyr_query_destroy(&priv->query);
    g_list_free_full(priv->result_caches, g_object_unref);

    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

static void moose_metadata_query_get_property(GObject *object,
                                              guint property_id,
                                              GValue *value,
                                              GParamSpec *pspec) {
    MooseMetadataQueryPrivate *priv =
        moose_metadata_query_get_instance_private(MOOSE_METADATA_QUERY(object));

    switch(property_id) {
    case PROP_GET_TYPE:
        g_value_set_string(value, glyr_get_type_to_string(priv->query.type));
        break;
    case PROP_ARTIST:
        g_value_set_string(value, priv->query.artist);
        break;
    case PROP_ALBUM:
        g_value_set_string(value, priv->query.album);
        break;
    case PROP_TITLE:
        g_value_set_string(value, priv->query.title);
        break;
    case PROP_DOWNLOAD:
        g_value_set_boolean(value, priv->query.download);
        break;
    case PROP_NUMBER:
        g_value_set_int(value, priv->query.number);
        break;
    case PROP_PROVIDERS:
        g_value_set_string(value, priv->query.from);
        break;
    case PROP_MUSIC_PATH:
        g_value_set_string(value, priv->query.musictree_path);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_metadata_query_set_property(GObject *object,
                                              guint property_id,
                                              const GValue *value,
                                              GParamSpec *pspec) {
    MooseMetadataQueryPrivate *priv =
        moose_metadata_query_get_instance_private(MOOSE_METADATA_QUERY(object));

    switch(property_id) {
    case PROP_GET_TYPE:
        glyr_opt_type(&priv->query, glyr_string_to_get_type(g_value_get_string(value)));
        break;
    case PROP_ARTIST:
        glyr_opt_artist(&priv->query, g_value_get_string(value));
        break;
    case PROP_ALBUM:
        glyr_opt_album(&priv->query, g_value_get_string(value));
        break;
    case PROP_TITLE:
        glyr_opt_title(&priv->query, g_value_get_string(value));
        break;
    case PROP_DOWNLOAD:
        glyr_opt_download(&priv->query, g_value_get_boolean(value));
        break;
    case PROP_NUMBER:
        glyr_opt_number(&priv->query, g_value_get_int(value));
        break;
    case PROP_PROVIDERS:
        glyr_opt_from(&priv->query, g_value_get_string(value));
        break;
    case PROP_MUSIC_PATH:
        glyr_opt_from(&priv->query, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_metadata_query_class_init(MooseMetadataQueryClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_metadata_query_finalize;
    gobject_class->set_property = moose_metadata_query_set_property;
    gobject_class->get_property = moose_metadata_query_get_property;

    /**
     * MooseMetadataQuery:type: (type utf8)
     *
     * The type to get.
     */
    g_object_class_install_property(
        gobject_class, PROP_GET_TYPE,
        g_param_spec_string("type", "getter type", "Kind of metadata to retrieve", NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    /**
     * MooseMetadataQuery:artist: (type utf8)
     *
     * The artist.
     */
    g_object_class_install_property(
        gobject_class, PROP_ARTIST,
        g_param_spec_string("artist", "Artistname", "The artistname", NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    /**
     * MooseMetadataQuery:album: (type utf8)
     *
     * The album.
     */
    g_object_class_install_property(
        gobject_class, PROP_ALBUM,
        g_param_spec_string("album", "Albumname", "The album", NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    /**
     * MooseMetadataQuery:title: (type utf8)
     *
     * The title.
     */
    g_object_class_install_property(
        gobject_class, PROP_TITLE,
        g_param_spec_string("title", "Songtitle", "The songtitle", NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    /**
     * MooseMetadataQuery:download: (type bool)
     *
     * Download the item?
     */
    g_object_class_install_property(
        gobject_class, PROP_DOWNLOAD,
        g_param_spec_boolean("download", "Download item", "Wether to download the item",
                             TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    /**
     * MooseMetadataQuery:number: (type int)
     *
     * Number of items to retrieve.
     */
    g_object_class_install_property(
        gobject_class, PROP_NUMBER,
        g_param_spec_int("number", "number", "How many items to retrieve", 0, 10e8, 1,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    /**
    * MooseMetadataQuery:providers: (type utf8)
    *
    * Comma separated list of providers to fetch data from.
    */
    g_object_class_install_property(
        gobject_class, PROP_PROVIDERS,
        g_param_spec_string("providers", "Providers", "Stringlist of providers", "all",
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    /**
    * MooseMetadataQuery:music-path: (type utf8)
    *
    * Path to the root of the music database (as given in the mpd.conf)
    */
    g_object_class_install_property(
        gobject_class, PROP_MUSIC_PATH,
        g_param_spec_string("music-path", "Musicpath", "Path to music database", NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void moose_metadata_query_init(MooseMetadataQuery *self) {
    MooseMetadataQueryPrivate *priv = moose_metadata_query_get_instance_private(self);
    glyr_query_init(&priv->query);
    glyr_opt_qsratio(&priv->query, 1.0);
    glyr_opt_parallel(&priv->query, 4);
    glyr_opt_timeout(&priv->query, 5.0);
    glyr_opt_useragent(&priv->query,
                       "Moose/0.0.1 +(https://github.com/studentkittens/moosecat)");
}

/////////////////

void moose_metadata_query_commit(MooseMetadataQuery *self) {
    g_assert(self);

    MooseMetadataQueryPrivate *priv = moose_metadata_query_get_instance_private(self);

    int n_caches = 0;

    GlyrMemCache *result = glyr_get(&priv->query, NULL, &n_caches);
    for(GlyrMemCache *iter = result; iter; iter = iter->next) {
        MooseMetadataCache *cache =
            g_object_new(moose_metadata_cache_get_type(), "pointer", iter, NULL);
        priv->result_caches = g_list_prepend(priv->result_caches, cache);
    }
}

void moose_metadata_query_cancel(MooseMetadataQuery *self) {
    g_assert(self);

    MooseMetadataQueryPrivate *priv = moose_metadata_query_get_instance_private(self);
    glyr_signal_exit(&priv->query);
}

GList *moose_metadata_query_get_results(MooseMetadataQuery *self) {
    g_assert(self);

    MooseMetadataQueryPrivate *priv = moose_metadata_query_get_instance_private(self);
    return priv->result_caches;
}

const char *moose_metadata_query_get_error(MooseMetadataQuery *self) {
    g_assert(self);

    MooseMetadataQueryPrivate *priv = moose_metadata_query_get_instance_private(self);
    if(priv->query.q_errno == GLYRE_UNKNOWN) {
        return NULL;
    } else {
        return glyr_strerror(priv->query.q_errno);
    }
}

void moose_metadata_query_unref(MooseMetadataQuery *self) {
    g_assert(self);
    g_object_unref(self);
}

GlyrQuery *moose_metadata_query_get_internal(MooseMetadataQuery *self) {
    g_assert(self);

    MooseMetadataQueryPrivate *priv = moose_metadata_query_get_instance_private(self);
    return &priv->query;
}

void moose_metadata_query_enable_debug(MooseMetadataQuery *self) {
    g_assert(self);

    MooseMetadataQueryPrivate *priv = moose_metadata_query_get_instance_private(self);
    glyr_opt_verbosity(&priv->query, 4);
}

gboolean moose_metadata_query_equal(MooseMetadataQuery *self, MooseMetadataQuery *other) {
    g_assert(self);

    MooseMetadataQueryPrivate *priv_a = moose_metadata_query_get_instance_private(self);
    MooseMetadataQueryPrivate *priv_b = moose_metadata_query_get_instance_private(other);

    if(self == other) {
        return TRUE;
    }

    GlyrQuery *qry_a = &priv_a->query, *qry_b = &priv_b->query;
    GLYR_FIELD_REQUIREMENT reqs = glyr_get_requirements(qry_a->type);

    /* Different get types will also yield to different reqs */
    if(reqs != glyr_get_requirements(qry_b->type)) {
        return FALSE;
    }

    if(qry_a->number != qry_b->number || qry_a->download != qry_b->download) {
        return FALSE;
    }

    return (!(reqs & GLYR_REQUIRES_ARTIST) ||
            g_ascii_strcasecmp(qry_a->artist, qry_b->artist) == 0) &&
           (!(reqs & GLYR_REQUIRES_ALBUM) ||
            g_ascii_strcasecmp(qry_a->album, qry_b->album) == 0) &&
           (!(reqs & GLYR_REQUIRES_TITLE) ||
            g_ascii_strcasecmp(qry_a->title, qry_b->title) == 0);
}
