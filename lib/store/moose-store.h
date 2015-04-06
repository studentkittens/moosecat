#ifndef MOOSE_STORE_H
#define MOOSE_STORE_H

#include "../mpd/moose-mpd-client.h"
#include "moose-store-playlist.h"
#include "moose-store-completion.h"

G_BEGIN_DECLS

/*
 * Type macros.
 */
#define MOOSE_TYPE_STORE (moose_store_get_type())
#define MOOSE_STORE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_STORE, MooseStore))
#define MOOSE_IS_STORE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_STORE))
#define MOOSE_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_STORE, MooseStoreClass))
#define MOOSE_IS_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_STORE))
#define MOOSE_STORE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_STORE, MooseStoreClass))

GType moose_store_get_type(void);

struct _MooseStorePrivate;

typedef struct _MooseStore {
    GObject parent;
    struct _MooseStorePrivate *priv;
} MooseStore;

typedef struct _MooseStoreClass { GObjectClass parent_class; } MooseStoreClass;

/**
 * moose_store_new:
 * @client: The client that is used to retrieve the database.
 *
 * Creates  a new Store with default options.
 * See moose_store_new_full().
 *
 * Returns: A newly allocated store with a refcount of 1.
 */
MooseStore *moose_store_new(MooseClient *client);

/**
 * moose_store_new_full:
 * @client: The client that is used to retrieve the database.
 * @db_directory: The directory where the zipped db is created in.
 * @tokenizer: (nullable): Tokenizer to use or NULL.
 * @use_memory_db: Cache the database in memory
 * @use_compression: Compress the database using gzip when writing on disk.
 *
 * The database will be named db_directory/moosecat_${host}:${port}.zip
 * The .zip ending is only used when @use_compression is True.
 *
 * Available tokenizers are: "simple", "porter", "icu". "porter" is the default.
 *
 * If @use_memory_db is True, then the whole database is cached in memory.  This
 * obviously uses a bit more Heapstorage, but allows a little faster lookups.
 * The database will be backupped to disk on the appropiate times.
 *
 * Creates  a new Store with default options.
 *
 * Returns: A newly allocated store with a refcount of 1.
 */
MooseStore *moose_store_new_full(MooseClient *client,
                                 const char *db_directory,
                                 const char *tokenizer,
                                 gboolean use_memory_db,
                                 gboolean use_compression);

/**
 * moose_store_unref:
 * @self: (nullable): a #MoseStore
 *
 * Decrements the reference count of a #MooseStore.
 */
void moose_store_unref(MooseStore *self);

/**
 * moose_store_total_songs:
 * @self: a #MooseStore
 *
 * This is basically the same as 'select count(*) from songs;' just faster.
 * This is usually the same as moose_status_stats_get_number_of_songs().
 * Used in tests to assert the correct value.
 *
 * Returns: The number of rows in the database.
 */
int moose_store_total_songs(MooseStore *self);

/**
 * moose_store_playlist_load:
 * @self: a #MooseStore
 * @playlist_name: a playlist name, as displayed by MPD.
 *
 * Load the playlist from the MPD server and store it in the database.
 * Note: For every song in a playlist only an ID is stored.
 * Therefore, this is quite efficient.
 *
 * Returns: A job id
 */
long moose_store_playlist_load(MooseStore *self, const char *playlist_name);

/**
 * moose_store_playlist_query:
 * @self: a #MooseStore
 * @stack: The playlist to fill.
 * @playlist_name: A playlist name (as displayed by MPD)
 * @match_clause: an fts match clause. Empty ("") or NULL returns *all* songs in the
 *playlist.
 *
 * Filters a stored playlist `playlist_name` by `match_clause` and writes it to
 * `stack`.
 *
 *
 * Returns: a job id
 */
long moose_store_playlist_query(MooseStore *self, MoosePlaylist *stack,
                                const char *playlist_name, const char *match_clause);

/**
 * moose_store_query_directories:
 * @self: the store to operate on.
 * @stack: a stack to fill.
 * @directory: the directory to list. (NULL == '/')
 * @depth: at wath depth to search (0 == '/', -1 to disable.)
 *
 * List a directory in MPD's database.
 *
 * Returns: A job id which you can call moose_store_wait_for_job on.
 */
long moose_store_query_directories(MooseStore *self, MoosePlaylist *stack,
                                   const char *directory, int depth);

/**
 * moose_store_query:
 * @self: a #MooseStore
 * @match_clause: (nullable): a fts match clause (NULL will return *all* songs)
 * @queue_only: restrict search to queue only?
 * @stack: the stack to select the songs too
 * @limit_len: limit the length of the return. negative numbers dont limit.
 *
 * Search the Queue or the whole Database.
 * Direclty after calling this function you will have no results yet in the stack.
 * You should call moose_store_wait_for_job() to wait for it to be filled.
 *
 * Example:
 *
 *      MoosePlaylist * stack = moose_playlist_new();
 *      int job_id = moose_store_query(store, "artist:Akrea", true, stack, -1);
 *      // You can other things than waiting here.
 *      moose_store_wait_for_job(store, job_id);
 *      moose_store_lock(store);
 *      for(int i = 0; i < moose_playlist_length(stack); ++i) {
 *          printf("%s\n", moose_song_tag(moose_playlist_at(i), MPD_TAG_TITLE, 0));
 *      }
 *
 * Returns: a Job id
 */
long moose_store_query(MooseStore *self, const char *match_clause, gboolean queue_only,
                       MoosePlaylist *stack, int limit_len);

/**
 * moose_store_wait:
 * @self: a #MooseStore.
 *
 * Wait for the store to finish it's current operation.
 * This is useful to wait for it to finish before shutdown.
 */
void moose_store_wait(MooseStore *self);

/**
 * moose_store_wait_for_job:
 * @self: the store to operate on
 * @job_id: a job id to wait on (acquired by the above functions)
 *
 * Wait for a certain job to complete.  This can be useful at times when you
 * want to fire an operation to the database, but do not want for it to complete
 * and do other things in the meanwhile. After some time you can get the result
 * with moose_store_get_result()
 */
void moose_store_wait_for_job(MooseStore *self, int job_id);

/**
 * moose_store_get_result:
 * @self: the store to operate on.
 * @job_id: the job id obtained from one of the above functions.
 *
 * Get a result from a job.
 *
 * Note: This function does NOT wait. To be sure that the result is there
 *       you should call moose_store_wait_for_job() previously.
 *
 * Returns: (transfer full): a MoosePlaylist containing the results you wanted.
 */
MoosePlaylist *moose_store_get_result(MooseStore *self, int job_id);

/**
 * moose_store_gw:
 * @self: the store to operate on
 * @job_id: job_id to wait on and get the result from
 *
 * Shortcurt for moose_store_wait_for_job and moose_store_get_result.
 *
 * Returns: (transfer full): same as moose_store_get_result
 */
MoosePlaylist *moose_store_gw(MooseStore *self, int job_id);

/**
 * moose_store_find_song_by_id:
 * @self: a #MooseStore
 * @needle_song_id: The song id to select.
 *
 * Find a song by it's ID in the database.
 *
 * This is often used and therefore implemented for convienience/speed in C.
 * Note: This has linear complexity since it needs to scan the whole list.
 *
 * TODO: Can this be more efficient?
 *
 * Returns: (transfer full): NULL if not found or a mpd_song struct (do not free!)
 */
MooseSong *moose_store_find_song_by_id(MooseStore *self, unsigned needle_song_id);

/**
 * moose_store_get_completion:
 * @self: a #MooseStore
 *
 * Convinience Function to create a MooseStoreCompletion struct.  The struct
 * will be created on the first call, afterwards the same struct is returned.
 * It will be freed on moose_store_unref for you.
 *
 * You can use moose_store_completion_lookup() to get a suggestion for a certain
 * tag and prefix.
 *
 * Returns: (transfer none): a valid MooseStoreCompletion, do not free.
 */
MooseStoreCompletion *moose_store_get_completion(MooseStore *self);

/**
 * moose_store_get_client:
 * @self: a #MooseStore
 *
 * Get the client property of the store.
 *
 * Returns: (transfer none): The client.
 */
MooseClient *moose_store_get_client(MooseStore *self);

/**
 * moose_store_get_known_playlists:
 * @self: a #MooseStore
 *
 * Get all known playlists (i.e. all, not just loaded)
 *
 * Returns: (transfer container) (element-type utf8): List of Playlists names.
 */
GList *moose_store_get_known_playlists(MooseStore *self);

/**
 * moose_store_get_loaded_playlists:
 * @self: a #MooseStore
 *
 * Get all loadded playlists.
 *
 * Returns: (transfer container) (element-type utf8) : List of Playlists names.
 */
GList *moose_store_get_loaded_playlists(MooseStore *self);

G_END_DECLS

#endif /* end of include guard: MOOSE_STORE_H */
