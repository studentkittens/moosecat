cimport binds as c


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


def parse_query(string):
    b_string = bytify(string)
    return stringify(c.mc_store_qp_parse(b_string))


def parse_query_bytes(b_string):
    return c.mc_store_qp_parse(b_string)
