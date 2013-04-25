#include "client-command-list.h"


bool mc_client_command_list_begin(mc_Client *self)
{
    g_assert(self);

    if (mc_client_command_list_is_active(self) == false) {
        self->_command_list_conn = mc_proto_get(self);

        if (self->_command_list_conn != NULL) {
            if (mpd_command_list_begin(self->_command_list_conn, false) == false) {
                self->_command_list_conn = NULL;
                return false;
            }
        }
    }

    return mc_client_command_list_is_active(self);
}

///////////////////

bool mc_client_command_list_is_active(mc_Client *self)
{
    g_assert(self);

    return self->_command_list_conn != NULL;
}

///////////////////

bool mc_client_command_list_commit(mc_Client *self)
{
    g_assert(self);

    if (mc_client_command_list_is_active(self) == true) {
        if (mpd_command_list_end(self->_command_list_conn) == false)
            return false;

        if (mpd_response_finish(self->_command_list_conn) == false)
            return false;

        mc_proto_put(self);
        self->_command_list_conn = NULL;
    }

    return !mc_client_command_list_is_active(self);
}
