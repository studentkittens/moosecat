#ifndef MOOSE_ZEROCONF_BROWSE_HH
#define MOOSE_ZEROCONF_BROWSE_HH

/**
 * SECTION: moose-misc-zeroconf
 * @short_description: Find other MPD-Servers via Zeroconf
 *
 * Zeroconf (with the Avahi Implementation under Linux) can be used to find other
 * MPD-Servers on the net, if they have this in their config:
 *
 *   # Avahi
 *   zeroconf_enabled "yes"
 *   zeroconf_name    "some_name_given_like_kitchen_box"
 */

#include <glib-object.h>

G_BEGIN_DECLS

/*
 * Type macros.
 */
#define MOOSE_TYPE_ZEROCONF_BROWSER \
    (moose_zeroconf_browser_get_type())
#define MOOSE_ZEROCONF_BROWSER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_ZEROCONF_BROWSER, MooseZeroconfBrowser))
#define MOOSE_IS_ZEROCONF_BROWSER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_ZEROCONF_BROWSER))
#define MOOSE_ZEROCONF_BROWSER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_ZEROCONF_BROWSER, MooseZeroconfBrowserClass))
#define MOOSE_IS_ZEROCONF_BROWSER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_ZEROCONF_BROWSER))
#define MOOSE_ZEROCONF_BROWSER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_ZEROCONF_BROWSER, MooseZeroconfBrowserClass))

GType moose_zeroconf_browser_get_type(void);

#define MOOSE_TYPE_ZEROCONF_SERVER \
    (moose_zeroconf_server_get_type())
#define MOOSE_ZEROCONF_SERVER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_ZEROCONF_SERVER, MooseZeroconfServer))
#define MOOSE_IS_ZEROCONF_SERVER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_ZEROCONF_SERVER))
#define MOOSE_ZEROCONF_SERVER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_ZEROCONF_SERVER, MooseZeroconfServerClass))
#define MOOSE_IS_ZEROCONF_SERVER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_ZEROCONF_SERVER))
#define MOOSE_ZEROCONF_SERVER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_ZEROCONF_SERVER, MooseZeroconfServerClass))

GType moose_zeroconf_server_get_type(void);

/**
 * MooseZeroconfState:
 *
 * Possible States of the Browser.
 */
typedef enum MooseZeroconfState {
    /* Not connected to Avahi */
    MOOSE_ZEROCONF_STATE_UNCONNECTED,
    /* Connected to avahi, but no results yet */
    MOOSE_ZEROCONF_STATE_CONNECTED,
    /* Some error happended, use moose_zeroconf_get_error to find out what */
    MOOSE_ZEROCONF_STATE_ERROR,
    /* A server was added or deleted, use moose_zeroconf_get_server to get a list */
    MOOSE_ZEROCONF_STATE_CHANGED,
    /* Informative event: Probably no new servers will be added in new future  */
    MOOSE_ZEROCONF_STATE_ALL_FOR_NOW
} MooseZeroconfState;

struct _MooseZeroconfBrowserPrivate;
struct _MooseZeroconfServerPrivate;

typedef struct _MooseZeroconfBrowser {
    GObject parent;
    struct _MooseZeroconfBrowserPrivate * priv;
} MooseZeroconfBrowser;

typedef struct _MooseZeroconfServer {
    GObject parent;
    struct _MooseZeroconfServerPrivate * priv;
} MooseZeroconfServer;

typedef struct _MooseZeroconfBrowserClass {
    GObjectClass parent_class;
} MooseZeroconfBrowserClass;

typedef struct _MooseZeroconfServerClass {
    GObjectClass parent_class;
} MooseZeroconfServerClass;

/**
 * moose_zeroconf_browser_new:
 *
 * Allocates a new #MooseZeroconfBrowser.
 *
 * Return value: a new #MooseZeroconfBrowser.
 */
MooseZeroconfBrowser * moose_zeroconf_browser_new(void);

/**
 * moose_zeroconf_browser_unref:
 *
 * Unrefs a #MooseZeroconfBrowser
 */
void moose_zeroconf_browser_unref(MooseZeroconfBrowser * self);

/**
 * moose_zeroconf_browser_get_state:
 *
 * Gets the current state of the Browser.
 *
 * Return value: one of #MooseZeroconfState
 */
MooseZeroconfState moose_zeroconf_browser_get_state(MooseZeroconfBrowser * self);

/**
 * moose_zeroconf_browser_get_error:
 *
 * Gets the current error, if the state is equal to MOOSE_ZEROCONF_STATE_ERROR.
 *
 * Return value: (transfer none): An static error description. Do not free.
 */
const char * moose_zeroconf_browser_get_error(MooseZeroconfBrowser * self);

/**
 * moose_zeroconf_browser_get_server_list:
 * @self: A #MooseZeroconfBrowser
 *
 * Returns: (element-type Moose.ZeroconfServer) (transfer full): List of #MooseZeroconfServer
 */
GList * moose_zeroconf_browser_get_server_list(MooseZeroconfBrowser * self);

/**
 * moose_zeroconf_server_get_host:
 * @self: A #MooseZeroconfServer
 *
 * Returns: (transfer full): Hostname of the Service. Free with g_free().
 */
char * moose_zeroconf_server_get_host(MooseZeroconfServer * self);

/**
 * moose_zeroconf_server_get_addr:
 * @self: A #MooseZeroconfServer
 *
 * Returns: (transfer full): Physical Address of the Service. Free with g_free().
 */
char * moose_zeroconf_server_get_addr(MooseZeroconfServer * self);

/**
 * moose_zeroconf_server_get_name:
 * @self: A #MooseZeroconfServer
 *
 * Returns: (transfer full): Selfgiven name of the Service. Free with g_free().
 */
char * moose_zeroconf_server_get_name(MooseZeroconfServer * self);

/**
 * moose_zeroconf_server_get_protocol:
 * @self: A #MooseZeroconfServer
 *
 * Returns: (transfer full): Protocol of the Service. Free with g_free().
 */
char * moose_zeroconf_server_get_protocol(MooseZeroconfServer * self);

/**
 * moose_zeroconf_server_get_domain:
 * @self: A #MooseZeroconfServer
 *
 * Returns: (transfer full): Domain of the Service. Free with g_free().
 */
char * moose_zeroconf_server_get_domain(MooseZeroconfServer * self);

/**
 * moose_zeroconf_server_get_port:
 * @self: A #MooseZeroconfServer
 *
 * Returns: Port of the Service
 */
int moose_zeroconf_server_get_port(MooseZeroconfServer * self);

G_END_DECLS

#endif /* end of include guard: MOOSE_ZEROCONF_BROWSE_HH */
