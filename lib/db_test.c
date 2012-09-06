#include "mpd/protocol.h"
#include "store/db.h"

#include <string.h>
#include <stdio.h>

///////////////////////////////

static void print_progress(mc_Client * self, bool print_newline, const char * msg, void * userdata)
{
    (void) self;
    (void) userdata;
    g_print("%s                                     %c", msg, print_newline ? '\n' : '\r');
}

///////////////////////////////

int main (void)
{
    mc_Client * client = mc_proto_create (MC_PM_COMMAND);
    mc_proto_connect (client, NULL, "localhost", 6600, 2.0);
    mc_proto_signal_add(client, "progress", print_progress, NULL);

    GTimer * db_timer = g_timer_new ();
    g_timer_start (db_timer);

    mc_StoreDB * db = mc_store_create (client, NULL, NULL);

    if (db != NULL)
    {
        g_print ("database: created in %2.3f seconds.                               \n", g_timer_elapsed (db_timer, NULL) );

        const int song_buf_size = 100;
        const int line_buf_size = 32;

        char line_buf[line_buf_size];
        mpd_song ** song_buf = g_malloc (sizeof (mpd_song *) * song_buf_size);

        memset (line_buf, 0, line_buf_size);
        memset (song_buf, 0, song_buf_size);

        for (;;)
        {
            g_print ("> ");
            if (fgets (line_buf, line_buf_size, stdin) == NULL)
                break;

            /* remove trailing newline */
            g_strstrip (line_buf);

            if (g_strcmp0 (line_buf, ":q") == 0)
                break;

            int selected = mc_store_search_out (db, line_buf, song_buf, song_buf_size);

            if (selected > 0)
            {
                g_print ("%-35s | %-35s | %-35s\n", "Artist", "Album", "Title");
                g_print ("------------------------------------------------------------------------------------------------\n");
                for (int i = 0; i < selected; i++)
                {
                    g_print ("%-35s | %-35s | %-35s\n",
                             mpd_song_get_tag (song_buf[i], MPD_TAG_ARTIST, 0),
                             mpd_song_get_tag (song_buf[i], MPD_TAG_ALBUM, 0),
                             mpd_song_get_tag (song_buf[i], MPD_TAG_TITLE, 0) );
                }

            }
            else
            {
                g_print ("=> No results.\n");
            }
        }
        g_free (song_buf);
    }
    puts("");

    g_timer_destroy (db_timer);

    mc_store_close (db);
    mc_proto_free (client);

}
