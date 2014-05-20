from libc.stdint cimport uint32_t, uint8_t
from libcpp cimport bool

# Too lazy to lookup how to import it properly.
ctypedef long time_t

###########################################################################
#                              libmpdclient                               #
###########################################################################

cdef extern from "../../lib/mpd/moose-song.h":
    ctypedef struct MooseSong:
        pass

    # Wrapped as properties:
    ctypedef enum MooseTagType:
        MOOSE_TAG_UNKNOWN
        MOOSE_TAG_ARTIST
        MOOSE_TAG_ALBUM
        MOOSE_TAG_ALBUM_ARTIST
        MOOSE_TAG_TITLE
        MOOSE_TAG_TRACK
        MOOSE_TAG_NAME
        MOOSE_TAG_GENRE
        MOOSE_TAG_DATE
        MOOSE_TAG_COMPOSER
        MOOSE_TAG_PERFORMER
        MOOSE_TAG_COMMENT
        MOOSE_TAG_DISC
        MOOSE_TAG_OPTIONAL_COUNT
        MOOSE_TAG_MUSICBRAINZ_ARTISTID
        MOOSE_TAG_MUSICBRAINZ_ALBUMID
        MOOSE_TAG_MUSICBRAINZ_ALBUMARTISTID
        MOOSE_TAG_MUSICBRAINZ_TRACKID
        MOOSE_TAG_COUNT

    MooseSong * moose_song_new()
    void moose_song_unref(MooseSong *)
    char * moose_song_get_tag(MooseSong *, MooseTagType tag)
    void moose_song_set_tag(MooseSong *, MooseTagType tag, char *)
    const char * moose_song_get_uri(MooseSong *)
    void moose_song_set_uri(MooseSong *, const char *)
    unsigned moose_song_get_duration(MooseSong *)
    void moose_song_set_duration(MooseSong *, unsigned)
    time_t moose_song_get_last_modified(MooseSong *)
    void moose_song_set_last_modified(MooseSong *, time_t)
    unsigned moose_song_get_pos(MooseSong *)
    void moose_song_set_pos(MooseSong *, unsigned)
    unsigned moose_song_get_id(MooseSong *)
    void moose_song_set_id(MooseSong *, unsigned)
    unsigned moose_song_get_prio(MooseSong *)
    void moose_song_set_prio(MooseSong *, unsigned)

cdef extern from "../../lib/mpd/moose-status.h":
    ctypedef struct MooseStatus:
        pass

    # Wrapped as class variables
    ctypedef enum MooseState:
        MOOSE_STATE_UNKNOWN
        MOOSE_STATE_STOP
        MOOSE_STATE_PLAY
        MOOSE_STATE_PAUSE

    int moose_status_get_volume(MooseStatus *)
    bool moose_status_get_repeat(MooseStatus *)
    bool moose_status_get_random(MooseStatus *)
    bool moose_status_get_single(MooseStatus *)
    bool moose_status_get_consume(MooseStatus *)
    unsigned moose_status_get_queue_length(MooseStatus *)
    unsigned moose_status_get_queue_version(MooseStatus *)
    MooseState moose_status_get_state(MooseStatus *)
    unsigned moose_status_get_crossfade(MooseStatus *)
    float moose_status_get_mixrampdb(MooseStatus *)
    float moose_status_get_mixrampdelay(MooseStatus *)
    int moose_status_get_song_pos(MooseStatus *)
    int moose_status_get_song_id(MooseStatus *)
    int moose_status_get_next_song_pos(MooseStatus *)
    int moose_status_get_next_song_id(MooseStatus *)
    unsigned moose_status_get_elapsed_time(MooseStatus *)
    unsigned moose_status_get_elapsed_ms(MooseStatus *)
    unsigned moose_status_get_total_time(MooseStatus *)
    unsigned moose_status_get_kbit_rate(MooseStatus *)
    unsigned moose_status_get_update_id(MooseStatus *)
    char * moose_status_get_error(MooseStatus *)
    unsigned moose_status_get_audio_sample_rate(MooseStatus *)
    int moose_status_get_audio_bits(MooseStatus *)
    int moose_status_get_audio_channels(MooseStatus *)
    char * moose_status_get_replay_gain_mode(MooseStatus *)
    void moose_status_free(MooseStatus * status)


cdef extern from "../../lib/mpd/moose-mpd-client.h":
    ################
    #  Structures  #
    ################

    # So, we just typedef const here
    ctypedef char * const_char_ptr "const char *"

    cdef struct mpd_stats:
        pass

    cdef struct mpd_status:
        pass

    cdef struct mpd_output:
        pass

    cdef struct mpd_playlist:
        pass

    # This is hidden to prevent misuse.
    # cdef struct mpd_connection:
    #    pass

    # Wrapped as properties:
    cdef struct mpd_audio_format:
        uint32_t sample_rate
        uint8_t bits
        uint8_t channels

    cdef struct mpd_pair:
        char * name
        char * value

    ###########
    #  Enums  #
    ###########

    cdef enum mpd_idle:
        MPD_IDLE_DATABASE
        MPD_IDLE_STORED_PLAYLIST
        MPD_IDLE_QUEUE
        MPD_IDLE_PLAYLIST
        MPD_IDLE_PLAYER
        MPD_IDLE_MIXER
        MPD_IDLE_OUTPUT
        MPD_IDLE_OPTIONS
        MPD_IDLE_UPDATE
        MPD_IDLE_STICKER
        MPD_IDLE_SUBSCRIPTION
        MPD_IDLE_MESSAGE
        MPD_IDLE_SEEK = MPD_IDLE_MESSAGE << 1

    cdef enum mpd_error:
        MPD_ERROR_SUCCESS
        MPD_ERROR_OOM
        MPD_ERROR_ARGUMENT
        MPD_ERROR_STATE
        MPD_ERROR_TIMEOUT
        MPD_ERROR_SYSTEM
        MPD_ERROR_RESOLVER
        MPD_ERROR_MALFORMED
        MPD_ERROR_CLOSED
        MPD_ERROR_SERVER

    cdef enum mpd_idle:
        MPD_IDLE_DATABASE
        MPD_IDLE_STORED_PLAYLIST
        MPD_IDLE_QUEUE
        MPD_IDLE_PLAYLIST
        MPD_IDLE_PLAYER
        MPD_IDLE_MIXER
        MPD_IDLE_OUTPUT
        MPD_IDLE_OPTIONS
        MPD_IDLE_UPDATE
        MPD_IDLE_STICKER
        MPD_IDLE_SUBSCRIPTION
        MPD_IDLE_MESSAGE


    ##########################
    #  Statistics Functions  #
    ##########################

    unsigned mpd_stats_get_number_of_artists(mpd_stats *)
    unsigned mpd_stats_get_number_of_albums(mpd_stats *)
    unsigned mpd_stats_get_number_of_songs(mpd_stats *)
    unsigned long mpd_stats_get_uptime(mpd_stats *)
    unsigned long mpd_stats_get_db_update_time(mpd_stats *)
    unsigned long mpd_stats_get_play_time(mpd_stats *)
    unsigned long mpd_stats_get_db_play_time(mpd_stats *)

    # Useful for testing
    mpd_stats * mpd_stats_begin()
    void mpd_stats_feed(mpd_stats *status, mpd_pair *pair)
    void mpd_stats_free(mpd_stats * stats)

    ######################
    #  Output Functions  #
    ######################

    bool mpd_output_get_enabled(mpd_output *)
    int mpd_output_get_id(mpd_output *)
    char * mpd_output_get_name(mpd_output *)

    #######################
    #  Playlist Function  #
    #######################

    char * mpd_playlist_get_path(mpd_playlist *)
    time_t mpd_playlist_get_last_modified(mpd_playlist *)
    mpd_playlist * mpd_playlist_begin(mpd_pair *)


###########################################################################
#                             Main Interface                              #
###########################################################################

# ../../ is relative to the cython'd file.
# The file is written to build/cython/moose.h
# So it needs to be two levels up.
cdef extern from "../../lib/mpd/moose-mpd-protocol.h":

    ctypedef enum MoosePmType:
        PM_IDLE 'MOOSE_PM_IDLE'
        PM_COMMAND 'MOOSE_PM_COMMAND'

    ctypedef enum MooseLogLevel:
        LOG_CRITICAL 'MOOSE_LOG_CRITICAL'
        LOG_ERROR    'MOOSE_LOG_ERROR'
        LOG_WARNING  'MOOSE_LOG_WARNING'
        LOG_INFO     'MOOSE_LOG_INFO'
        LOG_DEBUG    'MOOSE_LOG_DEBUG'

    ################
    #  Structures  #
    ################

    ctypedef struct MooseClient:
        pass

    #############
    #  Methods  #
    #############

    # Network:
    MooseClient *moose_create(MoosePmType)
    char *moose_connect( MooseClient *, void *, const char *, int, float)
    bool moose_is_connected(MooseClient *)
    char *moose_disconnect(MooseClient *)
    void moose_free(MooseClient *)

    # These two shall remain hidden, so the Python layer
    # is uanableto screw with the connection.
    # mpd_connection *moose_get(MooseClient *)
    # void moose_put(MooseClient *)

    # Signals:
    void moose_signal_add(MooseClient *, const char *, void *, void *)
    void moose_signal_add_masked(MooseClient *, const char *, void *, void *, mpd_idle )
    void moose_signal_rm(MooseClient *, const char *, void *)
    void moose_signal_dispatch(MooseClient *, const char *, ...)
    int moose_signal_length(MooseClient *, const char *)

    # Meta:
    void moose_force_sync(MooseClient *, mpd_idle events)
    const char *moose_get_host(MooseClient *)
    unsigned moose_get_port(MooseClient *)
    float moose_get_timeout(MooseClient *)

    # Status Timer
    void moose_status_timer_register(MooseClient *, int, bool)
    void moose_status_timer_unregister(MooseClient *)
    bool moose_status_timer_is_active(MooseClient *)

    # Outputs:
    bool moose_outputs_get_state(MooseClient *, const char *)
    const char ** moose_outputs_get_names(MooseClient *)
    bool moose_outputs_set_state(MooseClient *, const char *, bool)

    # Locking:
    MooseStatus * moose_ref_status(MooseClient *)
    mpd_stats * moose_lock_statistics(MooseClient *)
    MooseSong * moose_lock_current_song(MooseClient *)
    void moose_unlock_status(MooseClient *)
    void moose_unlock_statistics(MooseClient *)
    void moose_unlock_current_song(MooseClient *)
    void moose_lock_outputs(MooseClient *)
    void moose_unlock_outputs(MooseClient *)
    void moose_block_till_sync(MooseClient *) nogil


###########################################################################
#                             Client Commands                             #
###########################################################################

cdef extern from "../../lib/mpd/moose-mpd-client.h":
    void moose_client_init(MooseClient *)
    void moose_client_destroy(MooseClient *)
    long moose_client_send(MooseClient *, const char *)
    bool moose_client_recv(MooseClient *, long) nogil
    bool moose_client_run(MooseClient *, const char *) nogil
    bool moose_client_command_list_is_active(MooseClient *)
    void moose_client_wait(MooseClient *) nogil
    void moose_client_begin(MooseClient *)
    long moose_client_commit(MooseClient *)


###########################################################################
#                                Database                                 #
###########################################################################

cdef extern from "../../lib/store/moose-store-playlist.h":
    ctypedef struct MoosePlaylist:
        pass

    void g_object_unref(void *)
    void g_signal_connect(void *, char *, void *,  void *)

    MoosePlaylist * moose_playlist_new()
    MoosePlaylist * moose_playlist_new_full(long, void *)
    void moose_playlist_append (MoosePlaylist *, void *)
    void moose_playlist_clear (MoosePlaylist *)
    unsigned moose_playlist_length (MoosePlaylist *)
    void moose_playlist_sort (MoosePlaylist *, void *)
    void * moose_playlist_at(MoosePlaylist *, unsigned)


cdef extern from "../../lib/store/moose-store-settings.h":
    ctypedef struct MooseStoreSettings:
        bool use_memory_db
        bool use_compression
        char *db_directory
        char *tokenizer

    MooseStoreSettings * moose_store_settings_new ()
    void moose_store_settings_destroy (MooseStoreSettings *)
    void moose_store_settings_set_db_directory(MooseStoreSettings *, const char *)


cdef extern from "../../lib/store/moose-store.h":
    ctypedef struct MooseStore:
        pass

    MooseStore *moose_store_create(MooseClient *client, MooseStoreSettings *settings)
    void moose_store_close(MooseStore *)
    MooseSong *moose_store_song_at(MooseStore *, int)
    int moose_store_total_songs(MooseStore *)
    long moose_store_playlist_load(MooseStore *, const char *)
    long moose_store_playlist_select_to_stack(MooseStore *, MoosePlaylist *, const char *, const char *)
    long moose_store_dir_select_to_stack(MooseStore *, MoosePlaylist *, const char *, int)
    long moose_store_playlist_get_all_loaded(MooseStore *, MoosePlaylist *)
    long moose_store_playlist_get_all_known(MooseStore *, MoosePlaylist *)
    long moose_store_search_to_stack(MooseStore *, const char *, bool, MoosePlaylist *, int)
    void moose_store_wait(MooseStore *) nogil
    void moose_store_wait_for_job(MooseStore *, int) nogil
    MoosePlaylist *moose_store_get_result(MooseStore *, int)
    MoosePlaylist *moose_store_gw(MooseStore *, int)
    void moose_store_release(MooseStore *)
    MooseSong *moose_store_find_song_by_id(MooseStore *, unsigned)
    MooseStoreCompletion *moose_store_get_completion(MooseStore*)

cdef extern from "../../lib/store/moose-store-query-parser.h":
    ctypedef struct MooseStoreCompletion:
        pass

    MooseStoreCompletion * moose_store_cmpl_new(MooseStore *)
    void moose_store_cmpl_free(MooseStoreCompletion *)
    char * moose_store_cmpl_lookup(MooseStoreCompletion *, MooseTagType, char *)

cdef extern from "../../lib/store/moose-store-query-parser.h":
    char *moose_store_qp_parse(char *, char **, int *)
    char *moose_store_qp_tag_abbrev_to_full(char *, size_t)
    bool moose_store_qp_is_valid_tag(char *, size_t)
    MooseTagType moose_store_qp_str_to_tag_enum(char *)

###########################################################################
#                             Misc Interfaces                             #
###########################################################################

cdef extern from "../../lib/misc/moose-misc-external-logs.h":
    void moose_misc_catch_external_logs(MooseClient *)

cdef extern from "../../lib/misc/moose-misc-metadata-threads.h":
    ctypedef struct MooseMetadataThreads:
        pass

    MooseMetadataThreads * moose_mdthreads_new(void *, void *, void *, int)
    void moose_mdthreads_push(MooseMetadataThreads *, void *)
    void moose_mdthreads_forward(MooseMetadataThreads *, void *)
    void moose_mdthreads_free(MooseMetadataThreads *)

cdef extern from "../../lib/misc/moose-misc-zeroconf.h":
    ctypedef struct MooseZeroconfBrowser:
        pass

    ctypedef struct MooseZeroconfServer:
        pass

    cdef enum MooseZeroconfState:
        ZEROCONF_STATE_UNCONNECTED 'MOOSE_ZEROCONF_STATE_UNCONNECTED'
        ZEROCONF_STATE_CONNECTED   'MOOSE_ZEROCONF_STATE_CONNECTED'
        ZEROCONF_STATE_ERROR       'MOOSE_ZEROCONF_STATE_ERROR'
        ZEROCONF_STATE_CHANGED     'MOOSE_ZEROCONF_STATE_CHANGED'
        ZEROCONF_STATE_ALL_FOR_NOW 'MOOSE_ZEROCONF_STATE_ALL_FOR_NOW'

    MooseZeroconfBrowser * moose_zeroconf_browser_new()
    void moose_zeroconf_browser_destroy(MooseZeroconfBrowser *)
    MooseZeroconfState moose_zeroconf_browser_get_state(MooseZeroconfBrowser *)
    const char * moose_zeroconf_browser_get_error(MooseZeroconfBrowser *)
    MooseZeroconfServer ** moose_zeroconf_browser_get_server(MooseZeroconfBrowser *)
    char * moose_zeroconf_server_get_host(MooseZeroconfServer *)
    char * moose_zeroconf_server_get_addr(MooseZeroconfServer *)
    char * moose_zeroconf_server_get_name(MooseZeroconfServer *)
    char * moose_zeroconf_server_get_protocol(MooseZeroconfServer *)
    char * moose_zeroconf_server_get_domain(MooseZeroconfServer *)
    unsigned moose_zeroconf_server_get_port(MooseZeroconfServer *)


cdef extern from "../../lib/moose-config.h":
    enum:
        VERSION               'MOOSE_VERSION'
        VERSION_MAJOR         'MOOSE_VERSION_MAJOR'
        VERSION_MINOR         'MOOSE_VERSION_MINOR'
        VERSION_PATCH         'MOOSE_VERSION_PATCH'
        VERSION_GIT_REVISION  'MOOSE_VERSION_GIT_REVISION'


