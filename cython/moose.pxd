from libcpp cimport bool

###########################################################################
#                              libmpdclient                               #
###########################################################################

cdef extern from "mpd/client.h":
    ################
    #  Structures  #
    ################

    cdef struct mpd_song:
        pass

    cdef struct mpd_stats:
        pass

    cdef struct mpd_status:
        pass

    cdef struct mpd_connection:
        pass

    ###########
    #  Enums  #
    ###########

    cdef enum mpd_idle:
        pass # TODO: Define actual values.

###########################################################################
#                             Main Interface                              #
###########################################################################

cdef extern from "../lib/mpd/protocol.h":

    ctypedef enum mc_PmType:
        MC_PM_IDLE
        MC_PM_COMMAND

    ################
    #  Structures  #
    ################

    ctypedef struct mc_Client:
        mpd_status * status
        mpd_stats * stats
        mpd_song * song

    ctypedef struct GMainContext:
        pass


    #############
    #  Methods  #
    #############

    # Networking
    mc_Client * mc_proto_create (mc_PmType)
    mpd_connection * mc_proto_get (mc_Client *)
    void mc_proto_put (mc_Client *)
    bool mc_proto_is_connected (mc_Client *)
    char * mc_proto_disconnect (mc_Client *)
    void mc_proto_free (mc_Client *)
    char * mc_proto_connect (mc_Client *, GMainContext *, char *, int, float)

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

cdef extern from "../lib/mpd/client.h":

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

cdef extern from "../lib/store/stack.h":
    ctypedef struct mc_Stack:
        pass

    mc_Stack * mc_stack_create (long, void *)
    void mc_stack_append (mc_Stack *, void *)
    void mc_stack_free (mc_Stack *)
    void mc_stack_clear (mc_Stack *)
    unsigned mc_stack_length (mc_Stack *)
    void mc_stack_sort (mc_Stack *, void *)


cdef extern from "../lib/store/settings.h":
    ctypedef struct mc_StoreSettings:
        pass

    mc_StoreSettings * mc_store_settings_new ()
    void mc_store_settings_destroy (mc_StoreSettings *)


cdef extern from "../lib/store/db.h":
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

###########################################################################
#                             Misc Interfaces                             #
###########################################################################

cdef extern from "../lib/misc/bug-report.h":
    char * mc_misc_bug_report (mc_Client * client)

cdef extern from "../lib/misc/posix-signal.h":
    void mc_misc_register_posix_signal (mc_Client * client)
