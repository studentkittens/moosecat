#include "mpd/protocol.h"
#include "store/db.h"

#include <string.h>
#include <stdio.h>
#include <locale.h>

#include <curses.h>


/** VT100 command to clear the screen. Use puts(VT100_CLEAR_SCREEN) to clear
 *  the screen. */
#define VT100_CLEAR_SCREEN "\033[2J"

/** VT100 command to reset the cursor to the top left hand corner of the
 *  screen. */
#define VT100_CURSOR_TO_ORIGIN "\033[H"


/** global vars
 * -> too lazy */

GMainLoop * _main_loop = NULL;

///////////////////////////////

gboolean stdin_io_callback(GIOChannel *source, GIOCondition condition, gpointer data)
{
    bool retv = true;
    char c;


    g_printerr(".. before called ..\n");
    c = getch();
    g_printerr(".. after called ..\n");

    g_printerr("=> %d\n", c);

    /*
    if (c == '\n')
    {
        puts("... newline ...");
        //puts(VT100_CLEAR_SCREEN);
        //puts(VT100_CURSOR_TO_ORIGIN);
    }
    */
#if 0
    switch(status)
    {
        case G_IO_STATUS_EOF:
            g_main_loop_quit(_main_loop);
            retv = false;
            break;
        case G_IO_STATUS_NORMAL:
        case G_IO_STATUS_ERROR:
        case G_IO_STATUS_AGAIN:
            break;
    }
#endif

#if 0
    switch((c = getchar()))
    {
        case ':':
            { 
                char cmd[256] = {0};
                if(fgets(cmd, 256, stdin))
                {
                    /* Chomp newline */
                    g_strstrip (cmd);
                    puts(cmd);
                }
                return true;
            }
        case 'q':
            {
                g_main_loop_quit(_main_loop);
                retv = false;
                break;
            }
    }


    while((c = getchar()) != EOF && c != '\n');
#endif 

    return retv;
}

///////////////////////////////

int main(void)
{
    mc_Client * client = mc_proto_create (MC_PM_COMMAND);
    mc_proto_connect (client, NULL, "localhost", 6600, 2.0);

    setlocale(LC_ALL, NULL);

    GIOChannel * stdin_chan = g_io_channel_unix_new(fileno(stdin));
    g_io_add_watch(stdin_chan, G_IO_IN | G_IO_PRI, stdin_io_callback, NULL);

    _main_loop = g_main_loop_new (NULL, true);
    g_main_loop_run (_main_loop);

#if 0
    GTimer * db_timer = g_timer_new ();
    g_timer_start (db_timer);

    mc_StoreDB * db = mc_store_create (client, NULL, NULL);

    if (db != NULL)
    {
        g_print ("database created in %2.3f seconds.\n", g_timer_elapsed(db_timer, NULL));

        const int song_buf_size = 100;
        const int line_buf_size = 32;

        char line_buf[line_buf_size];
        mpd_song ** song_buf = g_malloc(sizeof(mpd_song *) * song_buf_size);

        memset(line_buf, 0, line_buf_size);
        memset(song_buf, 0, song_buf_size);

        for(;;)
        {
            g_print("> ");
            if(fgets(line_buf, line_buf_size, stdin) == NULL)
                break;

            /* remove trailing newline */
            g_strstrip(line_buf);

            int selected = mc_store_search_out (db, line_buf, song_buf, song_buf_size);

            if (selected > 0)
            {
                g_print ("%-35s | %-35s | %-35s\n", "Artist", "Album", "Title");
                g_print ("------------------------------------------------------------------------------------------------\n");
                for (int i = 0; i < selected; i++)
                {
                    g_print("%-35s | %-35s | %-35s\n", 
                            mpd_song_get_tag(song_buf[i], MPD_TAG_ARTIST, 0),
                            mpd_song_get_tag(song_buf[i], MPD_TAG_ALBUM, 0),
                            mpd_song_get_tag(song_buf[i], MPD_TAG_TITLE, 0));
                }

            }
            else
            {
                g_print("=> No results.\n");
            }
        }
        g_free (song_buf);
    }

    g_timer_destroy (db_timer);
#endif

    g_io_channel_unref(stdin_chan);
    mc_proto_free (client);
}
