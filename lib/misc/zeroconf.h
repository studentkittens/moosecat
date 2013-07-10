#ifndef MC_ZEROCONF_BROWSE_HH
#define MC_ZEROCONF_BROWSE_HH

/**
 * Opaque private structures.
 * Use the functions to modify them
 */
struct mc_ZeroconfBrowser;
struct mc_ZeroconfServer;

/**
 * @brief Possible States of the Browser.
 */
typedef enum mc_ZeroconfState {
    MC_ZEROCONF_STATE_UNCONNECTED,  /* Not connected to Avahi                                                  */
    MC_ZEROCONF_STATE_CONNECTED,    /* Connected to avahi, but no results yet                                  */
    MC_ZEROCONF_STATE_ERROR,        /* Some error happended, use mc_zeroconf_get_error to find out what        */
    MC_ZEROCONF_STATE_CHANGED,      /* A server was added or deleted, use mc_zeroconf_get_server to get a list */
    MC_ZEROCONF_STATE_ALL_FOR_NOW   /* Informative event: Probably no new servers will be added in new future  */
} mc_ZeroconfState;

typedef void (* mc_ZeroconfCallback)(struct mc_ZeroconfBrowser *, void *);

/**
 * @brief Instance a new Zeroconf-Browser
 *
 * It will immediately try to connect to avahi and start searching once 
 * a mainloop is available. If the server is not running yet it will wait 
 * for it patiently to show up. If the server restarts it will reattach once
 * done. As user of the API you won't notice and can even access the old servers
 * from the last daemon.
 *
 * @return a newly allocated Browser, free with mc_zeroconf_destroy()
 */
struct mc_ZeroconfBrowser * mc_zeroconf_new(const char * protocol);

/**
 * @brief Destrpoy an allocagted Zeroconf Browser
 *
 * @param self Browser to deallocate
 */
void mc_zeroconf_destroy(struct mc_ZeroconfBrowser * self);

/**
 * @brief Register a callback that is called on state changes.
 *
 * If you want to unregister you can pass NULL as callback.
 *
 * @param self The browser to register upon.
 * @param callback The callback to call. 
 * @param user_data Optional user_data to be passed to the callback (or NULL)
 */
void mc_zeroconf_register(struct mc_ZeroconfBrowser * self, mc_ZeroconfCallback callback, void * user_data);

/**
 * @brief Get the current state of the Browser.
 *
 * See mc_ZeroconfState for Details.
 *
 * @param self Browser to ask state for.
 *
 * @return a value of mc_ZeroconfState
 */
mc_ZeroconfState mc_zeroconf_get_state(struct mc_ZeroconfBrowser * self);

/**
 * @brief Get last happended error or NULL if none.
 *
 * If a error happened state will change to MC_ZEROCONF_STATE_ERROR
 * and you will be notified. If not error happended this function will return
 * NULL.
 *
 * @param self the browser to ask for errors
 *
 * @return a constant string, do not free or alter.
 */
const char * mc_zeroconf_get_error(struct mc_ZeroconfBrowser * self);

/**
 * @brief Get a NULL terminated List of Servers.
 *
 * If no servers were detected this returns NULL.
 *
 * @param self the Browser to get the Server list from
 *
 * @return a array of mc_ZeroconfServers, free only the list with free()
 */
struct mc_ZeroconfServer ** mc_zeroconf_get_server(struct mc_ZeroconfBrowser * self);

/**
 * @brief Return the host a server.
 *
 * Do not free or alter the returned value.
 *
 * @param a server from mc_zeroconf_get_server()
 *
 * @return a nullterminated string.
 */
const char * mc_zeroconf_server_get_host(struct mc_ZeroconfServer * server);

/**
 * @brief Return the host a server (e.g. "localhost").
 *
 * Do not free or alter the returned value.
 *
 * @param a server from mc_zeroconf_get_server()
 *
 * @return a nullterminated string.
 */
const char * mc_zeroconf_server_get_addr(struct mc_ZeroconfServer * server);

/**
 * @brief Return the name of a server (e.g. "Seb's MPD Server").
 *
 * Do not free or alter the returned value.
 *
 * @param a server from mc_zeroconf_get_server()
 *
 * @return a nullterminated string.
 */
const char * mc_zeroconf_server_get_name(struct mc_ZeroconfServer * server);

/**
 * @brief Return the type of a server (e.g. "_mpd._tcp")
 *
 * Do not free or alter the returned value.
 *
 * @param a server from mc_zeroconf_get_server()
 *
 * @return a nullterminated string.
 */
const char * mc_zeroconf_server_get_type(struct mc_ZeroconfServer * server);

/**
 * @brief Return the domain of a server (e.g. "local") 
 *
 * Do not free or alter the returned value.
 *
 * @param a server from mc_zeroconf_get_server()
 *
 * @return a nullterminated string.
 */
const char * mc_zeroconf_server_get_domain(struct mc_ZeroconfServer * server);

/**
 * @brief Return the port of a server (e.g. 6600)
 *
 * @param a server from mc_zeroconf_get_server()
 *
 * @return an unsigned port number
 */
unsigned mc_zeroconf_server_get_port(struct mc_ZeroconfServer * server);


#endif /* end of include guard: MC_ZEROCONF_BROWSE_HH */

