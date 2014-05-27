#include "../moose-api.h"

#include <stdlib.h>
#include <stdio.h>

#include <mpd/client.h>

int main(int argc, char ** argv) {
    if (argc < 3) {
        return -1;
    }

    MooseClient * client = moose_client_new(MOOSE_PROTOCOL_IDLE);
    char * err = moose_client_connect(client, NULL, "localhost", 6600, 2);

    if (err != NULL) {
        g_print("Err: %s\n", err);
        g_free(err);
        return EXIT_FAILURE;
    }

    if (moose_client_is_connected(client) == FALSE) {
        g_print("Not connected.\n");
        return EXIT_FAILURE;
    }

    MooseStore * store = moose_store_new(client);
    moose_store_wait(store);

    MoosePlaylist * songs = moose_playlist_new();
    GTimer * pl_timer = g_timer_new();

    moose_store_playlist_load(store, argv[1]);
    g_timer_start(pl_timer);
    moose_store_playlist_select_to_stack(store, songs, argv[1], argv[2]);

    g_print("Found songs: %d\n", moose_playlist_length(songs));
    g_print("Time elapsed: %2.3fs\n", g_timer_elapsed(pl_timer, NULL));

    for (size_t i = 0; i < moose_playlist_length(songs); ++i) {
        MooseSong * song = moose_playlist_at(songs, i);
        g_print("%s\t%s\t%s\t%s\n",
                moose_song_get_tag(song, MOOSE_TAG_ARTIST),
                moose_song_get_tag(song, MOOSE_TAG_ALBUM),
                moose_song_get_tag(song, MOOSE_TAG_TITLE),
                moose_song_get_uri(song)
               );
    }


    g_timer_destroy(pl_timer);
    moose_playlist_clear(songs);

    moose_client_disconnect(client);
    moose_client_unref(client);

    return 0;
}
