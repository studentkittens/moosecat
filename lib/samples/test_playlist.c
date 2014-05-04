#include "../api.h"

#include <stdlib.h>
#include <stdio.h>

#include <mpd/client.h>

int main(int argc, char **argv)
{
    if(argc < 3) {
        return -1;
    }

    mc_Client *client = mc_create(MC_PM_IDLE);
    char *err = mc_connect(client, NULL, "localhost", 6600, 2);

    if (err != NULL) {
        g_print("Err: %s\n", err);
        g_free(err);
        return EXIT_FAILURE;
    }

    if (mc_is_connected(client) == FALSE) {
        g_print("Not connected.\n");
        return EXIT_FAILURE;
    }

    mc_Store *store = mc_store_create(client, NULL);
    mc_store_wait(store);

    mc_Playlist *songs = mc_stack_create(10000, NULL);
    GTimer *pl_timer = g_timer_new();

    mc_store_playlist_load(store, argv[1]);
    g_timer_start(pl_timer);
    mc_store_playlist_select_to_stack(store, songs, argv[1], argv[2]);

    g_print("Found songs: %d\n", mc_stack_length(songs));
    g_print("Time elapsed: %2.3fs\n", g_timer_elapsed(pl_timer, NULL));
            
    for(size_t i = 0; i < mc_stack_length(songs); ++i) {
        struct mpd_song *song = mc_stack_at(songs, i);
        g_print("%s\t%s\t%s\t%s\n", 
                mpd_song_get_tag(song, MPD_TAG_ARTIST, 0),
                mpd_song_get_tag(song, MPD_TAG_ALBUM, 0),
                mpd_song_get_tag(song, MPD_TAG_TITLE, 0),
                mpd_song_get_uri(song)
        );
    }


    g_timer_destroy(pl_timer);
    mc_stack_clear(songs);

    mc_disconnect(client);
    mc_free(client);

    return 0;
}
