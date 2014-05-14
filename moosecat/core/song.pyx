cimport binds as c

'''
Song is a Python-Level Wrapper around Moosecat's Song.

It has the same features, but is a lot nicer to use:
>>> s = song_from_ptr(some_moose_song_struct)
>>> print(s.artist)

Instead of:
>>> s = song_from_ptr(some_moose_song_struct)
>>> c.moose_song_get_tag(s, c.MPD_TAG_ARTIST)

Instanciation should always happen via song_from_ptr()!
(Will probably crash easily otherwise)
'''

# Last Modified Date
import time

cdef object song_from_ptr(c.MooseSong * ptr):
    'Instance a new Song() with a the underlying MooseSong ptr'
    if ptr != NULL:
        return Song()._init(ptr)
    else:
        return None

cdef class Song:
    '''
    A song represents one song in MPD's database.

    It only stores metadata, and can not be used to change a Song.
    Instead an easy way to access tags is provided.

    .. note::

        Song is just a wrapper around libmpdclient's MooseSong. Internally,
        a stack with all MooseSongs known to libmoosecat is held. These can be
        wrapped though in 1..n Song-Wrappers! So, don't rely on something like
        a id-compare to check if a song is equal.
    '''
    # Actual c-level struct
    cdef c.MooseSong * _song

    ################
    #  Allocation  #
    ################

    def __cinit__(self):
        self._song = NULL

    cdef c.MooseSong * _p(self) except NULL:
        if self._song != NULL:
            return self._song
        else:
            raise ValueError('MooseSong pointer is null for this instance!')

    cdef object _init(self, c.MooseSong * song):
        self._song = song
        return self

    #############
    #  Testing  #
    #############

    def test_begin(self, file_name='file://test_case_path'):
        b_file_name = bytify(file_name)
        self._song = c.moose_song_new()
        c.moose_song_set_uri(self._song, file_name)

    def test_feed(self, name, value):
        pass

    ################
    #  Properties  #
    ################

    cdef _tag(self, c.MooseTagType tag):
        cdef c.const_char_ptr p
        p = c.moose_song_get_tag(self._p(), tag)
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
        'Retrieve the uri (file://xyz e.g.) of this song.'
        def __get__(self):
            return stringify(<char *>c.moose_song_get_uri(self._p()))

    property duration:
        'Retrieve the duration of the song in seconds'
        def __get__(self):
            return c.moose_song_get_duration(self._p())

    property queue_pos:
        'Retrieve the position of the song in the Queue (or 0 if in DB only)'
        def __get__(self):
            return <int>c.moose_song_get_pos(self._p())
        def __set__(self, int pos):
            c.moose_song_set_pos(self._p(), pos)

    property queue_id:
        '''
        Retrieve the id of the song in the Queue (or 0 if in DB only)

        In contrast to the Position the Id won't change on move/add/delete.
        '''
        def __get__(self):
            return <int>c.moose_song_get_id(self._p())

    property last_modified:
        '''
        Get the date when this song's file file was last modified.

        :returns: a time.struct_time
        '''
        def __get__(self):
            return time.gmtime(c.moose_song_get_last_modified(self._p()))
