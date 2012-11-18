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

    cdef c.mpd_stats * _p(self):
        return self._stats

    cdef object _init(self, c.mpd_stats * stats):
        self._stats = stats
        return self

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

    property play_time:
        def __get__(self):
            return c.mpd_stats_get_play_time(self._p())

    property db_play_time:
        def __get__(self):
            return c.mpd_stats_get_db_play_time(self._p())
