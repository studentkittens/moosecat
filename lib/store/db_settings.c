#include <glib.h>
#include "db_settings.h"

mc_StoreSettings * mc_store_settings_new (void)
{
    mc_StoreSettings * self = g_new0 (mc_StoreSettings, 1);
    g_assert (self);

    self->use_memory_db = true;
    self->use_compression = true;
    self->db_directory = NULL;
    self->tokenizer = "porter";

    return self;
}

///////////////////////////////

void mc_store_settings_destroy (mc_StoreSettings * self)
{
    g_free (self);
}
