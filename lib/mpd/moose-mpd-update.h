#include <mpd/client.h>
#include <glib.h>

#include "moose-song.h"
#include "moose-status.h"

/* Prototype because of cyclic dependency */
struct MooseClient;


/**
 * @brief Data Structured used for Updating Status/Song/Stats
 */
typedef struct MooseUpdateData {
    GAsyncQueue * event_queue;
    GThread * update_thread;
    struct MooseClient * client;
    GRecMutex
        mtx_current_song,
        mtx_status,
        mtx_statistics,
        mtx_outputs;

    MooseSong * current_song;
    struct mpd_stats * statistics;
    MooseStatus *status;

    struct {
        int timeout_id;
        int interval;
        bool trigger_event;
        GTimer * last_update;
        GMutex mutex;
    } status_timer;

    /* Support for blockingtillnextupdate */
    GCond sync_cond;
    GMutex sync_mtx;
    int sync_id;

    /* ID of the last played song or -1 */
    struct {
        long id;
        enum mpd_state state;
    } last_song_data;

} MooseUpdateData;

/**
 * @brief Allocate a new MooseUpdateData
 *
 * @param self The client to associate this structure to.
 *
 * @return a newly allocated structure, free with moose_update_data_destroy
 */
MooseUpdateData * moose_update_data_new(struct MooseClient * self);

/**
 * @brief Free the ressources allocated by moose_update_data_new
 *
 * @param data the allocated data
 */
void moose_update_data_destroy(MooseUpdateData * data);

/**
 * @brief Delete currentsong/status/stats and set them to NULL
 *
 * @param data UpdateData to do this on
 */
void moose_update_reset(MooseUpdateData * data);

/**
 * @brief Push a new event to the Update Thread.
 *
 * If the event is valid all corresponding fields will be updated.
 *
 * @param data the UpdateData to operate on
 * @param event a bitmask of mpd_idle events
 */
void moose_update_data_push(MooseUpdateData * data, enum mpd_idle event);

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
void moose_update_register_status_timer(
    struct MooseClient * self,
    int repeat_ms,
    bool trigger_event);

/**
 * @brief Stop a registered status timer
 *
 * @param self the client to operate on
 */
void moose_update_unregister_status_timer(struct MooseClient * self);

/**
 * @brief Check if the status timer is registered
 *
 * @param self the client to operate on
 *
 * @return True if active.
 */
bool moose_update_status_timer_is_active(struct MooseClient * self);

/**
 * @brief Block till next update finished.
 *
 * Useful for debugging/testing programs.
 *
 * @param data corresponding data
 */
void moose_update_block_till_sync(MooseUpdateData * data);
