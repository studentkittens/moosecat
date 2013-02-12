#include "api.h"

#include <stdlib.h>
#include <stdio.h>

#include <mpd/client.h>

int main(void)
{
    mc_Client *client = mc_proto_create(MC_PM_COMMAND);
    char *err = mc_proto_connect(client, NULL, "localhost", 6600, 2);

    if (err != NULL) {
        g_print("Err: %s\n", err);
        g_free(err);
        return EXIT_FAILURE;
    }

    if (mc_proto_is_connected(client) == FALSE) {
        g_print("Not connected.\n");
        return EXIT_FAILURE;
    }

    mc_StoreDB *store = mc_store_create(client, NULL);
    mc_Stack *songs = mc_stack_create(10000, NULL);
    mc_store_wait(store);
    GTimer *pl_timer = g_timer_new();
    mc_store_playlist_load(store, "test");
    g_timer_start(pl_timer);
    mc_store_playlist_select_to_stack(store, songs, "test", "knorkator");
    g_print("%d\n", mc_stack_length(songs));

    for (unsigned i = 0; i < mc_stack_length(songs); ++i) {
        struct mpd_song *song = mc_stack_at(songs, i);
        g_print("%s\n", mpd_song_get_tag(song, MPD_TAG_TITLE, 0));
    }

    g_print("Time elapsed: %2.3fs\n", g_timer_elapsed(pl_timer, NULL));
    g_timer_destroy(pl_timer);
    mc_proto_disconnect(client);
    mc_proto_free(client);
    return FALSE;
}
