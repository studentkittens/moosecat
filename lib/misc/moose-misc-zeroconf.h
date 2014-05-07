#ifndef MOOSE_ZEROCONF_BROWSE_HH
#define MOOSE_ZEROCONF_BROWSE_HH


#include <glib-object.h>

G_BEGIN_DECLS

/*
 * Type macros.
 */
#define MOOSE_TYPE_ZEROCONF_BROWSER \
    (moose_zeroconf_browser_get_type ())
#define MOOSE_ZEROCONF_BROWSER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOOSE_TYPE_ZEROCONF_BROWSER, MooseZeroconfBrowser))
#define MOOSE_IS_ZEROCONF_BROWSER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOOSE_TYPE_ZEROCONF_BROWSER))
#define MOOSE_ZEROCONF_BROWSER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MOOSE_TYPE_ZEROCONF_BROWSER, MooseZeroconfBrowserClass))
#define MOOSE_IS_ZEROCONF_BROWSER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MOOSE_TYPE_ZEROCONF_BROWSER))
#define MOOSE_ZEROCONF_BROWSER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MOOSE_TYPE_ZEROCONF_BROWSER, MooseZeroconfBrowserClass))
    
GType moose_zeroconf_browser_get_type(void);


#define MOOSE_TYPE_ZEROCONF_SERVER \
    (moose_zeroconf_server_get_type ())
#define MOOSE_ZEROCONF_SERVER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOOSE_TYPE_ZEROCONF_SERVER, MooseZeroconfServer))
#define MOOSE_IS_ZEROCONF_SERVER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOOSE_TYPE_ZEROCONF_SERVER))
#define MOOSE_ZEROCONF_SERVER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), MOOSE_TYPE_ZEROCONF_SERVER, MooseZeroconfServerClass))
#define MOOSE_IS_ZEROCONF_SERVER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), MOOSE_TYPE_ZEROCONF_SERVER))
#define MOOSE_ZEROCONF_SERVER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), MOOSE_TYPE_ZEROCONF_SERVER, MooseZeroconfServerClass))

GType moose_zeroconf_server_get_type(void);

/**
 * @brief Possible States of the Browser.
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
    struct _MooseZeroconfBrowserPrivate *priv;
} MooseZeroconfBrowser;

typedef struct _MooseZeroconfServer {
    GObject parent;      
    struct _MooseZeroconfServerPrivate *priv;
} MooseZeroconfServer;

typedef struct _MooseZeroconfBrowserClass {
  GObjectClass parent_class;
} MooseZeroconfBrowserClass;

typedef struct _MooseZeroconfServerClass {
  GObjectClass parent_class;
} MooseZeroconfServerClass;


MooseZeroconfBrowser *moose_zeroconf_browser_new(void);
void moose_zeroconf_browser_destroy(MooseZeroconfBrowser *self);

/**
 * @brief Get the current state of the Browser.
 *
 * See MooseZeroconfState for Details.
 *
 * @param self Browser to ask state for.
 *
 * @return a value of MooseZeroconfState
 */
MooseZeroconfState moose_zeroconf_browser_get_state(MooseZeroconfBrowser * self);

/**
 * @brief Get last happended error or NULL if none.
 *
 * If a error happened state will change to MOOSE_zeroconf_browser_STATE_ERROR
 * and you will be notified. If not error happended this function will return
 * NULL.
 *
 * @param self the browser to ask for errors
 *
 * @return a constant string, do not free or alter.
 */
const char * moose_zeroconf_browser_get_error(MooseZeroconfBrowser * self);

/**
 * @brief Get a NULL terminated List of Servers.
 *
 * If no servers were detected this returns NULL.
 *
 * @param self the Browser to get the Server list from
 *
 * @return a array of MooseZeroconfServers, free only the list with free()
 */
GList *moose_zeroconf_browser_get_server_list(MooseZeroconfBrowser * self);



/**
 * @brief Return the host a server.
 *
 * Do not free or alter the returned value.
 *
 * @param a server from moose_zeroconf_get_server()
 *
 * @return a nullterminated string.
 */
char * moose_zeroconf_server_get_host(MooseZeroconfServer * server);

/**
 * @brief Return the host a server (e.g. "localhost").
 *
 * Do not free or alter the returned value.
 *
 * @param a server from moose_zeroconf_get_server()
 *
 * @return a nullterminated string.
 */
char * moose_zeroconf_server_get_addr(MooseZeroconfServer * server);

/**
 * @brief Return the name of a server (e.g. "Seb's MPD Server").
 *
 * Do not free or alter the returned value.
 *
 * @param a server from moose_zeroconf_get_server()
 *
 * @return a nullterminated string.
 */
char * moose_zeroconf_server_get_name(MooseZeroconfServer * server);

/**
 * @brief Return the type of a server (e.g. "_mpd._tcp")
 *
 * Do not free or alter the returned value.
 *
 * @param a server from moose_zeroconf_get_server()
 *
 * @return a nullterminated string.
 */
char * moose_zeroconf_server_get_protocol(MooseZeroconfServer * server);

/**
 * @brief Return the domain of a server (e.g. "local") 
 *
 * Do not free or alter the returned value.
 *
 * @param a server from moose_zeroconf_get_server()
 *
 * @return a nullterminated string.
 */
char * moose_zeroconf_server_get_domain(MooseZeroconfServer * server);

/**
 * @brief Return the port of a server (e.g. 6600)
 *
 * @param a server from moose_zeroconf_get_server()
 *
 * @return an unsigned port number
 */
int moose_zeroconf_server_get_port(MooseZeroconfServer * server);


/* DEPRECATED */

MooseZeroconfServer ** moose_zeroconf_browser_get_server(MooseZeroconfBrowser * self);

G_END_DECLS

#endif /* end of include guard: MOOSE_ZEROCONF_BROWSE_HH */
