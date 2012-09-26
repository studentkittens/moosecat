#include "mpd/protocol.h"
#include "store/db.h"
#include "misc/posix_signal.h"

#include <string.h>
#include <stdio.h>

///////////////////////////////

static void print_progress (mc_Client * self, bool print_newline, const char * msg, void * userdata)
{
    (void) self;
    (void) userdata;
    g_print ("%s                                     %c", msg, print_newline ? '\n' : '\r');
}

///////////////////////////////

static void print_error (mc_Client * self, enum mpd_error err,  const char * err_msg, bool is_fatal, void * userdata)
{
    (void) self;
    (void) userdata;
    (void) err;
    (void) is_fatal;
    g_print ("ERROR: %s\n", err_msg);
}

///////////////////////////////

static void print_event (mc_Client * self, enum mpd_idle event, void * user_data)
{
    g_print ("Usercallback: %p %d %p\n", self, event, user_data);
}

///////////////////////////////

int main (int argc, char * argv[])
{
    if (argc < 2) {
        puts ("usage: db_test [search|mainloop]");
        return -1;
    }

    mc_Client * client = mc_proto_create (MC_PM_COMMAND);
    mc_proto_connect (client, NULL, "localhost", 6600, 10.0);
    mc_proto_signal_add (client, "progress", print_progress, NULL);
    mc_proto_signal_add (client, "error", print_error, NULL);
    mc_misc_register_posix_signal (client);
    mc_proto_signal_add (client, "client-event", print_event, NULL);

    mc_StoreDB * db = mc_store_create (client, NULL, NULL);
    mc_store_set_wait_mode (db, true);


    if (db != NULL) {
        if (g_strcmp0 (argv[1], "search") == 0) {
            const int song_buf_size = 100;
            const int line_buf_size = 32;

            bool queue_only = true;

            char line_buf[line_buf_size];
            mpd_song ** song_buf = g_malloc (sizeof (mpd_song *) * song_buf_size);

            memset (line_buf, 0, line_buf_size);
            memset (song_buf, 0, song_buf_size);

            for (;;) {
                g_print ("> ");
                if (fgets (line_buf, line_buf_size, stdin) == NULL)
                    break;

                /* remove trailing newline */
                g_strstrip (line_buf);

                if (g_strcmp0 (line_buf, ":q") == 0)
                    break;

                if (g_strcmp0 (line_buf, ":u") == 0) {
                    mc_proto_force_sss_update (client, INT_MAX);
                    mc_proto_signal_dispatch (client, "client-event", client,
                                              MPD_IDLE_DATABASE | MPD_IDLE_QUEUE);
                    continue;
                }

                if (g_strcmp0 (line_buf, ":queue") == 0) {
                    queue_only = !queue_only;
                    g_print ("Searching only on Queue: %s\n", queue_only ? "Yes" : "No");
                    continue;
                }

                if (g_strcmp0 (line_buf, ":disconnect") == 0) {
                    mc_proto_disconnect (client);
                    continue;
                }

                if (g_strcmp0 (line_buf, ":connect") == 0) {
                    mc_proto_connect (client, NULL, "localhost", 6600, 10.0);
                    continue;
                }


                int selected = mc_store_search_out (db, line_buf, queue_only, song_buf, song_buf_size);

                if (selected > 0) {
                    g_print ("%-35s | %-35s | %-35s\n", "Artist", "Album", "Title");
                    g_print ("------------------------------------------------------------------------------------------------\n");
                    for (int i = 0; i < selected; i++) {
                        g_print ("%-35s | %-35s | %-35s\n",
                                 mpd_song_get_tag (song_buf[i], MPD_TAG_ARTIST, 0),
                                 mpd_song_get_tag (song_buf[i], MPD_TAG_ALBUM, 0),
                                 mpd_song_get_tag (song_buf[i], MPD_TAG_TITLE, 0) );
                    }

                } else {
                    g_print ("=> No results.\n");
                }
            }
            g_free (song_buf);

            puts ("");
        } else if (g_strcmp0 (argv[1], "mainloop") == 0) {
            puts ("");
            GMainLoop * loop = g_main_loop_new (NULL, true);
            g_main_loop_run (loop);
            g_main_loop_unref (loop);
        }
    }

    mc_store_close (db);
    mc_proto_free (client);
}
