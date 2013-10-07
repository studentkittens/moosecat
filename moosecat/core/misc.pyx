cimport binds as c
from libc.stdlib cimport free


def bug_report(Client client=None):
    '''
    Get a string describing libmoosecat's configuration and current state.

    :client: a Client object. Used to get extra info.
    :returns: a multiline string with information (rst conform)
    '''
    if client is None:
        report = c.mc_misc_bug_report(NULL)
    else:
        report = c.mc_misc_bug_report(client._p())

    return stringify(report)


def register_external_logs(Client client):
    '''
    Catch Gtk/GLibs logging, and forward them via the error callback.

    :client: The client object to use to forward error messages.
    :throws: a ValueError if no client is passed.
    '''
    if client is not None:
        c.mc_misc_catch_external_logs(client._p())
    else:
        raise ValueError('client argument may not be None')


class QueryParseException(Exception):
    '''
    Thrown if parse_query() encounters invalid syntax.

    The exact cause can be found in self.msg, the position
    of the error in (approx.) self.pos.
    '''
    def __init__(self, msg, pos):
        self.msg = msg
        self.pos = pos
        Exception.__init__(self, msg)


def parse_query(query):
    '''
    Parse a string (in the format of a Extended Search Syntax), to a query
    that can be used by SQLite's FTS Extension.

    .. note::

        There's usually no need to use this function directly, since
        all query-functions of :class:`moosecat.core.Store` already call
        this implicitly on your input. It might be useful to check if
        a query is valid.

    :query: An arbitary input string.
    :returns: A parsed string.
    :throws: a QueryParseException on invalid input.
    '''
    cdef char * b_result = NULL

    result = None
    b_query = bytify(query)
    b_result = parse_query_bytes(b_query)
    result = stringify(b_result)

    if b_result is not NULL:
        free(b_result)

    return result


cdef char * parse_query_bytes(b_query) except *:
    'Used internally. So no byte-str-conversion needs to be done.'
    cdef const char *warning = NULL
    cdef int warning_pos = 0
    cdef char *result = NULL

    result = c.mc_store_qp_parse(b_query, &warning, &warning_pos)
    if warning is not NULL:  # Uh-oh.
        if result is not NULL:
            free(result)
        raise QueryParseException(stringify(<char *>warning), warning_pos)
    return result

##################
# ZEROCONF STUFF #
##################

# Static callback that gets called from the C-Side.
# It will only call the registered Python function if any.
# log_exception() is defined in client.pyx
cdef void * wrap_ZeroconfCallback(c.mc_ZeroconfBrowser * browser, object data) with gil:
    try:
        data[0](data[1])
    except:
        log_exception(data[0])


cdef class ZeroconfState:
    '''
    Possible states of ZeroconfBrowser.

    Details:

        * ``UNCONNECTED`` : Browser is not connected.
        * ``CONNECTED``   : Browser is connected, but no results yet.
        * ``ERROR``       : An error occured, use browser.error to find out.
        * ``CHANGED``     : Server list changed. Use browser.server_list to get them.
        * ``ALL_FOR_NOW`` : Probably no new Servers in the next while.
    '''
    UNCONNECTED = c.ZEROCONF_STATE_UNCONNECTED
    CONNECTED   = c.ZEROCONF_STATE_CONNECTED
    ERROR       = c.ZEROCONF_STATE_ERROR
    CHANGED     = c.ZEROCONF_STATE_CHANGED
    ALL_FOR_NOW = c.ZEROCONF_STATE_ALL_FOR_NOW


cdef class ZeroconfBrowser:
    '''
    ZeroconfBrowser that can use Avahi to automatically list MPD servers in reach.
    '''
    cdef c.mc_ZeroconfBrowser * _browser
    cdef object _callback_data_map

    def __cinit__(self, protocol='_mpd._tcp'):
        '''
        Instance a new ZeroconfBrowser.
        Once instanced, and once a mainloop is ready it will start searching
        and call any registered callback on state change.

        :protocol: Used protocol-type to query for (default: "_mpd._tcp")
        '''
        b_protocol = bytify(protocol)
        self._browser = c.mc_zeroconf_new(b_protocol)
        self._callback_data_map = {}

    def __dealloc__(self):
        c.mc_zeroconf_destroy(self._browser)

    def register(self, func):
        '''
        Register a callback that is called on every state change of the browser.

        Will raise a ValueError if func is not a callable.

        :func: A callable that is called on each State change.
        '''
        if callable(func):
            data = [func, self]
            self._callback_data_map[func] = data
            c.mc_zeroconf_register(self._browser, <void *>wrap_ZeroconfCallback, <void *>data)
        else:
            raise ValueError('`func` must be a Callable.')

    property server_list:
        '''
        Get a list of available servers. You should call this in the callback
        to be always sure to have the most current list.

        This will return a list of dicionaries with following string keys:

            * **host**: Hostname of the Server ('localhost')
            * **addr**: Address of the Server  ('127.0.0.1')
            * **name**: Name of the Server     ('Grandmas Music Player Daemon')
            * **type**: Type of the Server     (usually '_mpd._tcp')
            * **domain**: Domain of the Server ('local')
            * **port**: Port of the Server     (6600)
        '''
        def __get__(self):
            cdef c.mc_ZeroconfServer ** server = c.mc_zeroconf_get_server(self._browser)
            cdef c.mc_ZeroconfServer * current
            cdef int i = 0

            py_server_list = []
            if server != NULL:
                while server[i] != NULL:
                    current = server[i]
                    py_server_list.append({
                        'host'   : stringify(<char *>c.mc_zeroconf_server_get_host(current)),
                        'addr'   : stringify(<char *>c.mc_zeroconf_server_get_addr(current)),
                        'name'   : stringify(<char *>c.mc_zeroconf_server_get_name(current)),
                        'type'   : stringify(<char *>c.mc_zeroconf_server_get_type(current)),
                        'domain' : stringify(<char *>c.mc_zeroconf_server_get_domain(current)),
                        'port'   : c.mc_zeroconf_server_get_port(current)
                    })
                    i += 1
                free(server)
            return py_server_list

    property error:
        'Get the last happended Error or an empty string if none happened lately.'
        def __get__(self):
            return stringify(<char *>c.mc_zeroconf_get_error(self._browser))

    property state:
        'Current state of the Browser. Its one of the values in ZeroconfState.'
        def __get__(self):
            return c.mc_zeroconf_get_state(self._browser)
