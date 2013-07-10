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

typedef struct mc_ZeroconfBrowser {
    AvahiClient * client;
    AvahiGLibPoll * poll;
    GList * server_list;

    mc_ZeroconfCallback callback;
    void * callback_data;

    mc_ZeroconfState state;
    char * last_error;

} mc_ZeroconfBrowser;

////////////////////////////////

typedef struct mc_ZeroconfServer {
    char * name; 
    char * type;
    char * domain;

    char * host;
    char * addr;
    unsigned port;
} mc_ZeroconfServer;

////////////////////////////////

static void mc_zeroconf_free_server(mc_ZeroconfServer * server)
{
    if(server != NULL) {
        g_free(server->name);
        g_free(server->type);
        g_free(server->domain);
        g_free(server->host);
        g_free(server->addr);

        g_slice_free(mc_ZeroconfServer, server);
    }
}

////////////////////////////////

static void mc_zeroconf_call_user_callback(mc_ZeroconfBrowser * self, mc_ZeroconfState state) 
{
    g_assert(self);

    self->state = state;
    if(self->callback) {
        self->callback(self, self->callback_data);
    }
}

////////////////////////////////

static void mc_zeroconf_drop_server(
        mc_ZeroconfBrowser * self,
        const char * name,
        const char * type,
        const char * domain) {
    for(GList * iter = self->server_list; iter; iter = iter->next) {
        mc_ZeroconfServer * server = iter->data;
        if(server != NULL) {
            if(1
               && g_strcmp0(server->name, name)     == 0
               && g_strcmp0(server->type, type)     == 0
               && g_strcmp0(server->domain, domain) == 0
            ) {
                self->server_list = g_list_delete_link(self->server_list, iter);
                mc_zeroconf_free_server(server);
                mc_zeroconf_call_user_callback(self, MC_ZEROCONF_STATE_CHANGED);
                break;
            }
        }
    }
}

////////////////////////////////

static void mc_zeroconf_check_client_error(mc_ZeroconfBrowser * self, const gchar * prefix_message)
{
    /* Print just a message for now */
    if(avahi_client_errno(self->client) != AVAHI_OK)
    {
        g_free(self->last_error);
        self->last_error = g_strdup_printf("%s: %s", 
                prefix_message, avahi_strerror(avahi_client_errno(self->client))
        );
        mc_zeroconf_call_user_callback(self, MC_ZEROCONF_STATE_ERROR);
    }
}

////////////////////////////////

static void mc_zeroconf_client_callback(
        AVAHI_GCC_UNUSED AvahiClient *client,
        AvahiClientState state,
        void * user_data)
{
    if(state == AVAHI_CLIENT_FAILURE)
    {
        mc_ZeroconfBrowser * self = user_data;
        mc_zeroconf_check_client_error(self, "Disconnected from the Avahi Daemon");
    }
}

////////////////////////////////

/* Called whenever a service has been resolved successfully or timed out */
static void mc_zeroconf_resolve_callback(
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

    mc_ZeroconfBrowser * self = user_data;
        
    /* Check if we had been successful */
    switch(event) {
        case AVAHI_RESOLVER_FAILURE: {
            gchar * format = g_strdup_printf("Failed to resolve service '%s' of type '%s' in domain '%s'", name, type, domain);
            if(format != NULL) {
                mc_zeroconf_check_client_error(self, format);
                g_free(format);
            }
            break;
        }
        case AVAHI_RESOLVER_FOUND: {
            mc_ZeroconfServer * server = g_slice_new0(mc_ZeroconfServer);
            server->port = port;
            server->name = g_strdup(name);
            server->type = g_strdup(type);
            server->domain = g_strdup(domain);
            server->host = g_strdup(host_name);
            server->addr = g_malloc0(AVAHI_ADDRESS_STR_MAX);;
            avahi_address_snprint(server->addr, AVAHI_ADDRESS_STR_MAX, address);

            self->server_list = g_list_prepend(self->server_list, server);

            mc_zeroconf_call_user_callback(self, MC_ZEROCONF_STATE_CHANGED);
        }
    }
    avahi_service_resolver_free(resolver);
}

////////////////////////////////

/* SERVICE BROWSER CALLBACK */
static void mc_zeroconf_service_browser_callback(
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
    mc_ZeroconfBrowser * self = user_data;

    /* Necessary since this may block till client is fully connected */
    AvahiClient * full_client = avahi_service_browser_get_client(browser);

    /* Check event, comments from the avahi documentation. */
    switch(event) {
        /* The object is new on the network */
        case AVAHI_BROWSER_NEW: {
            if(avahi_service_resolver_new(
                        full_client, interface, protocol, name, type, domain,
                        (AvahiLookupFlags)AVAHI_PROTO_INET, (AvahiLookupFlags)0,
                        mc_zeroconf_resolve_callback, self) 
                == NULL)
            {
                gchar * format = g_strdup_printf("Failed to resolve service '%s'",name);
                if(format != NULL) {
                    mc_zeroconf_check_client_error(self, format);
                    g_free(format);
                }
            }
            break;
        }
        /* The object has been removed from the network.*/
        case AVAHI_BROWSER_REMOVE: {
            mc_zeroconf_drop_server(self, name, type, domain);
            break;
        }
        /* One-time event, to notify the user that all entries from the caches have been sent. */
        case AVAHI_BROWSER_CACHE_EXHAUSTED: {
            break;
        }
        /* One-time event, to notify the user that more records will probably not show up in the near future, i.e.
         * all cache entries have been read and all static servers been queried */
        case AVAHI_BROWSER_ALL_FOR_NOW: {
            mc_zeroconf_call_user_callback(self, MC_ZEROCONF_STATE_ALL_FOR_NOW);
            break;
        }
        /* Browsing failed due to some reason which can be retrieved using avahi_server_errno()/avahi_client_errno() */
        case AVAHI_BROWSER_FAILURE: {
            mc_zeroconf_check_client_error(self, "browser-failure:");
            break;
        }
        default:
            break;
    }
}

////////////////////////////////
//         PUBLIC API         //
////////////////////////////////

struct mc_ZeroconfBrowser * mc_zeroconf_new(const char * protocol)
{
    int error = 0;

    /* Optional: Tell avahi to use g_malloc and g_free */
    avahi_set_allocator(avahi_glib_allocator());
    mc_ZeroconfBrowser * self = g_slice_new0(mc_ZeroconfBrowser);

    self->state = MC_ZEROCONF_STATE_UNCONNECTED;
    self->poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
    self->client = avahi_client_new(
            avahi_glib_poll_get(self->poll),
            (AvahiClientFlags)AVAHI_CLIENT_NO_FAIL,
            mc_zeroconf_client_callback,
            self,
            &error
    );

    if(self->client != NULL && avahi_client_get_state(self->client) != AVAHI_CLIENT_CONNECTING) {
        self->state = MC_ZEROCONF_STATE_CONNECTED;
        avahi_service_browser_new(
                self->client,
                AVAHI_IF_UNSPEC,
                AVAHI_PROTO_UNSPEC,
                (protocol == NULL) ? MPD_AVAHI_SERVICE_TYPE : protocol,
                avahi_client_get_domain_name(self->client),
                (AvahiLookupFlags)0,
                mc_zeroconf_service_browser_callback,
                self
        );
    } else if(self->client != NULL) {
        mc_zeroconf_check_client_error(self, "Initializing Avahi");
    }

    return self;
}

////////////////////////////////

void mc_zeroconf_destroy(struct mc_ZeroconfBrowser * self)
{
    g_assert(self);

    g_list_free_full(self->server_list, (GDestroyNotify)mc_zeroconf_free_server);

    if(self->last_error != NULL) {
        g_free(self->last_error);
    }

    if(self->client != NULL) {
        avahi_client_free(self->client);
    }
    if(self->poll != NULL) {
        avahi_glib_poll_free(self->poll);
    }

    g_slice_free(mc_ZeroconfBrowser, self);
}

////////////////////////////////

void mc_zeroconf_register(mc_ZeroconfBrowser * self, mc_ZeroconfCallback callback, void * user_data)
{
    g_assert(self);

    self->callback = callback;
    self->callback_data = user_data;
}

////////////////////////////////

mc_ZeroconfState mc_zeroconf_get_state(mc_ZeroconfBrowser * self)
{
    g_assert(self); 

    return self->state;
}

////////////////////////////////

const char * mc_zeroconf_get_error(mc_ZeroconfBrowser * self)
{
    g_assert(self); 

    return self->last_error;
}

////////////////////////////////

mc_ZeroconfServer ** mc_zeroconf_get_server(mc_ZeroconfBrowser * self)
{
    g_assert(self);

    mc_ZeroconfServer ** buf = NULL;

    if(self->server_list != NULL) {
        unsigned cursor = 0;
        buf = g_malloc0(sizeof(mc_ZeroconfServer *) * (g_list_length(self->server_list) + 1));
        for(GList * iter = self->server_list; iter; iter = iter->next) {
            buf[cursor++] = iter->data;
            buf[cursor+0] = NULL;
        }
    }
    return buf;
}

////////////////////////////////

const char * mc_zeroconf_server_get_host(mc_ZeroconfServer * server)
{
    g_assert(server);

    return server->host;
}

////////////////////////////////

const char * mc_zeroconf_server_get_addr(mc_ZeroconfServer * server)
{
    g_assert(server);

    return server->addr;
}

////////////////////////////////

const char * mc_zeroconf_server_get_name(mc_ZeroconfServer * server)
{
    g_assert(server);

    return server->name;
}

////////////////////////////////

const char * mc_zeroconf_server_get_type(mc_ZeroconfServer * server)
{
    g_assert(server);

    return server->type;
}

////////////////////////////////

const char * mc_zeroconf_server_get_domain(mc_ZeroconfServer * server)
{
    g_assert(server);

    return server->domain;
}

////////////////////////////////

unsigned mc_zeroconf_server_get_port(mc_ZeroconfServer * server)
{
    g_assert(server);

    return server->port;
}
