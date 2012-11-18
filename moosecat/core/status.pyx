cimport binds as c

cdef status_from_ptr(c.mpd_status * ptr):
    'Instance a new Status() with the underlying mpd_status ptr'
    if ptr != NULL:
        return Status()._init(ptr)
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

    ################
    #  Allocation  #
    ################

    def __cinit__(self):
        self._status = NULL

    cdef c.mpd_status * _p(self):
        return self._status

    cdef object _init(self, c.mpd_status * status):
        self._status = status
        return self

    ################
    #  Properties  #
    ################

    property volume:
        def __get__(self):
            return c.mpd_status_get_volume(self._p())

    property repeat:
        def __get__(self):
            return c.mpd_status_get_repeat(self._p())

    property random:
        def __get__(self):
            return c.mpd_status_get_random(self._p())

    property single:
        def __get__(self):
            return c.mpd_status_get_single(self._p())

    property consume:
        def __get__(self):
            return c.mpd_status_get_consume(self._p())

    property queue_length:
        def __get__(self):
            return c.mpd_status_get_queue_length(self._p())

    property queue_version:
        def __get__(self):
            return c.mpd_status_get_queue_version(self._p())

    property state:
        def __get__(self):
            cstate = c.mpd_status_get_state(self._p())
            return Status._cmpdstate_to_py[cstate]

    property crossfade:
        def __get__(self):
            return c.mpd_status_get_crossfade(self._p())

    property mixrampdb:
        def __get__(self):
            c.mpd_status_get_mixrampdb(self._p())

    property mixrampdelay:
        def __get__(self):
            c.mpd_status_get_mixrampdelay(self._p())

    property song_pos:
        def __get__(self):
            c.mpd_status_get_song_pos(self._p())

    property song_id:
        def __get__(self):
            c.mpd_status_get_song_id(self._p())

    property next_song_id:
        def __get__(self):
            c.mpd_status_get_next_song_id(self._p())

    property next_song_pos:
        def __get__(self):
            c.mpd_status_get_next_song_pos(self._p())

    property elapsed_seconds:
        def __get__(self):
            c.mpd_status_get_elapsed_time(self._p())

    property elapsed_ms:
        def __get__(self):
            c.mpd_status_get_elapsed_ms(self._p())

    property total_time:
        def __get__(self):
            c.mpd_status_get_total_time(self._p())

    property update_id:
        def __get__(self):
            c.mpd_status_get_update_id(self._p())

    ######################
    #  Audio Properties  #
    ######################

    cdef c.mpd_audio_format * _audio(self):
        return <c.mpd_audio_format*> c.mpd_status_get_audio_format(self._p())

    property kbit_rate:
        def __get__(self):
            c.mpd_status_get_kbit_rate(self._p())

    property audio_sample_rate:
        def __get__(self):
            return self._audio().sample_rate

    property audio_bits:
        def __get__(self):
            return self._audio().bits

    property audio_channels:
        def __get__(self):
            return self._audio().channels
