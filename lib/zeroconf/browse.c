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

void mc_zeroconf_free_server(mc_ZeroconfServer * server)
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

void mc_zeroconf_drop_server(
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
                g_printerr("Deleting %s %s %s\n", name, type, domain);
                self->server_list = g_list_delete_link(self->server_list, iter);
                mc_zeroconf_free_server(server);
                break;
            }
        }
    }
}

////////////////////////////////

void mc_zeroconf_check_client_error(mc_ZeroconfBrowser * self, const gchar * prefix_message)
{
    /* Print just a message for now */
    if(avahi_client_errno(self->client) != AVAHI_OK)
    {
        const gchar * error_message = avahi_strerror(avahi_client_errno(self->client));
        g_printerr("%s: %s\n",prefix_message,error_message);
        //m_signal_error_message.emit(Glib::ustring(prefix_message) + error_message);
    }
}

////////////////////////////////

void mc_zeroconf_client_callback(
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
            char * addr = g_malloc0(AVAHI_ADDRESS_STR_MAX);
            avahi_address_snprint(addr, sizeof(addr), address);
            g_printerr("=> '%s' of type '%s' in domain '%s':\n", name, type, domain);
            g_printerr("=> %s:%u (%s)\n",host_name, port, addr);

            mc_ZeroconfServer * server = g_slice_new0(mc_ZeroconfServer);
            server->name = g_strdup(name);
            server->type = g_strdup(type);
            server->domain = g_strdup(domain);

            server->addr = addr;
            server->port = port;
            server->host = g_strdup(host_name);

            self->server_list = g_list_prepend(self->server_list, server);
        }
    }
    avahi_service_resolver_free(resolver);
}

////////////////////////////////

/* SERVICE BROWSER CALLBACK */
void mc_zeroconf_service_browser_callback(
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
            g_printerr("-- NEW: %s %s %s\n",name,type,domain);
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
            g_printerr("-- DEL: %s %s %s\n", name, type, domain);
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
            // TODO: Notify user.
            break;
        }
        /* Browsing failed due to some reason which can be retrieved using avahi_server_errno()/avahi_client_errno() */
        case AVAHI_BROWSER_FAILURE: {
            // TODO: Notify user.
            break;
        }
        default:
            break;
    }
}

////////////////////////////////

struct mc_ZeroconfBrowser * mc_zeroconf_new(void)
{
    int error = 0;

    /* Optional: Tell avahi to use g_malloc and g_free */
    avahi_set_allocator(avahi_glib_allocator());

    mc_ZeroconfBrowser * self = g_slice_new0(mc_ZeroconfBrowser);

    self->poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
    self->client = avahi_client_new(
            avahi_glib_poll_get(self->poll),
            (AvahiClientFlags)AVAHI_CLIENT_NO_FAIL,
            mc_zeroconf_client_callback,
            self,
            &error
    );

    if(self->client != NULL && avahi_client_get_state(self->client) != AVAHI_CLIENT_CONNECTING) {
        avahi_service_browser_new(
                self->client,
                AVAHI_IF_UNSPEC,
                AVAHI_PROTO_UNSPEC,
                MPD_AVAHI_SERVICE_TYPE,
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

    for(GList * iter = self->server_list; iter; iter = iter->next) {
        mc_zeroconf_free_server(iter->data);
    }

    if(self->client != NULL) {
        avahi_client_free(self->client);
    }
    if(self->poll != NULL) {
        avahi_glib_poll_free(self->poll);
    }
}
