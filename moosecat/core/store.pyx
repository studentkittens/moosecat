cimport binds as c

# bool type
from libcpp cimport bool

'''
The Store.

libmoosecat offers a very convenient way to access stored songs.
Before any songs can be accessed, the Store must be initialized.
You can do this by calling the 'load()' method. This might take
some time to build the index though. You will not need to instance
the Store yourself, there is one attached to every Client object::

    client = Client()
    client.connect()
    client.store.load()
    ...

Stored Playlists
----------------

Stored Playlists (SPL) are stored too. You have to load a certain playlist first
though. You can get a list of avaible playlist-names via the playlist_names
property. It will give you a list of strings.

'''


'''
    mc_StoreDB * mc_store_create (mc_Client *, mc_StoreSettings *)
    void mc_store_close ( mc_StoreDB *)
    void mc_store_set_wait_mode (mc_StoreDB *, bool)

    mpd_song * mc_store_song_at (mc_StoreDB *, int)
    int mc_store_total_songs (mc_StoreDB *)
    void mc_store_playlist_load (mc_StoreDB *,char *)
    int mc_store_playlist_select_to_stack (mc_StoreDB *, mc_Stack *, char *, char *)
    int mc_store_dir_select_to_stack (mc_StoreDB *, mc_Stack *,char *, int)
    int mc_store_playlist_get_all_loaded (mc_StoreDB *, mc_Stack *)
    int mc_store_search_to_stack (mc_StoreDB *, char *, bool, mc_Stack *, int)
'''


cdef class Store:
    cdef c.mc_StoreDB * _db
    cdef c.mc_StoreSettings *_settings
    cdef c.mc_Client *_client
    cdef bool _wait_mode
    cdef bool _initialized

    cdef _init(self, c.mc_Client * client, db_directory, use_memory_db=True, use_compression=True, tokenizer=None):
        self._client = client
        self._settings = c.mc_store_settings_new ()
        self._db = NULL
        self._initialized = False
        self._wait_mode = False
        return self

    cdef c.mc_StoreDB * _p(self) except NULL:
        if self._db != NULL:
            return self._db
        else:
            raise ValueError('pointer instance is empty - this should not happen')

    def __dealloc__(self):
        self.close()
        c.mc_store_settings_destroy (self._settings)

    def load(self):
        # Before any load we should make sure, it's not running already.
        # Might leak massive amounts of memory otherwise.
        if self._initialized:
            self.close()

        self._db = c.mc_store_create (self._client, self._settings)
        self._initialized = (self._db != NULL)

    def close(self):
        if self._initialized is True:
            self.wait()
            c.mc_store_close(self._p())
        self._initialized = False

    def wait(self):
        cdef c.mc_StoreDB * p = self._p()
        with nogil:
            c.mc_store_wait(p)

    def load_stored_playlist(self, name):
        '''
        Load a stored playlist by it's name, and return a Playlist object.
        If the playlist is already loaded, the existing one is returned.

        :name: A playlist name (as given by stored_playlist_names)
        :returns: a Playlist object filled with the contents of the stored playlist.
        '''
        b_name = bytify(name)
        c.mc_store_playlist_load(self._p(), b_name)

    ################
    #  Properties  #
    ################

    property wait_mode:
        '''
        Set/Get the wait-mode of this Store instance.
        The wait-mode tells calls either to wait (True) or to return
        without response immediately (False) on blocking operations.

        Already running calls are not affected.

        Will wait by default.
        '''
        def __set__(self, mode):
            c.mc_store_set_wait_mode (self._db, mode)

        def __get__(self):
            return c.mc_store_get_wait_mode (self._db)

    cdef _make_playlist_names(self, c.mc_Stack * stack):
            cdef int i = 0
            cdef int length = c.mc_stack_length(stack)

            names = []

            while i < length:
                b_name = <char*>c.mpd_playlist_get_path(<c.mpd_playlist*>c.mc_stack_at(stack, i))
                names.append(stringify(b_name))
                i += 1

            return names

    property stored_playlist_names:
        '''
        Return a list of stored playlists names know by mpd.

        This property might change on the stored-playlist event.
        '''
        def __get__(self):
            cdef c.mc_Stack * pl_names = <c.mc_Stack *>c.mc_store_playlist_get_all_names(self._p())
            return self._make_playlist_names(pl_names)

    property loaded_stored_playlists:
        '''
        Return a list of Playlist objects, that are loaded by moosecat.

        This property might change on the stored-playlist event.
        '''
        def __get__(self):
            cdef c.mc_Stack * pl_names = c.mc_stack_create(10, NULL)
            c.mc_store_playlist_get_all_loaded(self._p(), pl_names)
            return self._make_playlist_names(pl_names)
