cimport binds as c
from libcpp cimport bool
from libc.stdlib cimport free

# 'with' statement support
from contextlib import contextmanager

'''
The Store.

libmoosecat offers a very convenient way to access stored songs.
Before any songs can be accessed, the Store must be initialized.
You can do this by calling the 'load()' method. This might take
some time to build the index though. You will not need to instance
the Store yourself, there is one attached to every Client object::

    client = Client()
    client.connect()
    client.store_initialize('/tmp')
    client.store.do_something()
    ...

Stored Playlists
----------------

Stored Playlists (SPL) are stored too. You have to load a certain playlist first
though. You can get a list of avaible playlist-names via the playlist_names
property. It will give you a list of strings.

'''

cdef class Store:
    cdef c.mc_Store * _db
    cdef c.mc_StoreSettings * _settings
    cdef c.mc_Client * _client
    cdef bool _initialized

    cdef _init(self, c.mc_Client * client, db_directory, use_memory_db=True, use_compression=True, tokenizer=None):
        self._client = client

        # Configure Settings
        self._settings = c.mc_store_settings_new()
        self._settings.use_memory_db = use_memory_db
        self._settings.use_compression = use_compression

        b_db_directory = bytify(db_directory)
        c.mc_store_settings_set_db_directory(self._settings, b_db_directory)
        #self._settings.db_directory = b_db_directory
        if tokenizer is not None:
            b_tokenizer = bytify(tokenizer)
            self._settings.tokenizer = b_tokenizer

        self._db = NULL
        self._initialized = False
        return self

    cdef c.mc_Store * _p(self) except NULL:
        if self._db != NULL:
            return self._db
        else:
            raise ValueError('pointer instance is empty - this should not happen')

    cdef c.mc_Client * _c(self) except NULL:
        if self._client != NULL:
            return self._client
        else:
            raise ValueError('client pointer instance is empty - this should not happen')

    def __dealloc__(self):
        self.close()
        c.mc_store_settings_destroy(self._settings)

    def load(self):
        # Before any load we should make sure, it's not running already.
        # Might leak massive amounts of memory otherwise.
        if self._initialized:
            self.close()

        self._db = c.mc_store_create(self._c(), self._settings)
        self._initialized = (self._db != NULL)

    def close(self):
        if self._initialized is True:
            self.wait()
            c.mc_store_close(self._p())
        self._initialized = False

    def wait(self):
        '''
        Wait for the store to finish its current operation.

        Operations that will use the database will lock a mutex. wait() will
        try to lock this mutex and and wait till it is released.
        '''
        cdef c.mc_Store * p = self._p()
        with nogil:
            c.mc_store_wait(p)

    def complete(self, tag, prefix):
        '''
        Complete a string of a certan tag (e.g. artist).
        On the first call a radix-tree is created on the c-side as index
        for all values of this attributes.

        :param tag: A tag-string.
        :param prefix: The prefix to complete.
        '''
        cdef c.mc_StoreCompletion *completion = c.mc_store_get_completion(self._p())

        b_tag = bytify(tag)
        tag_id = c.mc_store_qp_str_to_tag_enum(b_tag)

        if tag_id is not c.MPD_TAG_UNKNOWN:
            if prefix:
                b_prefix = bytify(prefix)
                b_suggestion = c.mc_store_cmpl_lookup(completion, tag_id, b_prefix)
            else:
                c.mc_store_cmpl_lookup(completion, tag_id, NULL)
                return None

            if b_suggestion:
                suggestion = stringify(b_suggestion)
                free(b_suggestion)
                return suggestion

    @contextmanager
    def query(self, match_clause=None, queue_only=True, limit_length=-1):
        '''
        Query songs being in either the Database or in the Queue.

        :match_clause: a FTS match clause
        :queue_only: If True only the queue contents are searched, if False the whole Database
        :limit_length: limit the length of the return to a pos. number. < 0 means no limit.

        :returns: a Playlist object with the requested songs. None if nothing found.
        '''
        cdef c.mc_Stack * result = NULL
        cdef int job_id = 0

        # Preallocation strategy
        stack_size = 1000 if limit_length < 0 else limit_length
        result = c.mc_stack_create(stack_size, NULL)

        if result != NULL:
            try:
                b_match_clause = bytify(match_clause.strip())
                job_id = c.mc_store_search_to_stack(self._p(), b_match_clause, queue_only, result, limit_length)

                c.mc_store_wait_for_job(self._p(), job_id)

                yield Queue()._init(result, self._c())
            finally:
                c.mc_store_release(self._p())
        else:
            yield []

    @contextmanager
    def query_directories(self, path=None, depth=-1):
        '''
        Get a directory listing for a certain path and/or depth.

        MPD mantains a virtual filesystem that can be accessed by this function.
        You can select a path and a depth. The function will return the intersection of both.
        Consider the following Example-Tree: ::

            0   1   2   3  ←───────── Depth-Level.
            ↓   ↓   ↓   ↓
            /   ↓   ↓   ↓
            ├── music   ↓
            │   ├── finntroll
            │   │   └── fiskarens.mp3
            │   └── gv.mp3
            └── podcasts
                └── alternativlos
                    └── folge_1.ogg


        **Examples:** ::

            >>> store.query_directories(depth=0)
            [(None, 'music'), (None, 'podcasts')]
            >>> store.query_directories(depth=1)
            [(None, 'music/finntroll'), (<moosecat.core.Song>, 'music/gv.mp3'), (None, 'music/alternativlos')]
            >>> store.query_directories(path='*.mp3')
            [(<moosecat.core.Song>, 'music/finntroll/fiskaren.mp3'), (<moosecat.core.Song>, 'music/gv.mp3')]
            >>> store.query_directories(path='*.mp3', depth=3)
            [(<moosecat.core.Song>, 'music/finntroll/fiskaren.mp3')]
            >>> store.query_directories(path=None, depth=-1)
            []  # will always deliver an empty list.

        **Advices:**

            * When depth >= 0, and path is None: Do this to realize a filebrowser.
            * When depth >= 0, and path in not None: List a specific directory,
              with the directory path being passed to path.
            * If you want to search stuff use path with a depth of -1.

        :path: the path to select
        :depth: the depth of the directory requested
        :returns: A list of tuples. Each tuple contains a Song (or None if directory), and the full path.
        '''
        if path is None and depth < 0:
            return []

        stack = c.mc_stack_create(10, NULL)
        rlist = []

        if path is None:
            job_id = c.mc_store_dir_select_to_stack(self._p(), stack, NULL, depth)
        else:
            b_path = bytify(path) if path else None
            job_id = c.mc_store_dir_select_to_stack(self._p(), stack, b_path, depth)

        # Wait for the job to finish.
        c.mc_store_wait_for_job(self._p(), job_id)

        for idx in range(c.mc_stack_length(stack)):
            song_id, path = stringify(<char *>c.mc_stack_at(stack, idx)).split(':', maxsplit=1)

            # No error checking is done here - we rely on libmoosecat.
            song_id = int(song_id)
            if song_id < 0:
                # it is a directory
                result = (None, path)
            else:
                # it is a song
                result = (Song()._init(c.mc_store_song_at(self._p(), song_id)), path)
            rlist.append(result)

        c.mc_stack_free(stack)

        try:
            yield rlist
        finally:
            c.mc_store_release(self._p())

    cdef c.mpd_playlist * _stored_playlist_get_by_name(self, b_name):
        'Find a mpd_playlist by its name'
        cdef c.mpd_playlist * result = NULL
        cdef c.mc_Stack * stack = c.mc_stack_create(10, NULL)

        try:
            job_id = c.mc_store_playlist_get_all_loaded(self._p(), stack)
            c.mc_store_wait_for_job(self._p(), job_id)
        finally:
            c.mc_store_release(self._p())

        i = 0
        while i < c.mc_stack_length(stack):
            pl_name = <char * > c.mpd_playlist_get_path(<c.mpd_playlist * > c.mc_stack_at(stack, i))
            if b_name == pl_name:
                result = <c.mpd_playlist * > c.mc_stack_at(stack, i)
                break
            i += 1

        c.mc_stack_free(stack)
        return result

    def stored_playlist_load(self, name):
        '''
        Load a stored playlist by it's name, and return a Playlist object.
        If the playlist is already loaded, the existing one is returned.

        :name: A playlist name (as given by stored_playlist_names)
        '''
        if name not in self.stored_playlist_names:
            raise ValueError('No such playlist: ' + name)

        b_name = bytify(name)
        c.mc_store_playlist_load(self._p(), b_name)

    @contextmanager
    def stored_playlist_query(self, name, match_clause=None):
        '''
        Search in a stored playlist with a match_clause.

        :name: the name of a stored playlist
        :match_clause: a fts match clause
        :returns: a Playlist with the specified songs.
        '''
        # Create a buffer for the songs
        cdef c.mc_Stack * stack = c.mc_stack_create(0, NULL)
        cdef c.mpd_playlist * playlist = NULL

        if stack != NULL:
            # Convert input to char * compatible bytestrings
            b_name = bytify(name)
            if match_clause is not None:
                b_match_clause = bytify(match_clause)
            else:
                b_match_clause = b'*'

            playlist = self._stored_playlist_get_by_name(b_name)
            if playlist != NULL:
                try:
                    job_id = c.mc_store_playlist_select_to_stack(self._p(), stack,  b_name, b_match_clause)
                    c.mc_store_wait_for_job(self._p(), job_id)

                    # Encapuslate the stack into a playlist object and give it the user
                    yield StoredPlaylist()._init_stored_playlist(stack, self._c(), playlist)
                finally:
                    c.mc_store_release(self._p())
            else:
                raise ValueError('No such loaded stored playlist with this name: ' + name)

    # This is private to prevent bad people from screwing up the locking order.
    @contextmanager
    def find_song_by_id(self, needle_song_id):
        # Negative values trigger a conversion error (unsigned -> signed)
        needle_song_id = max(0, needle_song_id)
        try:
            song = song_from_ptr(c.mc_store_find_song_by_id(self._p(), needle_song_id))
            yield song
        finally:
            c.mc_store_release(self._p())

    ################
    #  Properties  #
    ################

    cdef _make_playlist_names(self, c.mc_Stack * stack):
            cdef int i = 0
            cdef int length = c.mc_stack_length(stack)

            names = []

            # Iterate over all C-Side mpd_playlists
            # and fill their name in the names list
            while i < length:
                b_name = <char * > c.mpd_playlist_get_path(
                        <c.mpd_playlist * ?> c.mc_stack_at(stack, i)
                )
                names.append(stringify(b_name))
                i += 1

            # Make only the names accessible to the user.
            return names

    property stored_playlist_names:
        '''
        Return a list of stored playlists names know by mpd.

        This property might change on the stored-playlist event.
        '''
        def __get__(self):
            cdef c.mc_Stack * pl_names = c.mc_stack_create(10, NULL)
            job_id = c.mc_store_playlist_get_all_known(self._p(), pl_names)
            c.mc_store_wait_for_job(self._p(), job_id)

            try:
                names = self._make_playlist_names(pl_names)
            finally:
                c.mc_store_release(self._p())
            return names

    property loaded_stored_playlists:
        '''
        Return a list of Playlist objects, that are loaded by moosecat.

        This property might change on the stored-playlist event.
        '''
        def __get__(self):
            cdef c.mc_Stack * pl_names = c.mc_stack_create(10, NULL)
            job_id = c.mc_store_playlist_get_all_loaded(self._p(), pl_names)
            c.mc_store_wait_for_job(self._p(), job_id)

            try:
                names = self._make_playlist_names(pl_names)
            finally:
                c.mc_store_release(self._p())
            return names

    property total_songs:
        'Get the number of total songs in the store'
        def __get__(self):
            return c.mc_store_total_songs(self._p())
