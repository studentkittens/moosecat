cimport binds as c

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
    cdef c.mc_StoreDB * _db = NULL
    cdef c.mc_StoreSettings *_settings = NULL

    def __cinit__(self, client, db_directory, use_memory_db=True, use_compression=True, tokenizer=None):
        self._settings = c.mc_store_settings_new ()
        self._db = c.mc_store_create (client, self._settings)
        self._wait_mode = False

    property wait_mode:
        def __set__(self, mode):
            self._wait_mode = mode
            mc_store_set_wait_mode (self._db, mode)

        def __get__(self):
            return self._wait_mode


    def close(self):
        mc_store_close(self._db)

    def __dealloc__(self):
        c.mc_store_settings_destroy (self._settings)
