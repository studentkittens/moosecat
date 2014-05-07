#include "moose-misc-zeroconf.h"

#include <glib.h>
#include <glib/gprintf.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>


/* What type of services to browse */
#define MPD_AVAHI_SERVICE_TYPE "_mpd._tcp"

typedef struct _MooseZeroconfBrowserPrivate {
    AvahiClient * client;
    AvahiGLibPoll * poll;
    GList * server_list;

    MooseZeroconfState state;
    char * last_error;
} MooseZeroconfBrowserPrivate;

typedef struct _MooseZeroconfServerPrivate {
    char * name; 
    char * type;
    char * domain;
    char * host;
    char * addr;
    unsigned port;
} MooseZeroconfServerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
    MooseZeroconfBrowser, moose_zeroconf_browser, G_TYPE_OBJECT
);
G_DEFINE_TYPE_WITH_PRIVATE(
    MooseZeroconfServer, moose_zeroconf_server, G_TYPE_OBJECT
);

enum {
    BROWSER_SIGNAL_STATE_CHANGED,
    BROWSER_NUM_SIGNALS
};

static guint BROWSER_SIGNALS[BROWSER_NUM_SIGNALS];

////////////////////////////////

static void moose_zeroconf_call_user_callback(MooseZeroconfBrowser * self, MooseZeroconfState state) 
{
    g_assert(self);

    self->priv->state = state;
    g_signal_emit(self, BROWSER_SIGNALS[BROWSER_SIGNAL_STATE_CHANGED], 0);
}

////////////////////////////////

static void moose_zeroconf_drop_server(
        MooseZeroconfBrowser * self,
        const char * name,
        const char * type,
        const char * domain) {
    for(GList * iter = self->priv->server_list; iter; iter = iter->next) {
        MooseZeroconfServer * server = iter->data;
        if(server != NULL) {
            if(1
               && g_strcmp0(server->priv->name, name)     == 0
               && g_strcmp0(server->priv->type, type)     == 0
               && g_strcmp0(server->priv->domain, domain) == 0
            ) {
                self->priv->server_list = g_list_delete_link(self->priv->server_list, iter);
                g_object_unref(server);
                moose_zeroconf_call_user_callback(self, MOOSE_ZEROCONF_STATE_CHANGED);
                break;
            }
        }
    }
}

////////////////////////////////

static void moose_zeroconf_check_client_error(MooseZeroconfBrowser * self, const gchar * prefix_message)
{
    /* Print just a message for now */
    if(avahi_client_errno(self->priv->client) != AVAHI_OK)
    {
        g_free(self->priv->last_error);
        self->priv->last_error = g_strdup_printf("%s: %s", 
                prefix_message, avahi_strerror(avahi_client_errno(self->priv->client))
        );
        moose_zeroconf_call_user_callback(self, MOOSE_ZEROCONF_STATE_ERROR);
    }
}

////////////////////////////////

/* Called whenever a service has been resolved successfully or timed out */
static void moose_zeroconf_resolve_callback(
    AvahiServiceResolver * resolver,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AVAHI_GCC_UNUSED AvahiStringList *txt,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void * user_data)
{
    g_return_if_fail(resolver != NULL);

    MooseZeroconfBrowser * self = user_data;
        
    /* Check if we had been successful */
    switch(event) {
        case AVAHI_RESOLVER_FAILURE: {
            gchar * format = g_strdup_printf("Failed to resolve service '%s' of type '%s' in domain '%s'", name, type, domain);
            if(format != NULL) {
                moose_zeroconf_check_client_error(self, format);
                g_free(format);
            }
            break;
        }
        case AVAHI_RESOLVER_FOUND: {
            char addr_buf[AVAHI_ADDRESS_STR_MAX] = {0,};
            avahi_address_snprint(addr_buf, AVAHI_ADDRESS_STR_MAX, address);

            MooseZeroconfServer * server = g_object_new(MOOSE_TYPE_ZEROCONF_SERVER, NULL);
            g_object_set(server,
                "name", name,
                "type", type, 
                "domain", domain,
                "host",  host_name,
                "addr", addr_buf,
                "port", port,
                NULL
            );

            self->priv->server_list = g_list_prepend(self->priv->server_list, server);
            moose_zeroconf_call_user_callback(self, MOOSE_ZEROCONF_STATE_CHANGED);
        }
    }
    avahi_service_resolver_free(resolver);
}

////////////////////////////////

/* SERVICE BROWSER CALLBACK */
static void moose_zeroconf_service_browser_callback(
        AvahiServiceBrowser *browser,
        AvahiIfIndex interface,
        AvahiProtocol protocol,
        AvahiBrowserEvent event,
        const char *name,
        const char *type,
        const char *domain,
        AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
        void * user_data)
{
    MooseZeroconfBrowser * self = user_data;

    /* Necessary since this may block till client is fully connected */
    AvahiClient * full_client = avahi_service_browser_get_client(browser);

    /* Check event, comments from the avahi documentation. */
    switch(event) {
        /* The object is new on the network */
        case AVAHI_BROWSER_NEW: {
            if(avahi_service_resolver_new(
                full_client, interface, protocol, name, type, domain,
                (AvahiLookupFlags)AVAHI_PROTO_INET, (AvahiLookupFlags)0,
                moose_zeroconf_resolve_callback, self) 
                == NULL)
            {
                gchar * format = g_strdup_printf("Failed to resolve service '%s'",name);
                if(format != NULL) {
                    moose_zeroconf_check_client_error(self, format);
                    g_free(format);
                }
            }
            break;
        }
        /* The object has been removed from the network.*/
        case AVAHI_BROWSER_REMOVE: {
            moose_zeroconf_drop_server(self, name, type, domain);
            break;
        }
        /* One-time event, to notify the user that all entries from the caches have been sent. */
        case AVAHI_BROWSER_CACHE_EXHAUSTED: {
            break;
        }
        /* One-time event, to notify the user that more records will probably not show up in the near future, i.e.
         * all cache entries have been read and all static servers been queried */
        case AVAHI_BROWSER_ALL_FOR_NOW: {
            moose_zeroconf_call_user_callback(self, MOOSE_ZEROCONF_STATE_ALL_FOR_NOW);
            break;
        }
        /* Browsing failed due to some reason which can be retrieved using avahi_server_errno()/avahi_client_errno() */
        case AVAHI_BROWSER_FAILURE: {
            moose_zeroconf_check_client_error(self, "browser-failure:");
            break;
        }
        default:
            break;
    }
}

////////////////////////////////

static void moose_zeroconf_register_browser(MooseZeroconfBrowser * self)
{
    g_assert(self);
    self->priv->state = MOOSE_ZEROCONF_STATE_CONNECTED;
    avahi_service_browser_new(
        self->priv->client,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        MPD_AVAHI_SERVICE_TYPE,
        avahi_client_get_domain_name(self->priv->client),
        (AvahiLookupFlags)0,
        moose_zeroconf_service_browser_callback,
        self
    );
}
////////////////////////////////

static void moose_zeroconf_client_callback(
        AVAHI_GCC_UNUSED AvahiClient *client,
        AvahiClientState state,
        void * user_data);

static void moose_zeroconf_create_server(MooseZeroconfBrowser * self) 
{
    if(self->priv->client != NULL) {
        avahi_client_free(self->priv->client);
    }

    self->priv->client = avahi_client_new(
        avahi_glib_poll_get(self->priv->poll),
        (AvahiClientFlags)AVAHI_CLIENT_NO_FAIL,
        moose_zeroconf_client_callback,
        self,
        NULL 
    );

    if(self->priv->client != NULL) {
        if(avahi_client_get_state(self->priv->client) == AVAHI_CLIENT_CONNECTING) {
            moose_zeroconf_check_client_error(self, "Initializing Avahi");
        }
    }
}

////////////////////////////////

static void moose_zeroconf_client_callback(
        AVAHI_GCC_UNUSED AvahiClient *client,
        AvahiClientState state,
        void * user_data)
{
    /* This is called during avahi_client_new,
     * so at the precise moment there is no client
     * set yet - be sure it's set from the passed one.
     */
    MooseZeroconfBrowser * self = user_data;
    self->priv->client = client;

    switch(state) {
        case AVAHI_CLIENT_S_RUNNING: {
            moose_zeroconf_register_browser(self);
            break;
        }
        case AVAHI_CLIENT_FAILURE: {
            moose_zeroconf_check_client_error(self, "Disconnected from the Avahi Daemon");
            self->priv->state = MOOSE_ZEROCONF_STATE_UNCONNECTED;
            moose_zeroconf_create_server(self);
            break;
        }
        default: {
            break;
        }
    }
}

////////////////////////////////
//         PUBLIC API         //
////////////////////////////////

static void moose_zeroconf_browser_finalize(GObject *gobject)
{
    MooseZeroconfBrowser *self = MOOSE_ZEROCONF_BROWSER(gobject);

    if (self == NULL) {
        return;
    }

    g_list_free_full(self->priv->server_list, (GDestroyNotify)g_object_unref);
    g_free(self->priv->last_error);

    if(self->priv->client != NULL) {
        avahi_client_free(self->priv->client);
    }
    if(self->priv->poll != NULL) {
        avahi_glib_poll_free(self->priv->poll);
    }

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(self))
    )->finalize(gobject);
}

///////////////////////////////

static void moose_zeroconf_browser_class_init(MooseZeroconfBrowserClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = moose_zeroconf_browser_finalize;

    BROWSER_SIGNALS[BROWSER_SIGNAL_STATE_CHANGED] = g_signal_newv("state-changed",
             G_TYPE_FROM_CLASS(klass),
             G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
             NULL /* closure */,
             NULL /* accumulator */,
             NULL /* accumulator data */,
             g_cclosure_marshal_VOID__VOID,
             G_TYPE_NONE /* return_type */,
             0     /* n_params */,
             NULL  /* param_types */
    );
}

///////////////////////////////

MooseZeroconfBrowser *moose_zeroconf_browser_new(void) 
{
    return g_object_new(MOOSE_TYPE_ZEROCONF_BROWSER, NULL);
}

///////////////////////////////

void moose_zeroconf_browser_destroy(MooseZeroconfBrowser *self)
{
    g_object_unref(G_OBJECT(self));
}

///////////////////////////////

static void moose_zeroconf_browser_init(MooseZeroconfBrowser *self)
{
    self->priv = moose_zeroconf_browser_get_instance_private(self);

    /* Optional: Tell avahi to use g_malloc and g_free */
    avahi_set_allocator(avahi_glib_allocator());

    self->priv->client = NULL;
    self->priv->poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
    self->priv->state = MOOSE_ZEROCONF_STATE_UNCONNECTED;
    self->priv->last_error = NULL;

    moose_zeroconf_create_server(self);
}

////////////////////////////////

MooseZeroconfState moose_zeroconf_browser_get_state(MooseZeroconfBrowser * self)
{
    g_assert(self); 

    return self->priv->state;
}

////////////////////////////////

const char * moose_zeroconf_browser_get_error(MooseZeroconfBrowser * self)
{
    g_assert(self); 

    return self->priv->last_error;
}

////////////////////////////////

GList *moose_zeroconf_browser_get_server_list(MooseZeroconfBrowser * self)
{
    g_assert(self);

    return g_list_copy_deep(self->priv->server_list, (GCopyFunc)g_object_ref, NULL);
}

//////////////////////////////////////////
/// MooseZeroconfServer Implementation ///
//////////////////////////////////////////

static void moose_zeroconf_server_finalize(GObject* gobject)
{
    MooseZeroconfServer *self = MOOSE_ZEROCONF_SERVER(gobject);

    if(self != NULL) {
        g_free(self->priv->name);
        g_free(self->priv->type);
        g_free(self->priv->domain);
        g_free(self->priv->host);
        g_free(self->priv->addr);
    }

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(self))
    )->finalize(gobject);
}

////////////////////////////////

enum
{
    SERVER_PROP_0,
    SERVER_PROP_NAME,
    SERVER_PROP_TYPE,
    SERVER_PROP_DOMAIN,
    SERVER_PROP_HOST,
    SERVER_PROP_ADDR,
    SERVER_PROP_PORT,
    SERVER_N_PROPS
};

////////////////////////////////

static void moose_zeroconf_server_get_property(
    GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
    MooseZeroconfServer *self = MOOSE_ZEROCONF_SERVER(object);

    switch (property_id) {
        case SERVER_PROP_NAME:
            g_value_set_string(value, self->priv->name);
            break;
        case SERVER_PROP_TYPE:
            g_value_set_string(value, self->priv->type);
            break;
        case SERVER_PROP_DOMAIN:
            g_value_set_string(value, self->priv->domain);
            break;
        case SERVER_PROP_HOST:
            g_value_set_string(value, self->priv->host);
            break;
        case SERVER_PROP_ADDR:
            g_value_set_string(value, self->priv->addr);
            break;
        case SERVER_PROP_PORT:
            g_value_set_int(value, self->priv->port);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

////////////////////////////////

static void moose_zeroconf_server_set_property(
    GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
    MooseZeroconfServer *self = MOOSE_ZEROCONF_SERVER(object);

    switch (property_id) {
        case SERVER_PROP_NAME:
            self->priv->name = g_value_dup_string(value);
            break;
        case SERVER_PROP_TYPE:
            self->priv->type = g_value_dup_string(value);
            break;
        case SERVER_PROP_DOMAIN:
            self->priv->domain = g_value_dup_string(value);
            break;
        case SERVER_PROP_HOST:
            self->priv->host = g_value_dup_string(value);
            break;
        case SERVER_PROP_ADDR:
            self->priv->addr = g_value_dup_string(value);
            break;
        case SERVER_PROP_PORT:
            self->priv->port = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

////////////////////////////////

static void moose_zeroconf_server_class_init(MooseZeroconfServerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = moose_zeroconf_server_finalize;
    gobject_class->get_property = moose_zeroconf_server_get_property;
    gobject_class->set_property = moose_zeroconf_server_set_property;

    GParamSpec *pspec;
    pspec = g_param_spec_string(
        "name",
        "Servername",
        "Name that gave the server itself",
        NULL, /* default value */
        G_PARAM_READWRITE
    );
    g_object_class_install_property(gobject_class, SERVER_PROP_NAME, pspec);

    pspec = g_param_spec_string(
        "type",
        "Servertype",
        "Type of the server",
        NULL, /* default value */
        G_PARAM_READWRITE
    );
    g_object_class_install_property(gobject_class, SERVER_PROP_TYPE, pspec);

    pspec = g_param_spec_string(
        "domain",
        "Serverdomain",
        "Domain that the server lives in",
        NULL, /* default value */
        G_PARAM_READWRITE
    );
    g_object_class_install_property(gobject_class, SERVER_PROP_DOMAIN, pspec);

    pspec = g_param_spec_string(
        "host",
        "Hostname",
        "Hostname of the server",
        NULL, /* default value */
        G_PARAM_READWRITE
    );
    g_object_class_install_property(gobject_class, SERVER_PROP_HOST, pspec);

    pspec = g_param_spec_string(
        "addr",
        "Serveraddress",
        "Physical address of the server",
        NULL, /* default value */
        G_PARAM_READWRITE
    );
    g_object_class_install_property(gobject_class, SERVER_PROP_ADDR, pspec);

    pspec = g_param_spec_int(
        "port",
        "Serverport",
        "Port of the server",
        0, 
        G_MAXINT,
        0, /* default value */
        G_PARAM_READWRITE
    );
    g_object_class_install_property(gobject_class, SERVER_PROP_PORT, pspec);
}

///////////////////////////////

static void moose_zeroconf_server_init(MooseZeroconfServer *self)
{
    self->priv = moose_zeroconf_server_get_instance_private(self);
}

////////////////////////////////
//         PUBLIC API         //
////////////////////////////////

char * moose_zeroconf_server_get_host(MooseZeroconfServer * server)
{
    g_return_val_if_fail(server, NULL);

    gchar *string = NULL;
    g_object_get(server, "host", &string, NULL);
    return string;
}

////////////////////////////////

char * moose_zeroconf_server_get_addr(MooseZeroconfServer * server)
{
    g_return_val_if_fail(server, NULL);

    gchar *string = NULL;
    g_object_get(server, "addr", &string, NULL);
    return string;
}

////////////////////////////////

char * moose_zeroconf_server_get_name(MooseZeroconfServer * server)
{
    g_return_val_if_fail(server, NULL);

    gchar *string = NULL;
    g_object_get(server, "name", &string, NULL);
    return string;
}

////////////////////////////////

char * moose_zeroconf_server_get_protocol(MooseZeroconfServer * server)
{
    g_return_val_if_fail(server, NULL);

    gchar *string = NULL;
    g_object_get(server, "type", &string, NULL);
    return string;
}

////////////////////////////////

char * moose_zeroconf_server_get_domain(MooseZeroconfServer * server)
{
    g_return_val_if_fail(server, NULL);

    gchar *string = NULL;
    g_object_get(server, "domain", &string, NULL);
    return string;
}

////////////////////////////////

int moose_zeroconf_server_get_port(MooseZeroconfServer * server)
{
    g_return_val_if_fail(server, NULL);

    int port;
    g_object_get(server, "port", &port, NULL);
    return port;
}

////////////////////////////////

MooseZeroconfServer ** moose_zeroconf_browser_get_server(MooseZeroconfBrowser * self)
{
    g_assert(self);

    MooseZeroconfServer ** buf = NULL;

    if(self->priv->server_list != NULL) {
        unsigned cursor = 0;
        buf = g_malloc0(sizeof(MooseZeroconfServer *) * (g_list_length(self->priv->server_list) + 1));
        for(GList * iter = self->priv->server_list; iter; iter = iter->next) {
            buf[cursor++] = iter->data;
            buf[cursor+0] = NULL;
        }
    }
    return buf;
}
