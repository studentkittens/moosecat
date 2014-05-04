#include "zeroconf.h"

#include <glib.h>
#include <glib/gprintf.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>


/* What type of services to browse */
#define MPD_AVAHI_SERVICE_TYPE "_mpd._tcp"

typedef struct MooseZeroconfBrowser {
    AvahiClient * client;
    AvahiGLibPoll * poll;
    GList * server_list;

    MooseZeroconfCallback callback;
    void * callback_data;

    MooseZeroconfState state;
    char * last_error;

    char * protocol;
} MooseZeroconfBrowser;

////////////////////////////////

typedef struct MooseZeroconfServer {
    char * name; 
    char * type;
    char * domain;

    char * host;
    char * addr;
    unsigned port;
} MooseZeroconfServer;

////////////////////////////////

static void moose_zeroconf_free_server(MooseZeroconfServer * server)
{
    if(server != NULL) {
        g_free(server->name);
        g_free(server->type);
        g_free(server->domain);
        g_free(server->host);
        g_free(server->addr);

        g_slice_free(MooseZeroconfServer, server);
    }
}

////////////////////////////////

static void moose_zeroconf_call_user_callback(MooseZeroconfBrowser * self, MooseZeroconfState state) 
{
    g_assert(self);

    self->state = state;
    if(self->callback) {
        self->callback(self, self->callback_data);
    }
}

////////////////////////////////

static void moose_zeroconf_drop_server(
        MooseZeroconfBrowser * self,
        const char * name,
        const char * type,
        const char * domain) {
    for(GList * iter = self->server_list; iter; iter = iter->next) {
        MooseZeroconfServer * server = iter->data;
        if(server != NULL) {
            if(1
               && g_strcmp0(server->name, name)     == 0
               && g_strcmp0(server->type, type)     == 0
               && g_strcmp0(server->domain, domain) == 0
            ) {
                self->server_list = g_list_delete_link(self->server_list, iter);
                moose_zeroconf_free_server(server);
                moose_zeroconf_call_user_callback(self, MC_ZEROCONF_STATE_CHANGED);
                break;
            }
        }
    }
}

////////////////////////////////

static void moose_zeroconf_check_client_error(MooseZeroconfBrowser * self, const gchar * prefix_message)
{
    /* Print just a message for now */
    if(avahi_client_errno(self->client) != AVAHI_OK)
    {
        g_free(self->last_error);
        self->last_error = g_strdup_printf("%s: %s", 
                prefix_message, avahi_strerror(avahi_client_errno(self->client))
        );
        moose_zeroconf_call_user_callback(self, MC_ZEROCONF_STATE_ERROR);
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
            MooseZeroconfServer * server = g_slice_new0(MooseZeroconfServer);
            server->port = port;
            server->name = g_strdup(name);
            server->type = g_strdup(type);
            server->domain = g_strdup(domain);
            server->host = g_strdup(host_name);
            server->addr = g_malloc0(AVAHI_ADDRESS_STR_MAX);;
            avahi_address_snprint(server->addr, AVAHI_ADDRESS_STR_MAX, address);

            self->server_list = g_list_prepend(self->server_list, server);

            moose_zeroconf_call_user_callback(self, MC_ZEROCONF_STATE_CHANGED);
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
            moose_zeroconf_call_user_callback(self, MC_ZEROCONF_STATE_ALL_FOR_NOW);
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
    self->state = MC_ZEROCONF_STATE_CONNECTED;
    avahi_service_browser_new(
            self->client,
            AVAHI_IF_UNSPEC,
            AVAHI_PROTO_UNSPEC,
            self->protocol,
            avahi_client_get_domain_name(self->client),
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
    if(self->client != NULL) {
        avahi_client_free(self->client);
    }

    self->client = avahi_client_new(
            avahi_glib_poll_get(self->poll),
            (AvahiClientFlags)AVAHI_CLIENT_NO_FAIL,
            moose_zeroconf_client_callback,
            self,
            NULL 
    );

    if(self->client != NULL) {
        if(avahi_client_get_state(self->client) == AVAHI_CLIENT_CONNECTING) {
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
    self->client = client;

    switch(state) {
        case AVAHI_CLIENT_S_RUNNING: {
            moose_zeroconf_register_browser(self);
            break;
        }
        case AVAHI_CLIENT_FAILURE: {
            moose_zeroconf_check_client_error(self, "Disconnected from the Avahi Daemon");
            self->state = MC_ZEROCONF_STATE_UNCONNECTED;
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

struct MooseZeroconfBrowser * moose_zeroconf_new(const char * protocol)
{
    /* Optional: Tell avahi to use g_malloc and g_free */
    avahi_set_allocator(avahi_glib_allocator());
    MooseZeroconfBrowser * self = g_slice_new0(MooseZeroconfBrowser);

    self->state = MC_ZEROCONF_STATE_UNCONNECTED;
    self->poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
    self->protocol = g_strdup((protocol) ? protocol : MPD_AVAHI_SERVICE_TYPE);

    moose_zeroconf_create_server(self);
    return self;
}

////////////////////////////////

void moose_zeroconf_destroy(struct MooseZeroconfBrowser * self)
{
    g_assert(self);

    g_list_free_full(self->server_list, (GDestroyNotify)moose_zeroconf_free_server);
    g_free(self->last_error);
    g_free(self->protocol);

    if(self->client != NULL) {
        avahi_client_free(self->client);
    }
    if(self->poll != NULL) {
        avahi_glib_poll_free(self->poll);
    }

    g_slice_free(MooseZeroconfBrowser, self);
}

////////////////////////////////

void moose_zeroconf_register(MooseZeroconfBrowser * self, MooseZeroconfCallback callback, void * user_data)
{
    g_assert(self);

    self->callback = callback;
    self->callback_data = user_data;
}

////////////////////////////////

MooseZeroconfState moose_zeroconf_get_state(MooseZeroconfBrowser * self)
{
    g_assert(self); 

    return self->state;
}

////////////////////////////////

const char * moose_zeroconf_get_error(MooseZeroconfBrowser * self)
{
    g_assert(self); 

    return self->last_error;
}

////////////////////////////////

MooseZeroconfServer ** moose_zeroconf_get_server(MooseZeroconfBrowser * self)
{
    g_assert(self);

    MooseZeroconfServer ** buf = NULL;

    if(self->server_list != NULL) {
        unsigned cursor = 0;
        buf = g_malloc0(sizeof(MooseZeroconfServer *) * (g_list_length(self->server_list) + 1));
        for(GList * iter = self->server_list; iter; iter = iter->next) {
            buf[cursor++] = iter->data;
            buf[cursor+0] = NULL;
        }
    }
    return buf;
}

////////////////////////////////

const char * moose_zeroconf_server_get_host(MooseZeroconfServer * server)
{
    g_assert(server);

    return server->host;
}

////////////////////////////////

const char * moose_zeroconf_server_get_addr(MooseZeroconfServer * server)
{
    g_assert(server);

    return server->addr;
}

////////////////////////////////

const char * moose_zeroconf_server_get_name(MooseZeroconfServer * server)
{
    g_assert(server);

    return server->name;
}

////////////////////////////////

const char * moose_zeroconf_server_get_type(MooseZeroconfServer * server)
{
    g_assert(server);

    return server->type;
}

////////////////////////////////

const char * moose_zeroconf_server_get_domain(MooseZeroconfServer * server)
{
    g_assert(server);

    return server->domain;
}

////////////////////////////////

unsigned moose_zeroconf_server_get_port(MooseZeroconfServer * server)
{
    g_assert(server);

    return server->port;
}
