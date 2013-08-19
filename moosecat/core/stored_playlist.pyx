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

        ######################
        #  Actual Functions  #
        ######################

        def add(self, song):
            '''
            Add the song the playlist.

            :song: The song to add.
            '''
            client_send(self._c(), 'playlist_add {name} {uri}'.format(
                name=self.name(), uri=song.uri
            ))

        def clear(self):
            'Clear the playlist.'
            client_send(self._c(), 'playlist_clear {name}'.format(name=self.name()))

        def delete(self, song):
            '''
            Delete a song from the playlist.

            :song: The song to delete.
            '''
            client_send(self._c(), 'playlist_delete {name} {uri}'.format(
                name=self.name(), uri=song.uri
            ))

        def move(self, song, offset=1):
            '''
            Move the song in the Playlist.

            :song: The song to move.
            :offset: Offset to move the song.
            '''
            client_send(self._c(), 'playlist_move {name} {old_pos} {new_pos}'.format(
                name=self.name(), old_pos=song.queue_pos, new_pos=song.queue_pos + offset
            ))

        def rename(self, new_plname):
            '''
            Rename the stored playlist.

            :new_plname: New name of the Playlist.

            Note: Do not use this Playlist Wrapper after rename().
                  The internal name is not updated.
            '''
            client_send(self._c(), 'playlist_rename {old_name} {new_name}'.format(
                old_name=self.name(), new_name=new_plname
            ))

        def rm(self):
            'Remove the Stored Playlist fully.'
            client_send(self._c(), 'playlist_rm {name}'.format(name=self.name()))

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
