#include "common.h"

///////////////////////////

static const int map_io_enums[][2] =
{
    {G_IO_IN,  MPD_ASYNC_EVENT_READ  },
    {G_IO_OUT, MPD_ASYNC_EVENT_WRITE },
    {G_IO_HUP, MPD_ASYNC_EVENT_HUP   },
    {G_IO_ERR, MPD_ASYNC_EVENT_ERROR }
};

///////////////////////////

static const unsigned map_io_enums_size = sizeof (map_io_enums) /
        sizeof (const int) /
        2;

///////////////////////////

mpd_async_event gio_to_mpd_async (GIOCondition condition)
{
    int events = 0;
    for (unsigned i = 0; i < map_io_enums_size; i++)
        if (condition & map_io_enums[i][0])
            events |= map_io_enums[i][1];
    return (mpd_async_event) events;
}

///////////////////////////

GIOCondition mpd_async_to_gio (mpd_async_event events)
{
    int condition = 0;
    for (unsigned i = 0; i < map_io_enums_size; i++)
        if (events & map_io_enums[i][1])
            condition |= map_io_enums[i][0];
    return (GIOCondition) condition;
}

///////////////////////////

mpd_connection * mpd_connect (const char * host, int port, int timeout)
{
    mpd_connection * con = mpd_connection_new (host, port, timeout * 1000);

    if (con && mpd_connection_get_error (con) != MPD_ERROR_SUCCESS)
    {
        g_print ("Unable to connect: %s\n", mpd_connection_get_error_message (con) );
        mpd_connection_free (con);
        con = NULL;
    }

    return con;
}
