from libc.stdint cimport uint32_t, uint8_t
from libcpp cimport bool

# Too lazy to lookup how to import it properly.
ctypedef long time_t


###########################################################################
#                              libmpdclient                               #
###########################################################################

cdef extern from "../../lib/mpd/moose-mpd-client.h":
    ################
    #  Structures  #
    ################

    cdef struct mpd_song:
        pass

    # Cython does not support const
    # So, we just typedef const here
    ctypedef mpd_song * const_mpd_song_ptr "const struct mpd_song *"
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

    # Wrapped as properties:
    cdef enum mpd_tag_type:
        MPD_TAG_UNKNOWN
        MPD_TAG_ARTIST
        MPD_TAG_ALBUM
        MPD_TAG_ALBUM_ARTIST
        MPD_TAG_TITLE
        MPD_TAG_TRACK
        MPD_TAG_NAME
        MPD_TAG_GENRE
        MPD_TAG_DATE
        MPD_TAG_COMPOSER
        MPD_TAG_PERFORMER
        MPD_TAG_COMMENT
        MPD_TAG_DISC
        MPD_TAG_MUSICBRAINZ_ARTISTID
        MPD_TAG_MUSICBRAINZ_ALBUMID
        MPD_TAG_MUSICBRAINZ_ALBUMARTISTID
        MPD_TAG_MUSICBRAINZ_TRACKID
        MPD_TAG_COUNT

    # Wrapped as class variables
    cdef enum mpd_state:
        MPD_STATE_UNKNOWN
        MPD_STATE_STOP
        MPD_STATE_PLAY
        MPD_STATE_PAUSE

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

    ######################
    #  Status Functions  #
    ######################

    int mpd_status_get_volume(mpd_status *)
    bool mpd_status_get_repeat(mpd_status *)
    bool mpd_status_get_random(mpd_status *)
    bool mpd_status_get_single(mpd_status *)
    bool mpd_status_get_consume(mpd_status *)
    unsigned mpd_status_get_queue_length(mpd_status *)
    unsigned mpd_status_get_queue_version(mpd_status *)
    mpd_state mpd_status_get_state(mpd_status *)
    unsigned mpd_status_get_crossfade(mpd_status *)
    float mpd_status_get_mixrampdb(mpd_status *)
    float mpd_status_get_mixrampdelay(mpd_status *)
    int mpd_status_get_song_pos(mpd_status *)
    int mpd_status_get_song_id(mpd_status *)
    int mpd_status_get_next_song_pos(mpd_status *)
    int mpd_status_get_next_song_id(mpd_status *)
    unsigned mpd_status_get_elapsed_time(mpd_status *)
    unsigned mpd_status_get_elapsed_ms(mpd_status *)
    unsigned mpd_status_get_total_time(mpd_status *)
    unsigned mpd_status_get_kbit_rate(mpd_status *)
    mpd_audio_format * mpd_status_get_audio_format(mpd_status *)
    unsigned mpd_status_get_update_id(mpd_status *)
    char * mpd_status_get_error(mpd_status *)

    # Useful for testing
    mpd_status * mpd_status_begin()
    void mpd_status_feed(mpd_status *status, mpd_pair *pair)
    void mpd_status_free(mpd_status * status)

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

    #####################
    #  Songs Functions  #
    #####################

    char * mpd_song_get_uri(mpd_song *)
    char * mpd_song_get_tag(mpd_song *,  mpd_tag_type, unsigned)
    unsigned mpd_song_get_duration(mpd_song *)
    unsigned mpd_song_get_start(mpd_song *)
    unsigned mpd_song_get_end(mpd_song *)
    time_t mpd_song_get_last_modified(mpd_song *)
    void mpd_song_set_pos(mpd_song *, unsigned)
    unsigned mpd_song_get_pos(mpd_song *)
    unsigned mpd_song_get_id(mpd_song *)
    void mpd_song_free(mpd_song*)

    # Useful for testing.
    mpd_song * mpd_song_begin(mpd_pair *pair)
    void mpd_song_feed(mpd_song *song, mpd_pair *pair)

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
        PM_IDLE 'MC_PM_IDLE'
        PM_COMMAND 'MC_PM_COMMAND'

    ctypedef enum MooseLogLevel:
        LOG_CRITICAL 'MC_LOG_CRITICAL'
        LOG_ERROR    'MC_LOG_ERROR'
        LOG_WARNING  'MC_LOG_WARNING'
        LOG_INFO     'MC_LOG_INFO'
        LOG_DEBUG    'MC_LOG_DEBUG'

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
    mpd_status * moose_lock_status(MooseClient *)
    mpd_stats * moose_lock_statistics(MooseClient *)
    mpd_song * moose_lock_current_song(MooseClient *)
    const char * moose_get_replay_gain_mode(MooseClient *)
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

    MoosePlaylist * moose_stack_create (long, void *)
    void moose_stack_append (MoosePlaylist *, void *)
    void moose_stack_free (MoosePlaylist *)
    void moose_stack_clear (MoosePlaylist *)
    unsigned moose_stack_length (MoosePlaylist *)
    void moose_stack_sort (MoosePlaylist *, void *)
    void * moose_stack_at(MoosePlaylist *, unsigned)


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
    mpd_song *moose_store_song_at(MooseStore *, int)
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
    mpd_song *moose_store_find_song_by_id(MooseStore *, unsigned)
    MooseStoreCompletion *moose_store_get_completion(MooseStore*)

cdef extern from "../../lib/store/moose-store-query-parser.h":
    ctypedef struct MooseStoreCompletion:
        pass

    MooseStoreCompletion * moose_store_cmpl_new(MooseStore *)
    void moose_store_cmpl_free(MooseStoreCompletion *)
    char * moose_store_cmpl_lookup(MooseStoreCompletion *, mpd_tag_type, char *)

cdef extern from "../../lib/store/moose-store-query-parser.h":
    char *moose_store_qp_parse(char *, char **, int *)
    char *moose_store_qp_tag_abbrev_to_full(char *, size_t)
    bool moose_store_qp_is_valid_tag(char *, size_t)
    mpd_tag_type moose_store_qp_str_to_tag_enum(char *)

###########################################################################
#                             Misc Interfaces                             #
###########################################################################

cdef extern from "../../lib/misc/bug-report.h":
    char * moose_misc_bug_report (MooseClient * client)

cdef extern from "../../lib/misc/external-logs.h":
    void moose_misc_catch_external_logs(MooseClient *)

cdef extern from "../../lib/misc/metadata-threads.h":
    ctypedef struct MooseMetadataThreads:
        pass

    MooseMetadataThreads * moose_mdthreads_new(void *, void *, void *, int)
    void moose_mdthreads_push(MooseMetadataThreads *, void *)
    void moose_mdthreads_forward(MooseMetadataThreads *, void *)
    void moose_mdthreads_free(MooseMetadataThreads *)

cdef extern from "../../lib/misc/zeroconf.h":
    cdef struct MooseZeroconfBrowser:
        pass

    cdef struct MooseZeroconfServer:
        pass

    cdef enum MooseZeroconfState:
        ZEROCONF_STATE_UNCONNECTED 'MC_ZEROCONF_STATE_UNCONNECTED'
        ZEROCONF_STATE_CONNECTED   'MC_ZEROCONF_STATE_CONNECTED'
        ZEROCONF_STATE_ERROR       'MC_ZEROCONF_STATE_ERROR'
        ZEROCONF_STATE_CHANGED     'MC_ZEROCONF_STATE_CHANGED'
        ZEROCONF_STATE_ALL_FOR_NOW 'MC_ZEROCONF_STATE_ALL_FOR_NOW'

    MooseZeroconfBrowser * moose_zeroconf_new(const char *)
    void moose_zeroconf_destroy(MooseZeroconfBrowser *)
    void moose_zeroconf_register(MooseZeroconfBrowser *, void *, void *)
    MooseZeroconfState moose_zeroconf_get_state(MooseZeroconfBrowser *)
    const char * moose_zeroconf_get_error(MooseZeroconfBrowser *)
    MooseZeroconfServer ** moose_zeroconf_get_server(MooseZeroconfBrowser *)
    const char * moose_zeroconf_server_get_host(MooseZeroconfServer *)
    const char * moose_zeroconf_server_get_addr(MooseZeroconfServer *)
    const char * moose_zeroconf_server_get_name(MooseZeroconfServer *)
    const char * moose_zeroconf_server_get_type(MooseZeroconfServer *)
    const char * moose_zeroconf_server_get_domain(MooseZeroconfServer *)
    unsigned moose_zeroconf_server_get_port(MooseZeroconfServer *)


cdef extern from "../../lib/config.h":
    enum:
        VERSION               'MC_VERSION'
        VERSION_MAJOR         'MC_VERSION_MAJOR'
        VERSION_MINOR         'MC_VERSION_MINOR'
        VERSION_PATCH         'MC_VERSION_PATCH'
        VERSION_GIT_REVISION  'MC_VERSION_GIT_REVISION'


