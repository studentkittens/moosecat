###########################################################################
#                    Common Utils on the Wrapper Side                     #
###########################################################################

# I refuse to write UTF-8 all the time
cdef stringify(char * bytestring):
    'Convert bytes to str, using utf-8'
    if bytestring is NULL:
        return ''
    else:
        return bytestring.decode('UTF-8')


# We use UTF-8 anyways everywhere
cdef bytify(string):
    'Convert str to bytes, using utf-8'
    return string.encode('UTF-8')


cdef client_send(c.MooseClient * client, b_full_command):
    'Send a raw byte command to the client core'
    return c.moose_client_send(client, b_full_command)


def _fmt(command, *args):
    'Format a command and optional arguments to a full command suitable for client_send'
    b_command = bytify(command.strip())
    if args:
        b_args = bytify(' /// '.join(str(arg) for arg in args))
        return b_command + b' ' + b_args
    else:
        return b_command
