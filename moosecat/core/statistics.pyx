cimport binds as c
import time

cdef statistics_from_ptr(c.mpd_stats * ptr):
    return Statistics()._init(ptr)


cdef class Statistics:
    '''
    A object holding information about different statistics.

    See below for details on these.
    '''

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
        'Get the total number of distinct artists in the Database'
        def __get__(self):
            return c.mpd_stats_get_number_of_artists(self._p())

    property number_of_albums:
        'Get the total number of distinct albums in the Database'
        def __get__(self):
            return c.mpd_stats_get_number_of_albums(self._p())

    property number_of_songs:
        'Get the total number of distinct Songs in the Database'
        def __get__(self):
            return c.mpd_stats_get_number_of_songs(self._p())

    property uptime:
        'MPDs uptime with seconds since the last epoch'
        def __get__(self):
            return time.gmtime(c.mpd_stats_get_uptime(self._p()))

    property db_update_time:
        'Last update of the database measured as seconds since the last epoch'
        def __get__(self):
            return time.gmtime(c.mpd_stats_get_db_update_time(self._p()))

    property playtime:
        'Time MPD spend playing since it spawned in seconds.'
        def __get__(self):
            return time.gmtime(c.mpd_stats_get_play_time(self._p()))

    property db_playtime:
        'Time it would take to play all songs in the database in seconds.'
        def __get__(self):
            return time.gmtime(c.mpd_stats_get_db_play_time(self._p()))
