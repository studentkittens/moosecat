#ifndef MC_DB_SETTINGS_HH
#define MC_DB_SETTINGS_HH

#include <stdbool.h>

typedef struct {
    /* Open the database entireyl in memory.
     * (i.e. use ":memory" as db name).
     *
     * Adv: Little bit faster on few systems.
     * Dis: Higher memory usage. (~5-10 MB)
     *
     * Default: True.
     */
    bool use_memory_db;

    /* Compress / Uncompress database
     * on close/startup.
     *
     * Adv: Less HD usage (12 MB vs. 3 MB)
     * Dis: Little bit slower startup.
     *
     * Default: True.
     */
    bool use_compression;

    /* Directory where the database shall be stored in.
     *
     * Default: NULL, which is equal to "./"
     */
    const char *db_directory;

    /* Tokenizer algorithm to use to split words. NULL == default == "porter"
     * See: http://www.sqlite.org/fts3.html#tokenizer
     * Supported: see MC_STORE_SUPPORTED_TOKENIZERS in db_private.h
     *
     * Default: "porter", which is able to match "Smile" with "Smiling"
     */
    const char *tokenizer;

} MooseStoreSettings;

/* Create a new settings object with default settings.
 * Likely, you only want to change the directory.
 * Note: You are responsible for managing memory.
 *        moose_store_settings_destroy will only free
 *        memomry that was allocated by moose_store_settings_new.
 */
MooseStoreSettings *moose_store_settings_new(void);

/* Free a MooseStoreSettings.
 * You do not have to call this yourself normally.
 * If a settings object is attached to a Store,
 * it will be closed on closedown of the store.
 */
void moose_store_settings_destroy(MooseStoreSettings *settings);

void moose_store_settings_set_db_directory(MooseStoreSettings *self, const char *db_directory);


#endif /* end of include guard: MC_DB_SETTINGS_HH */
