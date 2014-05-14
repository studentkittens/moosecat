#include <glib.h>
#include "moose-store-settings.h"

MooseStoreSettings * moose_store_settings_new(void)
{
    MooseStoreSettings * self = g_new0(MooseStoreSettings, 1);
    g_assert(self);
    self->use_memory_db = true;
    self->use_compression = true;
    self->db_directory = NULL;
    self->tokenizer = "porter";
    return self;
}

///////////////////////////////

void moose_store_settings_destroy(MooseStoreSettings * self)
{
    g_free(self);
}

///////////////////////////////

void moose_store_settings_set_db_directory(MooseStoreSettings * self, const char * db_directory)
{
    g_assert(self);

    self->db_directory = g_strdup(db_directory);
}
