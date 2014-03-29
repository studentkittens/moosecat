'''
Status is a wrapper around MPD's 'status' command.

It will easily let you access attributes like the volume, the player state
and some more things. Also some properties are **settable**, so you can
change these attributes actually!

Usage Example:

    >>> if client.status.volume > 100:
    ...     client.status.volume = 100

**IMPORTANT NOTE:** Do not save references to client.status, a change to the Status
                    might cause to allocate a new Status object and transparently
                    subsitute the current one. The old one will contain old, incorrect
                    values!

Don't do this: ::

    >>> my_status = client.status
    >>> my_status.volume
    50
    >>> my_status.volume = 100
    >>> # After some iterations of the mainloop
    >>> my_status.volume # Uh-oh!
    50
    >>> client.status.volume
    100


**Hint:** You will use this most likely by accessing 'client.status'.
'''

cimport binds as c


cdef status_from_ptr(c.mpd_status * ptr, c.mc_Client * client):
    'Instance a new Status() with the underlying mpd_status ptr'
    if ptr != NULL:
        return Status()._init(ptr, client)
    else:
        return None

cdef class Status:
    # State Enums
    Playing = c.MPD_STATE_PLAY
    Paused  = c.MPD_STATE_PAUSE
    Stopped = c.MPD_STATE_STOP
    Unknown = c.MPD_STATE_UNKNOWN

    # c.MPD_STATE_ -> Python State
    _cmpdstate_to_py = {
            c.MPD_STATE_PLAY: Playing,
            c.MPD_STATE_PAUSE: Paused,
            c.MPD_STATE_STOP: Stopped,
            c.MPD_STATE_UNKNOWN: Unknown
    }

    # Actual c-level struct
    cdef c.mpd_status * _status
    cdef c.mc_Client * _client

    ################
    #  Allocation  #
    ################

    def __cinit__(self):
        self._status = NULL

    cdef c.mpd_status * _p(self) except NULL:
        if self._status != NULL:
            return self._status
        else:
            raise ValueError('mpd_status pointer is null for this instance!')

    cdef c.mc_Client * _c(self) except NULL:
        if self._client!= NULL:
            return self._client
        else:
            raise ValueError('_client pointer is null for this instance!')

    cdef object _init(self, c.mpd_status * status, c.mc_Client * client):
        self._status = status
        self._client = client
        return self

    #############
    #  Testing  #
    #############

    def test_begin(self):
        self._status = c.mpd_status_begin()

    def test_feed(self, name, value):
        cdef c.mpd_pair pair
        b_name = bytify(name)
        b_value = bytify(value)
        pair.name = b_name
        pair.value = b_value
        c.mpd_status_feed(self._p(), &pair)

    ################
    #  Properties  #
    ################

    property volume:
        'Get the current volume from 0 - 100'
        def __get__(self):
            return c.mpd_status_get_volume(self._p())
        def __set__(self, vol):
            if not (0 <= vol <= 100):
                return

            if vol is self.volume:
                return

            client_send(self._c(), _fmt('setvol', int(vol)))

    property repeat:
        'Check if repeat mode is on'
        def __get__(self):
            return c.mpd_status_get_repeat(self._p())
        def __set__(self, state):
            client_send(self._c(), _fmt('repeat', 1 if state else 0))

    property random:
        'Check if random mode is on'
        def __get__(self):
            return c.mpd_status_get_random(self._p())
        def __set__(self, state):
            client_send(self._c(), _fmt('random', 1 if state else 0))

    property single:
        'Check if single mode is on'
        def __get__(self):
            return c.mpd_status_get_single(self._p())
        def __set__(self, state):
            client_send(self._c(), _fmt('single', 1 if state else 0))

    property consume:
        'Check if consume mode is on'
        def __get__(self):
            return c.mpd_status_get_consume(self._p())
        def __set__(self, state):
            client_send(self._c(), _fmt('consume ', 1 if state else 0))

    property queue_length:
        'Return the length of the Queue'
        def __get__(self):
            return c.mpd_status_get_queue_length(self._p())

    property queue_version:
        '''
        Return the Queue version.

        It changes whenever a operation is done on the Queue.
        This includes: move, delete, add. Every atomar operations increments
        the queue version by one.

        Generally this is not very useful to the "outer" client, but very
        interesting to libmoosecat.
        '''
        def __get__(self):
            return c.mpd_status_get_queue_version(self._p())

    property state:
        '''
        The current state of playback.

        Can be:

            * Status.Playing
            * Status.Stopped
            * Status.Paused
            * Status.Unknown

        Use :class:`moosecat.core.Client` 's player_xyz() family of functions
        to modify the state.
        '''
        def __get__(self):
            cstate = c.mpd_status_get_state(self._p())
            return Status._cmpdstate_to_py[cstate]

    property crossfade:
        'Get or Set the Crossfade in seconds or 0 to disable'
        def __get__(self):
            return c.mpd_status_get_crossfade(self._p())
        def __set__(self, crossfade):
            client_send(self._c(), _fmt('crossfade', crossfade))

    property mixrampdb:
        'Retrieve or Set the mixrampdb value (see mpds documentation)'
        def __get__(self):
            return c.mpd_status_get_mixrampdb(self._p())
        def __set__(self, decibel):
            client_send(self._c(), _fmt('mixrampdb', decibel))

    property mixrampdelay:
        'Mixrampdelay (see mpds documentation)'
        def __get__(self):
            return c.mpd_status_get_mixrampdelay(self._p())
        def __set__(self, seconds):
            client_send(self._c(), _fmt('mixrampdelay', seconds))

    property song_pos:
        'Position of the currently playing song within the Queue'
        def __get__(self):
            return c.mpd_status_get_song_pos(self._p())

    property song_id:
        'ID of the currently playing song within the Queue'
        def __get__(self):
            return c.mpd_status_get_song_id(self._p())

    property next_song_id:
        'ID of the next playing song within the Queue'
        def __get__(self):
            return c.mpd_status_get_next_song_id(self._p())

    property next_song_pos:
        'Pos of the next playing song within the Queue'
        def __get__(self):
            return c.mpd_status_get_next_song_pos(self._p())

    property elapsed_seconds:
        'Elapsed seconds of the currently playing song'
        def __get__(self):
            return c.mpd_status_get_elapsed_time(self._p())

    property elapsed_ms:
        'Elapsed milliseconds of the currently playing song'
        def __get__(self):
            return c.mpd_status_get_elapsed_ms(self._p())

    property total_time:
        'Total duration of song in seconds'
        def __get__(self):
            return c.mpd_status_get_total_time(self._p())

    property is_updating:
        'Check if mpd is currently updating'
        def __get__(self):
            return c.mpd_status_get_update_id(self._p()) > 0

    ######################
    #  Audio Properties  #
    ######################

    cdef c.mpd_audio_format * _audio(self) except? NULL:
        return <c.mpd_audio_format*> c.mpd_status_get_audio_format(self._p())

    property kbit_rate:
        '''
        audio: The last known kilobyte rate of the currently playing song.

        .. note::

            Normally this will only be updated on certain events in order to
            save network traffic. If you want to update this constantly use
            the :py:func:`Client.status_timer_activate` function.
        '''
        def __get__(self):
            return c.mpd_status_get_kbit_rate(self._p())

    property audio_sample_rate:
        'audio: The sample rate of the current song'
        def __get__(self):
            if self._audio():
                return self._audio().sample_rate
            else:
                return 0

    property audio_bits:
        'audio: mostly 24 bit'
        def __get__(self):
            if self._audio():
                return self._audio().bits
            else:
                return 0

    property audio_channels:
        'audio: used channels '
        def __get__(self):
            if self._audio():
                return self._audio().channels
            else:
                return 0

    ############################
    #  Replay Gain Properties  #
    ############################

    property replay_gain_mode:
        '''
        Return or Set the current replay_gain mode

        Possible values: ['off', 'album', 'track', 'auto']
        '''
        def __get__(self):
            return stringify(<char *>c.mc_get_replay_gain_mode(self._c()))

        def __set__(self, mode):
            if mode in ['off', 'album', 'track', 'auto']:
                client_send(self._c(), _fmt('replay_gain_mode', mode))
            else:
                raise ValueError('mode must be one of off, track, album, auto')

    def __str__(self):
        # Quite a hack, but only used for debugging.
        attrs = {k: v.__get__(self)
                 for k, v in self.__class__.__dict__.items()
                 if type(v) == type(Status.volume)}
        return '\n'.join(['{k}: {v}'.format(k=k,v=v) for k,v in attrs.items()])
