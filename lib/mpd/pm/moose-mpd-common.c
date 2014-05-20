#include "moose-mpd-common.h"
#include "../moose-mpd-signal-helper.h"

mpd_connection * mpd_connect(MooseClient * self, const char * host, int port, float timeout, char * * err)
{
    mpd_connection * con = mpd_connection_new(host, port, timeout * 1000);

    if (con && mpd_connection_get_error(con) != MPD_ERROR_SUCCESS) {
        char * error_message = g_strdup(mpd_connection_get_error_message(con));
        /* Report the error, but don't try to handle it in that early stage */
        moose_shelper_report_error_without_handling(self, con);

        if (err != NULL) {
            *err = error_message;
        } else {
            g_free(error_message);
        }

        mpd_connection_free(con);
        con = NULL;
    } else if (err != NULL) {
        *err = NULL;
    }

    return con;
}
