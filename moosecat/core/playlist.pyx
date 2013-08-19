cimport binds as c


cdef class Playlist:
    '''
    A Playlist is defined as a set of Songs.

    It implements the following behaviours:

        * ``__iter__`` - Iterate over all songs in the playlist.
        * ``__getitem__`` - Get a song at a specific position.
        * ``__len__`` - get the length of the playlist with ``len()``

    For those seeking implementation details: :class:`.Playlist` is a wrapper for ``mc_Stack``.
    '''
    cdef c.mc_Stack * _stack
    cdef c.mc_Client * _client

    cdef _init(self, c.mc_Stack * stack, c.mc_Client * client):
        self._stack = stack
        self._client = client
        return self

    cdef c.mc_Client * _c(self) except NULL:
        if self._client != NULL:
            return self._client
        else:
            raise ValueError('mc_Client pointer is null for this instance!')

    cdef c.mc_Stack * _p(self) except NULL:
        if self._stack != NULL:
            return self._stack
        else:
            raise ValueError('mc_Stack pointer is null for this instance!')

    def __iter__(self):
        def iterator():
            cdef int length = c.mc_stack_length(self._p())
            for i in range(length):
                yield self[i]

        return iterator()

    def __getitem__(self, idx):
        'Get a song at a certain position out of the Playlist'
        cdef int length = c.mc_stack_length(self._p())
        if idx < length:
            return song_from_ptr(<c.mpd_song*>c.mc_stack_at(self._p(), idx))
        else:
            raise IndexError('Index out of Playlist\'s range')

    def __len__(self):
        'Get the length of the Playlist'
        return c.mc_stack_length(self._p())
