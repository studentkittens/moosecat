cimport binds as c


cdef class AudioOutput:
    '''
    AudioOutput is a class that encapusulates an mpd_output.

    It only offers three properties, one of which is settable:

        * **enable**: Get/Settable; Check if this  AudioOutput is active, or make it active.
        * **name**: Getter; The name of the AudioOutput as specified in mpd.conf
        * **oid**: MPD gives AudioOutput some ID. This is it. Not very useful, except for debugging.

    It should be instanced only from the Cython-Side, since it needs an pointer to
    mpd_output to work properly. You can instance it yourself, but you will get only
    a ValueError on usage.

    .. note::

        This might be unexpected, but AudioOutput mirrors only MPDs Status.
        If the Output is not yet known to be enabled on serverside, you will still
        get the "old" state:

        >>> first = client.outputs[0]
        >>> first.enabled
        True
        >>> first.enabled = False
        >>> first.enabled
        True  # Not set yet on serverside.
        >>> client.sync()
        >>> first.enabled
        True  # Still! Because it still wraps an old mpd_output instance.
        >>> first = client.outputs[0]
        >>> first.enabled
        False  # Finally!
    '''
    cdef c.mpd_output * _output
    cdef c.mc_Client * _client

    def __cinit__(self):
        'You should not instance this yourself'
        self._output = NULL
        self._client = NULL

    cdef c.mpd_output * _p(self) except NULL:
        if self._output != NULL:
            return self._output
        else:
            raise ValueError('mpd_output pointer is null for this instance!')

    cdef _init(self, c.mpd_output * output, c.mc_Client * client):
        'Meant for the Cython side'
        self._output = output
        self._client = client
        return self

    property enabled:
        'Getter/Setter: Check if the output is enabled, or make it active.'
        def __get__(self):
            return c.mpd_output_get_enabled(self._p())
        def __set__(self, state):
            c.mc_client_output_switch(self._client, c.mpd_output_get_name(self._p()), state)

    property name:
        'Getter: Get the name of the output like in the mpd.conf.'
        def __get__(self):
            # byte_opname gets not changed here. Promised.
            byte_opname = <char*>c.mpd_output_get_name(self._p())
            return stringify(byte_opname)

    property oid:
        'Get the MPD-Id of this Output. Probably not very useful.'
        def __get__(self):
            return c.mpd_output_get_id(self._p())
