#ifndef MOOSE_JOB_MANAGER_H
#define MOOSE_JOB_MANAGER_H

/**
 * @brief A class to implement a Job Queue with cancellation and Priority in a
 * dead simple way.
 */

#include <glib-object.h>

G_BEGIN_DECLS

/*
 * Type macros.
 */
#define MOOSE_TYPE_JOB_MANAGER \
    (moose_job_manager_get_type())
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

typedef struct _MooseJobManagerClass {
    GObjectClass parent;
} MooseJobManagerClass;

/**
 * @brief Create a new JobManger instance.
 *
 * @param on_execute: Callback to call on execution of a job (may not be NULL.)
 * @param user_data: Userdata that is passed to the callback once per job manager.
 *
 * @return a newly allocated MooseJobManager, pass to moose_store_unref() when done.
 */
MooseJobManager * moose_job_manager_new(void);

/**
 * @brief Check in a callback is this job should be cancelled.
 *
 * Note: Do not try to dereference cancel yourself, this can cause data-races!
 *
 * Check in your operation callback on strategical position for cancellation.
 *
 * @param jm: the jobmanger the job is running on.
 * @param cancel the cancel pointer you received from the callback.
 *
 * @return gboolean if you should cancel your operation.
 */
gboolean moose_job_manager_check_cancel(MooseJobManager * jm, volatile gboolean * cancel);

/**
 * @brief Send a new Job to the Manger.
 *
 * @param jm the job manger to send jobs to.
 * @param priority Priority from -INT_MAX to INT_MAX.
 * @param user_data user_data to be passed to the on_exec callback.
 *
 * @return a unique integer, being the id of the job.
 */
long moose_job_manager_send(MooseJobManager * jm, int priority, void * user_data);

/**
 * @brief Blocks until the internal Queue is empty. (No jobs currently processed)
 *
 * @param jm: Jobmanager to wait on.
 */
void moose_job_manager_wait(MooseJobManager * jm);

/**
 * @brief Wait for the Job specified by ID to finish.
 *
 * @param jm The job manager you started the job on.
 * @param job_id the ID obtained by moose_job_manager_send()
 */
void moose_job_manager_wait_for_id(MooseJobManager * jm, int job_id);

/**
 * @brief Get the result of a job.
 *
 * Note: NULL might be returned either if you really returned NULL,
 *       or if the result is not computed yet!
 *
 * @param jm The job manager to operate on.
 * @param job_id Id of the job, obtained from moose_job_manager_send()
 *
 * @return the void * pointer returned by your callback.
 */
void * moose_job_manager_get_result(MooseJobManager * jm, int job_id);

/**
 * @brief Free all data associated with this Job Manager.
 *
 * Note: This will send a termination job, with highest priority.
 *       Call moose_job_manager_wait() before if you want to wait for the jobs to finish.
 *
 * @param jm Job Manager to close.
 */
void moose_job_manager_unref(MooseJobManager * jm);

G_END_DECLS

#endif /* end of include guard: MOOSE_JOB_MANAGER_H */
