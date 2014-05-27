#ifndef DB_GUARD_H
#define DB_GUARD_H

#include "../mpd/moose-mpd-client.h"
#include "moose-store-playlist.h"
#include "moose-store-completion.h"

G_BEGIN_DECLS

/*
 * Type macros.
 */
#define MOOSE_TYPE_STORE \
    (moose_store_get_type())
#define MOOSE_STORE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_STORE, MooseStore))
#define MOOSE_IS_STORE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_STORE))
#define MOOSE_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_STORE, MooseStoreClass))
#define MOOSE_IS_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_STORE))
#define MOOSE_STORE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_STORE, MooseStoreClass))


GType moose_store_get_type(void);

struct _MooseStorePrivate;

typedef struct _MooseStore {
    GObject parent;
    struct _MooseStorePrivate *priv;
} MooseStore;

typedef struct _MooseStoreClass {
    GObjectClass parent_class;
} MooseStoreClass;

/**
 * @brief Create a new MooseStore.
 *
 * @param directory the path to the containing dir
 * @param client a connection to operate on.
 *
 * If no db exist yet at this place (assuming it is a valid path),
 * then a new empy is created with a Query-Version of 0.
 * A call to moose_store_update will therefore insert the whole queue to the list.
 *
 * If the db is already created, a call to moose_store_update() will only
 * insert the latest differences (which might be enourmous too,
 * since mpd returns all changes behind a certain ID)
 *
 * The name of the db will me moosecat_$host:$port
 *
 * @return a new MooseStore , free with moose_store_unref()
 */
MooseStore *moose_store_new(MooseClient *client);
MooseStore *moose_store_new_full(
    MooseClient *client,
    const char *db_directory,
    const char *tokenizer,
    bool use_memory_db,
    bool use_compression
);


void moose_store_unref(MooseStore *self);

/**
 * @brief Returns a mpd_song at some index
 *
 * This is effectively only useful on moose_store_dir_select_to_stack
 * (which returns a list of files, prefixed with IDs or -1 in case of a directory)
 *
 * @param self the store to operate on.
 * @param idx the index (0 - moose_store_total_songs())
 *
 * @return an mpd_song. DO NOT FREE!
 */
MooseSong * moose_store_song_at(MooseStore * self, int idx);

/**
 * @brief Returns the total number of songs stored in the database.
 *
 * This is basically the same as 'select count(*) from songs;' just faster.
 *
 * @param self the store to operate on.
 *
 * @return a number between 0 and fucktuple.
 */
int moose_store_total_songs(MooseStore * self);

///// PLAYLIST HANDLING ////

/**
 * @brief Load the playlist from the MPD server and store it in the database.
 *
 * Note: For every song in a playlist only an ID is stored. Therefore, this is quite efficient.
 *
 * @param self the store to operate on.
 * @param playlist_name a playlist name, as displayed by MPD.
 *
 * @return a job id
 */
long moose_store_playlist_load(MooseStore * self, const char * playlist_name);

/**
 * @brief Queries the contents of a playlist in a similar fashion as moose_store_search_out does.
 *
 * On succes, stack is filled with mpd_songs up to $return_value songs.
 *
 * @param self the store to operate on.
 * @param stack a stack to fill.
 * @param playlist_name a playlist name, as displayed by MPD.
 * @param match_clause an fts match clause. Empty ("") or NULL returns *all* songs in the playlist.
 *
 * @return a job id
 */
long moose_store_playlist_select_to_stack(
    MooseStore * self, MoosePlaylist * stack,
    const char * playlist_name, const char * match_clause);

/**
 * @brief List a directory in MPD's database.
 *
 * @param self the store to operate on.
 * @param stack a stack to fill.
 * @param directory the directory to list. (NULL == '/')
 * @param depth at wath depth to search (0 == '/', -1 to disable.)
 *
 * @return a job id
 */
long moose_store_dir_select_to_stack(
    MooseStore * self, MoosePlaylist * stack,
    const char * directory, int depth);

/**
 * @brief return a stack of the loaded playlists. Not the actually ones there.
 *
 * @param self the store to operate on.
 * @param stack a stack of mpd_playlists. DO NOT FREE CONTENT!
 *
 * @return a job id
 */
long moose_store_playlist_get_all_loaded(MooseStore * self, MoosePlaylist * stack);

/**
 * @brief Returns a stack with mpd_playlists of all KNOWN playlists (not loaded)
 *
 * Only free the stack with g_object_unref(). Not the playlists!
 *
 *
 * @param self the store to operate on
 * @param stack a stack to store the playlists in.
 *
 * @return a job id
 */
long moose_store_playlist_get_all_known(MooseStore * self, MoosePlaylist * stack);

/**
 * @brief search the Queue or the whole Database.
 *
 * @param self the client to operate on
 * @param match_clause a fts match clause (NULL will return *all* songs)
 * @param queue_only restrict search to queue only?
 * @param stack the stack to select the songs too
 * @param limit_len limit the length of the return. negative numbers dont limit.
 *
 * Direclty after calling this function you will have no results yet in the stack.
 * You should call moose_store_wait_for_job() to wait for it to be filled.
 *
 * Example:
 *
 *      MoosePlaylist * stack = moose_playlist_new();
 *      int job_id = moose_store_search_to_stack(store, "artist:Akrea", true, stack, -1);
 *      // You can other things than waiting here.
 *      moose_store_wait_for_job(store, job_id);
 *      moose_store_lock(store);
 *      for(int i = 0; i < moose_playlist_length(stack); ++i) {
 *          printf("%s\n", moose_song_tag(moose_playlist_at(i), MPD_TAG_TITLE, 0));
 *      }
 *      moose_store_unlock(store);
 *
 *
 * @return a Job id
 */
long moose_store_search_to_stack(
    MooseStore * self, const char * match_clause,
    bool queue_only, MoosePlaylist * stack, int limit_len);

/**
 * @brief Wait for the store to finish it's current operation.
 *
 * This is useful to wait for it to finish before shutdown.
 *
 * @param self the store to operate on.
 */
void moose_store_wait(MooseStore * self);

/**
 * @brief Wait for a certain job to complete.
 *
 * This can be useful at times when you want to fire an operation to
 * the database, but do not want for it to complete and do other things in
 * the meanwhile. After some time you can get the result with
 * moose_store_get_result()
 *
 * @param self the store to operate on
 * @param job_id a job id to wait on (acquired by the above functions)
 */
void moose_store_wait_for_job(MooseStore * self, int job_id);

/**
 * @brief Get a result from a job.
 *
 * Note: This function does NOT wait. To be sure that the result is there
 *       you should call moose_store_wait_for_job() previously.
 *
 * @param self the store to operate on.
 * @param job_id the job id obtained from one of the above functions.
 *
 * @return a MoosePlaylist containing the results you wanted.
 */
MoosePlaylist * moose_store_get_result(MooseStore * self, int job_id);


/**
 * @brief Shortcur for moose_store_wait_for_job and moose_store_get_result
 *
 * @param self the store to operate on
 * @param job_id job_id to wait on and get the result from
 *
 * @return same as moose_store_get_result
 */
MoosePlaylist * moose_store_gw(MooseStore * self, int job_id);


/**
 * @brief Tells store that you done using a ressource.
 *
 *
 * Bad things will happen if you don't release the ressources.
 * libmoosecat will likely deadlock.
 *
 * @param self the store you had the ressources from
 */
void moose_store_release(MooseStore * self);

/**
 * @brief Find a song by it's ID in the database.
 *
 * This is often used and therefore implemented for convienience/speed in C.
 * Note: This has linear complexity since it needs to scan the whole list.
 *
 * You need to call moose_store_release() after done using the song.
 *
 * @param self the Store to search on.
 * @param needle_song_id
 *
 * @return NULL if not found or a mpd_song struct (do not free!)
 */
MooseSong * moose_store_find_song_by_id(MooseStore * self, unsigned needle_song_id);

/**
 * @brief Convinience Function to createa MooseStoreCompletion struct.
 *
 * The struct will be created on the first call, afterwards the same struct is
 * returned.  * It will be freed on moose_store_unref.
 *
 * You can use moose_store_completion_lookup() to get a suggestion for a certain tag and
 * prefix.
 *
 * @param self the store to create the struct on.
 *
 * @return a valid MooseStoreCompletion, do not free.
 */
MooseStoreCompletion * moose_store_get_completion(MooseStore * self);

G_END_DECLS

#endif /* end of include guard: DB_GUARD_H */
