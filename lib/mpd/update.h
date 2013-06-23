#include <mpd/client.h>
#include <glib.h>

/* Prototype because of cyclic dependency */
struct mc_Client;


typedef struct mc_UpdateData {
    GAsyncQueue *event_queue;
    GThread *update_thread;
    struct mc_Client *client;
} mc_UpdateData;


mc_UpdateData * mc_proto_update_data_new(struct mc_Client *self);
void mc_proto_update_data_destroy(mc_UpdateData *data);
void mc_proto_update_data_push(mc_UpdateData *data, enum mpd_idle event);

/**
 * @brief Update status/stats/song.
 *
 * The function looks like that, in order to be registered as event callback.
 *
 * @param events the happenene event. Use INT_MAX to update all events.
 * @param user_data not used. pass NULL.
 */
//void mc_proto_update_context_info_cb(struct mc_Client *self, enum mpd_idle events, void *user_data);

void mc_proto_update_register_status_timer(
    struct mc_Client *self,
    int repeat_ms,
    bool trigger_event);

/**
 * @brief Stop a registered status timer
 *
 * @param self the client to operate on
 */
void mc_proto_update_unregister_status_timer(struct mc_Client *self);

bool mc_proto_update_status_timer_is_active(struct mc_Client *self);

void mc_lock_status(struct mc_Client *self);
void mc_unlock_status(struct mc_Client *self);
void mc_lock_stats(struct mc_Client *self);
void mc_unlock_stats(struct mc_Client *self);
void mc_lock_current_song(struct mc_Client *self);
void mc_unlock_current_song(struct mc_Client *self);
