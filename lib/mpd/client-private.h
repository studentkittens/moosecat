#ifndef MC_CLIENT_PRIVATE_HH
#define MC_CLIENT_PRIVATE_HH

/**
 * @brief Macros to easily implement certain client commands.
 */


#define BEGIN_COMMAND                                       \
    if(mc_client_command_list_is_active(self) == false) {   \
        struct mpd_connection * conn = mc_proto_get(self);  \
        if(conn != NULL)                                    \
        {                                                   \


#define END_COMMAND                                         \
            if (mpd_response_finish(conn) == false) {       \
                mc_shelper_report_error(self, conn);        \
            }                                               \
        }                                                   \
        mc_proto_put(self);                                 \
    } else {                                                \
        g_warning("CommandList Mode is active!\n");         \
    }                                                       \
 
/* Template to define new commands */
#define COMMAND(_code_, _command_list_code_)                     \
    g_assert (self);                                             \
    if(mc_client_command_list_is_active(self)) {                 \
        struct mpd_connection * conn = self->_command_list_conn; \
        _command_list_code_;                                     \
    } else {                                                     \
        BEGIN_COMMAND                                            \
        {                                                        \
            _code_;                                              \
        }                                                        \
        END_COMMAND                                              \
    }


#endif /* end of include guard: MC_CLIENT_PRIVATE_HH */
