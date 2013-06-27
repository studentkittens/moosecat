#include <mpd/client.h>
#include <glib.h>

/* Prototype because of cyclic dependency */
struct mc_Client;


/**
 * @brief Data Structured used for Updating Status/Song/Stats
 */
typedef struct mc_UpdateData {
    GAsyncQueue *event_queue;
    GThread *update_thread;
    struct mc_Client *client;
    GRecMutex data_set_mutex;
} mc_UpdateData;


/**
 * @brief Allocate a new mc_UpdateData 
 *
 * @param self The client to associate this structure to.
 *
 * @return a newly allocated structure, free with mc_proto_update_data_destroy
 */
mc_UpdateData * mc_proto_update_data_new(struct mc_Client *self);


/**
 * @brief Free the ressources allocated by mc_proto_update_data_new
 *
 * @param data the allocated data
 */
void mc_proto_update_data_destroy(mc_UpdateData *data);

/**
 * @brief 
 *
 * @param data
 * @param event
 */
void mc_proto_update_data_push(mc_UpdateData *data, enum mpd_idle event);


void mc_proto_update_register_status_timer(
    struct mc_Client *self,
    int repeat_ms,
    bool trigger_event);

/**
 * @brief Stop a registered status timer
 *
 * NOT THREADSAFE
 *
 * @param self the client to operate on
 */
void mc_proto_update_unregister_status_timer(struct mc_Client *self);


/**
 * @brief 
 *
 * TODO: Make these threadsafe?
 *
 * NOT THREADSAFE
 *
 * @param self
 *
 * @return 
 */
bool mc_proto_update_status_timer_is_active(struct mc_Client *self);


/* LOCKING */

void mc_update_data_open(struct mc_Client *self);
void mc_update_data_close(struct mc_Client *self);


void mc_lock_status(struct mc_Client *self);
void mc_unlock_status(struct mc_Client *self);
void mc_lock_stats(struct mc_Client *self);
void mc_unlock_stats(struct mc_Client *self);
void mc_lock_current_song(struct mc_Client *self);
void mc_unlock_current_song(struct mc_Client *self);
