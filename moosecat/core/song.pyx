cimport binds as c

'''
Song is a Python-Level Wrapper aroung libmpdclient's mpd_song.

It has the same features, but is a lot nicer to use:
>>> s = song_from_ptr(some_mpd_song_struct)
>>> print(s.artist)

Instead of:
>>> s = song_from_ptr(some_mpd_song_struct)
>>> c.mpd_song_get_tag(s, c.MPD_TAG_ARTIST, 0)

Instanciation should always happen via song_from_ptr()!
(Will probably crash easily otherwise)
'''

# Last Modified Date
import time

cdef song_from_ptr(c.mpd_song * ptr):
    'Instance a new Song() with a the underlying mpd_song ptr'
    if ptr != NULL:
        return Song()._init(ptr)
    else:
        return None

cdef class Song:
    # Actual c-level struct
    cdef c.mpd_song * _song

    ################
    #  Allocation  #
    ################

    def __cinit__(self):
        self._song = NULL

    cdef c.mpd_song * _p(self) except NULL:
        if self._song != NULL:
            return self._song
        else:
            raise ValueError('mpd_song pointer is null for this instance!')

    cdef object _init(self, c.mpd_song * song):
        self._song = song
        return self

    #############
    #  Testing  #
    #############

    def test_begin(self, file_name='file://test_case_path'):
        cdef c.mpd_pair pair
        byte_file = bytify(file_name)
        pair.name = 'file'
        pair.value = byte_file
        self._song = c.mpd_song_begin(&pair)

    def test_feed(self, name, value):
        cdef c.mpd_pair pair
        b_name = bytify(name)
        b_value = bytify(value)
        pair.name = b_name
        pair.value = b_value
        c.mpd_song_feed(self._p(), &pair)

    ################
    #  Properties  #
    ################

    cdef _tag(self, c.mpd_tag_type tag):
        cdef c.const_char_ptr p
        p = c.mpd_song_get_tag(<c.const_mpd_song_ptr>self._p(), tag, 0)
        if p == NULL:
            return ''
        else:
            return stringify(<char*>p)

    property artist:
        'Retrieve the artist tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_ARTIST)

    property album:
        'Retrieve the album tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_ALBUM)

    property album_artist:
        'Retrieve the title tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_ALBUM_ARTIST)

    property title:
        'Retrieve the title tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_TITLE)

    property track:
        'Retrieve the track tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_TRACK)

    property genre:
        'Retrieve the genre tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_GENRE)

    property date:
        'Retrieve the date tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_DATE)

    property performer:
        'Retrieve the performer tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_PERFORMER)

    property comment:
        'Retrieve the artist tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_COMMENT)

    property composer:
        'Retrieve the composer tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_COMPOSER)

    property disc:
        'Retrieve the disc tag'
        def __get__(self):
            return self._tag(c.MPD_TAG_DISC)

    property musicbrainz_artistid:
        'Retrieve the MB ArtistID'
        def __get__(self):
            return self._tag(c.MPD_TAG_MUSICBRAINZ_ARTISTID)

    property musicbrainz_albumartistid:
        'Retrieve the MB AlbumArtistID'
        def __get__(self):
            return self._tag(c.MPD_TAG_MUSICBRAINZ_ALBUMARTISTID)

    property musicbrainz_trackid:
        'Retrieve the MB TrackID'
        def __get__(self):
            return self._tag(c.MPD_TAG_MUSICBRAINZ_TRACKID)

    property musicbrainz_albumid:
        'Retrieve the MB AlbumID'
        def __get__(self):
            return self._tag(c.MPD_TAG_MUSICBRAINZ_ALBUMID)

    ########################
    #  Non-Tag Properties  #
    ########################

    property uri:
        def __get__(self):
            return c.mpd_song_get_uri(self._p())

    property duration:
        def __get__(self):
            return c.mpd_song_get_duration(self._p())

    property start_end:
        def __get__(self):
            return (c.mpd_song_get_start(self._p()),
                    c.mpd_song_get_end(self._p()))

    property queue_pos:
        def __get__(self):
            return c.mpd_song_get_pos(self._p())
        def __set__(self, int pos):
            c.mpd_song_set_pos(self._p(), pos)

    property queue_id:
        def __get__(self):
            return c.mpd_song_get_id(self._p())

    property last_modified:
        def __get__(self):
            return time.gmtime(c.mpd_song_get_last_modified(self._p()))