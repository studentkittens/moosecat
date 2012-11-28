cimport binds as c

cdef statistics_from_ptr(c.mpd_stats * ptr):
    return Statistics()._init(ptr)


cdef class Statistics:

    ################
    #  Allocation  #
    ################

    cdef c.mpd_stats * _stats

    def __cinit__(self):
        self._stats = NULL

    cdef c.mpd_stats * _p(self) except NULL:
        if self._stats != NULL:
            return self._stats
        else:
            raise ValueError('mpd_stats pointer is null for this instance!')

    cdef object _init(self, c.mpd_stats * stats):
        self._stats = stats
        return self

    #############
    #  Testing  #
    #############

    def test_begin(self):
        self._stats = c.mpd_stats_begin()

    def test_feed(self, name, value):
        cdef c.mpd_pair pair
        b_name = bytify(name)
        b_value = bytify(value)
        pair.name = b_name
        pair.value = b_value
        c.mpd_stats_feed(self._p(), &pair)

    ################
    #  Properties  #
    ################

    property number_of_artists:
        def __get__(self):
            return c.mpd_stats_get_number_of_artists(self._p())

    property number_of_albums:
        def __get__(self):
            return c.mpd_stats_get_number_of_albums(self._p())

    property number_of_songs:
        def __get__(self):
            return c.mpd_stats_get_number_of_songs(self._p())

    property uptime:
        def __get__(self):
            return c.mpd_stats_get_uptime(self._p())

    property db_update_time:
        def __get__(self):
            return c.mpd_stats_get_db_update_time(self._p())

    property playtime:
        def __get__(self):
            return c.mpd_stats_get_play_time(self._p())

    property db_playtime:
        def __get__(self):
            return c.mpd_stats_get_db_play_time(self._p())
