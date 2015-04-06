#ifndef MOOSE_JOB_MANAGER_H
#define MOOSE_JOB_MANAGER_H

/*
 * A class to implement a Job Queue with cancellation and Priority in a
 * dead simple way.
 */

#include <glib-object.h>

G_BEGIN_DECLS

/*
 * Type macros.
 */
#define MOOSE_TYPE_JOB_MANAGER (moose_job_manager_get_type())
#define MOOSE_JOB_MANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_JOB_MANAGER, MooseJobManager))
#define MOOSE_IS_JOB_MANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_JOB_MANAGER))
#define MOOSE_JOB_MANAGER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_JOB_MANAGER, MooseJobManagerClass))
#define MOOSE_IS_JOB_MANAGER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_JOB_MANAGER))
#define MOOSE_JOB_MANAGER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_JOB_MANAGER, MooseJobManagerClass))

GType moose_job_manager_get_type(void);

struct _MooseJobManagerPrivate;

typedef struct _MooseJobManager {
    GObject parent;
    struct _MooseJobManagerPrivate *priv;
} MooseJobManager;

typedef struct _MooseJobManagerClass { GObjectClass parent; } MooseJobManagerClass;

/**
 * moose_job_manager_new:
 *
 * Create a new JobManger instance.
 *
 * Returns: a newly allocated MooseJobManager, pass to moose_job_manager_unref() when
 *done.
 */
MooseJobManager *moose_job_manager_new(void);

/**
 * moose_job_manager_check_cancel:
 * @jm: a #MooseJobManager
 * @cancel: the cancel pointer you received from the callback.
 *
 * Check in a callback is this job should be cancelled.  Note: Do not try to
 * dereference cancel yourself, this can cause data-races!
 *
 * Check in your operation callback on strategical position for cancellation.
 *
 * Returns: if you should cancel your operation.
 */
gboolean moose_job_manager_check_cancel(MooseJobManager *jm, volatile gboolean *cancel);

/**
 * moose_job_manager_send:
 * @jm: a #MooseJobManager
 * @priority: Priority from -INT_MAX to INT_MAX.
 * @user_data: user_data to be passed to the on_exec callback.
 *
 * Send a new Job to the Manger.
 *
 * Returns: a unique integer, being the id of the job.
 */
long moose_job_manager_send(MooseJobManager *jm, int priority, void *user_data);

/**
 * moose_job_manager_wait:
 * @jm: a #MooseJobManager
 *
 * Blocks until the internal Queue is empty. (== No jobs currently processed)
 */
void moose_job_manager_wait(MooseJobManager *jm);

/**
 * moose_job_manager_wait_for_id:
 * @jm: a #MooseJobManager
 * @job_id: the ID obtained by moose_job_manager_send()
 *
 * Wait for the `job_id` to finish.
 */
void moose_job_manager_wait_for_id(MooseJobManager *jm, int job_id);

/**
 * moose_job_manager_get_result:
 * @jm: a #MooseJobManager
 * @job_id: Id of the job, obtained from moose_job_manager_send()
 *
 * Get the result of a job.
 * Note: NULL might be returned either if you really returned NULL,
 *       or if the result is not computed yet!
 *
 * Returns: (transfer none): the void * pointer returned by your callback.
 */
void *moose_job_manager_get_result(MooseJobManager *jm, int job_id);

/**
 * moose_job_manager_unref:
 * @jm: a #MooseJobManager
 *
 * Free all data associated with this Job Manager.
 *
 * Note: This will send a termination job, with highest priority.
 *       Call moose_job_manager_wait() before if you want to wait for the jobs to finish.
 */
void moose_job_manager_unref(MooseJobManager *jm);

G_END_DECLS

#endif /* end of include guard: MOOSE_JOB_MANAGER_H */
