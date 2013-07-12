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


cdef client_send(c.mc_Client * client, command):
    cdef char * b_command
    b_command = command

    return c.mc_client_send(client, b_command)
