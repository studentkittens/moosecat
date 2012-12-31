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


    ctypedef enum mc_OpFinishedEnum:
        MC_OP_DB_UPDATED
        MC_OP_QUEUE_UPDATED
        MC_OP_SPL_UPDATED
        MC_OP_SPL_LIST_UPDATED


    ################
    #  Structures  #
    ################

    ctypedef struct mc_Client:
        mpd_status * status
        mpd_stats * stats
        mpd_song * song

    #############
    #  Methods  #
    #############

    # Hiden on purpose. Do not screw with
    # the connetion on the python layer
    # mpd_connection * mc_proto_get (mc_Client *)
    mpd_status * mc_proto_get_status(mc_Client *)

    # Networking
    mc_Client * mc_proto_create (mc_PmType)
    void mc_proto_put (mc_Client *)
    bool mc_proto_is_connected (mc_Client *)
    char * mc_proto_disconnect (mc_Client *)
    void mc_proto_free (mc_Client *)
    char * mc_proto_connect (mc_Client *, void *, char *, int, float)

    # Signals:
    void mc_proto_signal_add (mc_Client * , char *, void *, void *)
    void mc_proto_signal_add_masked (mc_Client * , char *, void *, void *, mpd_idle)
    void mc_proto_signal_rm (mc_Client * , char *, void *)
    void mc_proto_signal_dispatch (mc_Client * , char *, ...)
    int mc_proto_signal_length (mc_Client * , char *)
    void mc_proto_force_sss_update (mc_Client * , mpd_idle)

###########################################################################
#                             Client Commands                             #
###########################################################################

cdef extern from "../../lib/mpd/client.h":

    bool mc_client_output_switch (mc_Client *,  char *, bool)
    bool mc_client_password (mc_Client *,  char *)
    void mc_client_consume (mc_Client *, bool)
    void mc_client_crossfade (mc_Client *, bool)
    void mc_client_database_rescan (mc_Client *, char *)
    void mc_client_database_update (mc_Client *, char *)
    void mc_client_mixramdb (mc_Client *, int)
    void mc_client_mixramdelay (mc_Client *, int)
    void mc_client_next (mc_Client *)
    void mc_client_pause (mc_Client *)
    void mc_client_play_id (mc_Client *, unsigned)
    void mc_client_playlist_add (mc_Client *,  char *,  char *)
    void mc_client_playlist_clear (mc_Client *,  char *)
    void mc_client_playlist_delete (mc_Client *,  char *, unsigned)
    void mc_client_playlist_load (mc_Client *,  char *)
    void mc_client_playlist_move (mc_Client *,  char *, unsigned, unsigned)
    void mc_client_playlist_rename (mc_Client *,  char *,  char *)
    void mc_client_playlist_rm (mc_Client *,  char *)
    void mc_client_playlist_save (mc_Client *,  char *)
    void mc_client_play (mc_Client *)
    void mc_client_previous (mc_Client *)
    void mc_client_prio_id (mc_Client *, unsigned, unsigned)
    void mc_client_prio (mc_Client *, unsigned, unsigned)
    void mc_client_prio_range (mc_Client *, unsigned , unsigned , unsigned)
    void mc_client_queue_add (mc_Client *,  char * uri)
    void mc_client_queue_clear (mc_Client *)
    void mc_client_queue_delete_id (mc_Client *, int)
    void mc_client_queue_delete (mc_Client *, int)
    void mc_client_queue_delete_range (mc_Client *, int , int)
    void mc_client_queue_move (mc_Client *, unsigned , unsigned)
    void mc_client_queue_move_range (mc_Client *, unsigned , unsigned , unsigned)
    void mc_client_queue_shuffle (mc_Client *)
    void mc_client_queue_swap_id (mc_Client *, int , int)
    void mc_client_queue_swap (mc_Client *, int , int)
    void mc_client_random (mc_Client *, bool)
    void mc_client_repeat (mc_Client *, bool)
    void mc_client_replay_gain_mode (mc_Client *,  char *)
    void mc_client_seekcur (mc_Client *, int)
    void mc_client_seekid (mc_Client *, int , int)
    void mc_client_seek (mc_Client *, int , int)
    void mc_client_setvol (mc_Client *, int)
    void mc_client_single (mc_Client *, bool)
    void mc_client_stop (mc_Client *)

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


cdef extern from "../../lib/store/db-settings.h":
    ctypedef struct mc_StoreSettings:
        pass

    mc_StoreSettings * mc_store_settings_new ()
    void mc_store_settings_destroy (mc_StoreSettings *)


cdef extern from "../../lib/store/db.h":
    ctypedef struct mc_StoreDB:
        pass

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
    bool mc_store_get_wait_mode (mc_StoreDB *)

###########################################################################
#                             Misc Interfaces                             #
###########################################################################

cdef extern from "../../lib/misc/bug-report.h":
    char * mc_misc_bug_report (mc_Client * client)

cdef extern from "../../lib/misc/posix-signal.h":
    void mc_misc_register_posix_signal (mc_Client * client)

cdef extern from "../../lib/config.h":
    enum:
        VERSION               'MC_VERSION'
        VERSION_MAJOR         'MC_VERSION_MAJOR'
        VERSION_MINOR         'MC_VERSION_MINOR'
        VERSION_PATCH         'MC_VERSION_PATCH'
        VERSION_GIT_REVISION  'MC_VERSION_GIT_REVISION'
