from libc.stdint cimport uint32_t, uint8_t
from libcpp cimport bool

# Too lazy to lookup how to import it properly.
ctypedef long time_t


###########################################################################
#                              libmpdclient                               #
###########################################################################

cdef extern from "mpd/client.h":
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
cdef extern from "../../lib/mpd/protocol.h":

    ctypedef enum mc_PmType:
        PM_IDLE 'MC_PM_IDLE'
        PM_COMMAND 'MC_PM_COMMAND'

    ctypedef enum mc_LogLevel:
        LOG_CRITICAL 'MC_LOG_CRITICAL'
        LOG_ERROR    'MC_LOG_ERROR'
        LOG_WARNING  'MC_LOG_WARNING'
        LOG_INFO     'MC_LOG_INFO'
        LOG_DEBUG    'MC_LOG_DEBUG'

    ################
    #  Structures  #
    ################

    ctypedef struct mc_Client:
        pass

    #############
    #  Methods  #
    #############

    # Network:
    mc_Client *mc_create(mc_PmType)
    char *mc_connect( mc_Client *, void *, const char *, int, float)
    bool mc_is_connected(mc_Client *)
    char *mc_disconnect(mc_Client *)
    void mc_free(mc_Client *)

    # These two shall remain hidden, so the Python layer
    # is uanableto screw with the connection.
    # mpd_connection *mc_get(mc_Client *)
    # void mc_put(mc_Client *)

    # Signals:
    void mc_signal_add(mc_Client *, const char *, void *, void *)
    void mc_signal_add_masked(mc_Client *, const char *, void *, void *, mpd_idle )
    void mc_signal_rm(mc_Client *, const char *, void *)
    void mc_signal_dispatch(mc_Client *, const char *, ...)
    int mc_signal_length(mc_Client *, const char *)

    # Meta:
    void mc_force_sync(mc_Client *, mpd_idle events)
    const char *mc_get_host(mc_Client *)
    unsigned mc_get_port(mc_Client *)
    float mc_get_timeout(mc_Client *)

    # Status Timer
    void mc_status_timer_register(mc_Client *, int, bool)
    void mc_status_timer_unregister(mc_Client *)
    bool mc_status_timer_is_active(mc_Client *)

    # Outputs:
    bool mc_outputs_get_state(mc_Client *, const char *)
    const char ** mc_outputs_get_names(mc_Client *)
    bool mc_outputs_set_state(mc_Client *, const char *, bool)

    # Locking:
    mpd_status * mc_lock_status(mc_Client *)
    mpd_stats * mc_lock_statistics(mc_Client *)
    mpd_song * mc_lock_current_song(mc_Client *)
    const char * mc_get_replay_gain_mode(mc_Client *)
    void mc_unlock_status(mc_Client *)
    void mc_unlock_statistics(mc_Client *)
    void mc_unlock_current_song(mc_Client *)
    void mc_lock_outputs(mc_Client *)
    void mc_unlock_outputs(mc_Client *)
    void mc_block_till_sync(mc_Client *) nogil


###########################################################################
#                             Client Commands                             #
###########################################################################

cdef extern from "../../lib/mpd/client.h":
    void mc_client_init(mc_Client *)
    void mc_client_destroy(mc_Client *)
    long mc_client_send(mc_Client *, const char *)
    bool mc_client_recv(mc_Client *, long) nogil
    bool mc_client_run(mc_Client *, const char *) nogil
    bool mc_client_command_list_is_active(mc_Client *)
    void mc_client_wait(mc_Client *) nogil
    void mc_client_begin(mc_Client *)
    long mc_client_commit(mc_Client *)


###########################################################################
#                                Database                                 #
###########################################################################

cdef extern from "../../lib/store/stack.h":
    ctypedef struct mc_Stack:
        pass

    mc_Stack * mc_stack_create (long, void *)
    void mc_stack_append (mc_Stack *, void *)
    void mc_stack_free (mc_Stack *)
    void mc_stack_clear (mc_Stack *)
    unsigned mc_stack_length (mc_Stack *)
    void mc_stack_sort (mc_Stack *, void *)
    void * mc_stack_at(mc_Stack *, unsigned)


cdef extern from "../../lib/store/db-settings.h":
    ctypedef struct mc_StoreSettings:
        bool use_memory_db
        bool use_compression
        char *db_directory
        char *tokenizer

    mc_StoreSettings * mc_store_settings_new ()
    void mc_store_settings_destroy (mc_StoreSettings *)
    void mc_store_settings_set_db_directory(mc_StoreSettings *, const char *)


cdef extern from "../../lib/store/db.h":
    ctypedef struct mc_Store:
        pass


    mc_Store *mc_store_create(mc_Client *client, mc_StoreSettings *settings)
    void mc_store_close(mc_Store *)
    mpd_song *mc_store_song_at(mc_Store *, int)
    int mc_store_total_songs(mc_Store *)
    long mc_store_playlist_load(mc_Store *, const char *)
    long mc_store_playlist_select_to_stack(mc_Store *, mc_Stack *, const char *, const char *)
    long mc_store_dir_select_to_stack(mc_Store *, mc_Stack *, const char *, int)
    long mc_store_playlist_get_all_loaded(mc_Store *, mc_Stack *)
    long mc_store_playlist_get_all_known(mc_Store *, mc_Stack *)
    long mc_store_search_to_stack(mc_Store *, const char *, bool, mc_Stack *, int)
    void mc_store_wait(mc_Store *) nogil
    void mc_store_wait_for_job(mc_Store *, int) nogil
    mc_Stack *mc_store_get_result(mc_Store *, int)
    mc_Stack *mc_store_gw(mc_Store *, int)
    void mc_store_release(mc_Store *)


cdef extern from "../../lib/store/db-query-parser.h":
    char *mc_store_qp_parse(char *, char **, int *)

###########################################################################
#                             Misc Interfaces                             #
###########################################################################

cdef extern from "../../lib/misc/bug-report.h":
    char * mc_misc_bug_report (mc_Client * client)

cdef extern from "../../lib/misc/posix-signal.h":
    void mc_misc_register_posix_signal (mc_Client * client)

cdef extern from "../../lib/misc/external-logs.h":
    void mc_misc_catch_external_logs(mc_Client *)

cdef extern from "../../lib/misc/zeroconf.h":
    ctypedef struct mc_ZeroconfBrowser:
        pass

    ctypedef struct mc_ZeroconfServer:
        pass

    ctypedef enum mc_ZeroconfState:
        ZEROCONF_STATE_UNCONNECTED 'MC_ZEROCONF_STATE_UNCONNECTED'
        ZEROCONF_STATE_CONNECTED   'MC_ZEROCONF_STATE_CONNECTED'
        ZEROCONF_STATE_ERROR       'MC_ZEROCONF_STATE_ERROR'
        ZEROCONF_STATE_CHANGED     'MC_ZEROCONF_STATE_CHANGED'
        ZEROCONF_STATE_ALL_FOR_NOW 'MC_ZEROCONF_STATE_ALL_FOR_NOW'

    mc_ZeroconfBrowser * mc_zeroconf_new(const char *)
    void mc_zeroconf_destroy(mc_ZeroconfBrowser *)
    void mc_zeroconf_register(mc_ZeroconfBrowser *, void *, void *)
    mc_ZeroconfState mc_zeroconf_get_state(mc_ZeroconfBrowser *)
    const char * mc_zeroconf_get_error(mc_ZeroconfBrowser *)
    mc_ZeroconfServer ** mc_zeroconf_get_server(mc_ZeroconfBrowser *)
    const char * mc_zeroconf_server_get_host(mc_ZeroconfServer *)
    const char * mc_zeroconf_server_get_addr(mc_ZeroconfServer *)
    const char * mc_zeroconf_server_get_name(mc_ZeroconfServer *)
    const char * mc_zeroconf_server_get_type(mc_ZeroconfServer *)
    const char * mc_zeroconf_server_get_domain(mc_ZeroconfServer *)
    unsigned mc_zeroconf_server_get_port(mc_ZeroconfServer *)


cdef extern from "../../lib/config.h":
    enum:
        VERSION               'MC_VERSION'
        VERSION_MAJOR         'MC_VERSION_MAJOR'
        VERSION_MINOR         'MC_VERSION_MINOR'
        VERSION_PATCH         'MC_VERSION_PATCH'
        VERSION_GIT_REVISION  'MC_VERSION_GIT_REVISION'
