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


cdef char * parse_query_bytes(b_query) except NULL:
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
