#include "../mpd/protocol.h"
#include "../store/db.h"
#include "../misc/posix-signal.h"

#include <string.h>
#include <stdio.h>

///////////////////////////////

static void print_logging(mc_Client *self, const char * message, mc_LogLevel level, gpointer user_data)
{
    g_printerr("Logging(%d): %s\n", level, message);
}

///////////////////////////////

static void print_event(mc_Client *self, enum mpd_idle event, void *user_data)
{
    g_print("event-signal: %p %d %p\n", self, event, user_data);
}

///////////////////////////////

static void print_connectivity(mc_Client *self, bool server_changed, bool was_connected, void *user_data)
{
    g_print("connectivity-signal: changed=%d was_connected=%d is_connected=%d\n",
            server_changed, was_connected, mc_is_connected(self)
    );
}

///////////////////////////////

int main(int argc, char *argv[])
{
    if (argc < 2) {
        puts("usage: db_test [search|mainloop]");
        return -1;
    }

    mc_Client *client = mc_create(MC_PM_IDLE);

    /* Trigger some bugs */
    for(int i = 0; i < 10; i++) {
        mc_connect(client, NULL, "localhost", 6666, 10.0);
        mc_disconnect(client);
    }

    mc_connect(client, NULL, "localhost", 6666, 10.0);
    mc_signal_add(client, "logging", print_logging, NULL);
    mc_signal_add(client, "client-event", print_event, NULL);
    mc_signal_add(client, "connectivity", print_connectivity, NULL);
    mc_misc_register_posix_signal(client);


    mc_StoreSettings *settings = mc_store_settings_new();
    settings->use_memory_db = FALSE;
    settings->use_compression = FALSE;
    mc_Store *db = mc_store_create(client, settings);

    mc_store_playlist_load(db, "test1");

    if (db != NULL) {
        if (g_strcmp0(argv[1], "search") == 0) {
            const int song_buf_size = 100;
            const int line_buf_size = 32;
            bool queue_only = true;
            char line_buf[line_buf_size];
            mc_Stack *song_buf = mc_stack_create(song_buf_size, NULL);
            memset(line_buf, 0, line_buf_size);

            for (;;) {
                g_print("> ");

                if (fgets(line_buf, line_buf_size, stdin) == NULL)
                    break;

                /* remove trailing newline */
                g_strstrip(line_buf);

                if (g_strcmp0(line_buf, ":q") == 0)
                    break;

                if (g_strcmp0(line_buf, ":u") == 0) {
                    mc_force_sync(client, INT_MAX);
                    continue;
                }

                if (strncmp(line_buf, ":load", 5) == 0) {
                    char *pl_name = g_strstrip(&line_buf[5]);
                    mc_store_gw(db, mc_store_playlist_load(db, pl_name));
                    continue;
                }

                if (g_strcmp0(line_buf, ":queue") == 0) {
                    queue_only = !queue_only;
                    g_print("Searching only on Queue: %s\n", queue_only ? "Yes" : "No");
                    continue;
                }

                if (g_strcmp0(line_buf, ":disconnect") == 0) {
                    mc_disconnect(client);
                    continue;
                }

                if (g_strcmp0(line_buf, ":connect") == 0) {
                    mc_connect(client, NULL, "localhost", 6600, 10.0);
                    continue;
                }

                if (strncmp(line_buf, ":spl ", 5) == 0) {
                    char **args = g_strsplit(line_buf, " ", -1);

                    if (args != NULL)  {
                        mc_Stack *stack = mc_stack_create(1000, NULL);
                        mc_store_gw(db, mc_store_playlist_select_to_stack(db, stack, args[1], args[2]));
                        g_print("%s %s\n", args[1], args[2]);

                        int found = mc_stack_length(stack);
                        if (found == 0) {
                            g_print("Nothing found.\n");
                        } else {
                            for (int i = 0; i < found; ++i) {
                                struct mpd_song *song = mc_stack_at(stack, i);
                                g_print("%s\n", mpd_song_get_uri(song));
                            }
                        }
                        mc_store_release(db);

                        mc_stack_free(stack);
                        g_strfreev(args);
                    }

                    continue;
                }

                if (strncmp(line_buf, ":ls ", 4) == 0) {
                    char **args = g_strsplit(line_buf, " ", -1);

                    if (args != NULL) {
                        char *query = (*args[1] == '_') ? NULL : args[1];
                        mc_Stack *stack = mc_stack_create(100, g_free);
                        int depth = (args[2]) ? g_ascii_strtoll(args[2], NULL, 10) : -1;
                        mc_store_gw(db, mc_store_dir_select_to_stack(db, stack, query, depth));

                        int found = mc_stack_length(stack);
                        if (found == 0) {
                            g_print("Nothing found.\n");
                        } else {
                            for (int i = 0; i < found; ++i) {
                                g_print("%s\n", (char *) mc_stack_at(stack, i));
                            }
                        }
                        mc_store_release(db);

                        mc_stack_free(stack);
                        g_strfreev(args);
                    }

                    continue;
                }

                mc_stack_clear(song_buf);
                mc_store_gw(db, mc_store_search_to_stack(db, line_buf, queue_only, song_buf, song_buf_size));

                int selected = mc_stack_length(song_buf);
                if (selected > 0) {
                    g_print("#%04d/%03d %-35s | %-35s | %-35s\n", 0, 0, "Artist", "Album", "Title");
                    g_print("------------------------------------------------------------------------------------------------\n");

                    for (int i = 0; i < selected; i++) {
                        struct mpd_song *song = mc_stack_at(song_buf, i);
                        g_print("%04d/%04d %-35s | %-35s | %-35s\n",
                                mpd_song_get_pos(song),
                                mpd_song_get_id(song),
                                mpd_song_get_tag(song, MPD_TAG_ARTIST, 0),
                                mpd_song_get_tag(song, MPD_TAG_ALBUM, 0),
                                mpd_song_get_tag(song, MPD_TAG_TITLE, 0));
                    }
                } else {
                    g_print("=> No results.\n");
                }

                mc_store_release(db);
            }

            mc_stack_free(song_buf);
        } else if (g_strcmp0(argv[1], "mainloop") == 0) {
            puts("");
            GMainLoop *loop = g_main_loop_new(NULL, true);
            g_main_loop_run(loop);
            g_main_loop_unref(loop);

        }
    }

    mc_store_close(db);
    mc_store_settings_destroy(settings);
    mc_free(client);
}
