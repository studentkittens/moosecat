cimport binds as c


cdef class AudioOutput:
    '''
    AudioOutput is a class that encapusulates an mpd_output_name.

    It only offers three properties, one of which is settable:

        * **enable**: Get/Settable; Check if this  AudioOutput is active, or make it active.
        * **name**: Getter; The name of the AudioOutput as specified in mpd.conf
        * **oid**: MPD gives AudioOutput some ID. This is it. Not very useful, except for debugging.

    It should be instanced only from the Cython-Side, since it needs an pointer to
    mpd_output_name to work properly. You can instance it yourself, but you will get only
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
        True  # Still! Because it still wraps an old mpd_output_name instance.
        >>> first = client.outputs[0]
        >>> first.enabled
        False  # Finally!
    '''
    cdef const char * _output_name
    cdef c.mc_Client * _client

    def __cinit__(self):
        'You should not instance this yourself'
        self._output_name = NULL
        self._client = NULL

    cdef _init(self, const char * output, c.mc_Client * client):
        'Meant for the Cython side'
        self._output_name = output
        self._client = client
        return self

    property enabled:
        'Getter/Setter: Check if the output is enabled, or make it active.'
        def __get__(self):
            return c.mc_outputs_get_state(self._client, self._output_name)
        def __set__(self, state):
            c.mc_outputs_set_state(self._client, self._output_name, 1 if state else 0)

    property name:
        'Getter: Get the name of the output like in the mpd.conf.'
        def __get__(self):
            return stringify(<char *>self._output_name)
