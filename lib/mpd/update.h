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
    GRecMutex 
        mtx_current_song,
        mtx_status,
        mtx_statistics,
        mtx_outputs;

    struct mpd_song *current_song;
    struct mpd_stats *statistics;
    struct mpd_status *status;
    const char *replay_gain_status;

    struct {
        int timeout_id;
        int interval;
        bool trigger_event;
        GTimer *last_update;
        bool reset_timer;
        GMutex mutex;
    } status_timer;

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
 * @brief Delete currentsong/status/stats and set them to NULL
 *
 * @param data UpdateData to do this on 
 */
void mc_proto_update_reset(mc_UpdateData *data);

/**
 * @brief Push a new event to the Update Thread.
 *
 * If the event is valid all corresponding fields will be updated.
 *
 * @param data the UpdateData to operate on 
 * @param event a bitmask of mpd_idle events 
 */
void mc_proto_update_data_push(mc_UpdateData *data, enum mpd_idle event);

/**
 * @brief Activate the Status Timer.
 *
 * It will repeatedly update the status if in a certain interval.
 * Normal updates on event base are taking into account.
 *
 * @param self the Client to activate status updates on
 * @param repeat_ms Interval in ms
 * @param trigger_event If true the client-event callbacks are invoked 
 */
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

/**
 * @brief Check if the status timer is registered 
 *
 * @param self the client to operate on
 *
 * @return True if active.
 */
bool mc_proto_update_status_timer_is_active(struct mc_Client *self);


/* LOCKING */

struct mpd_status * mc_lock_status(struct mc_Client *self);
void mc_unlock_status(struct mc_Client *self);
struct mpd_stats * mc_lock_statistics(struct mc_Client *self);
void mc_unlock_statistics(struct mc_Client *self);
struct mpd_song * mc_lock_current_song(struct mc_Client *self);
void mc_unlock_current_song(struct mc_Client *self);
void mc_lock_outputs(struct mc_Client *self);
void mc_unlock_outputs(struct mc_Client *self);
