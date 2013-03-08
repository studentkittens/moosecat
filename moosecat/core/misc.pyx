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
