cimport binds as c


cdef class StoredPlaylist(Playlist):
        '''
        Like Playlist, but models one of MPD's stored Playlist.
        Therefore it adds methods to manipulate the playlist.

        All methods trigger the Idle.STORED_PLAYLIST event.
        '''
        cdef c.mpd_playlist *_playlist

        cdef c.mpd_playlist * _pl(self) except NULL:
            if self._playlist != NULL:
                return self._playlist
            else:
                raise ValueError('mpd_playlist pointer instance is empty - this should not happen')

        cdef char * name(self):
            return <char *>c.mpd_playlist_get_path(self._pl())

        cdef _init_stored_playlist(self, c.mc_Stack *stack, c.mc_Client *client, c.mpd_playlist *playlist):
            self._playlist = playlist
            return Playlist._init(self, stack, client)

        ################
        #  properties  #
        ################

        property name:
            'Get the name of the stored Playlist.'
            def __get__(self):
                return stringify(<char*>c.mpd_playlist_get_path(self._pl()))

        property last_modified:
            'The date of the last modification.'
            def __get__(self):
                return c.mpd_playlist_get_last_modified(self._pl())
