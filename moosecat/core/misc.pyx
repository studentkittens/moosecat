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
    if client is not None:
        c.mc_misc_catch_external_logs(client._p())
    else:
        raise ValueError('client argument may not be None')


class QueryParseException(Exception):
    def __init__(self, msg, pos):
        self.msg = msg
        self.pos = pos
        Exception.__init__(self, msg)


def parse_query(query):
    b_query = bytify(query)
    b_result = parse_query_bytes(b_query)
    return stringify(b_result)


def parse_query_bytes(b_query):
    cdef const char *warning = NULL
    cdef int warning_pos = 0
    cdef char *result

    result = c.mc_store_qp_parse(b_query, &warning, &warning_pos)
    if warning is not NULL:  # Uh-oh.
        if result is not NULL:
            free(result)
        raise QueryParseException(stringify(<char *>warning), warning_pos)
    else:
        return result
