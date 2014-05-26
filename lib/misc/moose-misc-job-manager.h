#ifndef MOOSE_JOB_MANAGER_H
#define MOOSE_JOB_MANAGER_H

#include <stdbool.h>

/**
 * @brief A class to implement a Job Queue with cancellation and Priority in a
 * dead simple way.
 */
struct MooseJobManager;

/**
 * @brief Callback that is called when one job is executed.
 *
 * @param jm: The jobmanager structure.
 * @param cancel: a pointer to a bollean that can be checked with moose_jm_check_cancel()
 * @param user_data: Data that can be passed to moose_jm_send()
 */
typedef void * (* MooseJobManagerCallback)(
    struct MooseJobManager * jm,    /* JobManger executing the Job                                  */
    volatile bool * cancel,       /* Pointer to check periodically if the operation was cancelled */
    void * user_data,             /* user data passed to the executor                             */
    void * job_data               /* user data passed to the executor                             */
);

/**
 * @brief Create a new JobManger instance.
 *
 * @param on_execute: Callback to call on execution of a job (may not be NULL.)
 * @param user_data: Userdata that is passed to the callback once per job manager.
 *
 * @return a newly allocated MooseJobManager, pass to moose_store_close() when done.
 */
struct MooseJobManager * moose_jm_create(MooseJobManagerCallback on_execute, void * user_data);

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
 * @return bool if you should cancel your operation.
 */
bool moose_jm_check_cancel(struct MooseJobManager * jm, volatile bool * cancel);

/**
 * @brief Send a new Job to the Manger.
 *
 * @param jm the job manger to send jobs to.
 * @param priority Priority from -INT_MAX to INT_MAX.
 * @param user_data user_data to be passed to the on_exec callback.
 *
 * @return a unique integer, being the id of the job.
 */
long moose_jm_send(struct MooseJobManager * jm, int priority, void * user_data);

/**
 * @brief Blocks until the internal Queue is empty. (No jobs currently processed)
 *
 * @param jm: Jobmanager to wait on.
 */
void moose_jm_wait(struct MooseJobManager * jm);

/**
 * @brief Wait for the Job specified by ID to finish.
 *
 * @param jm The job manager you started the job on.
 * @param job_id the ID obtained by moose_jm_send()
 */
void moose_jm_wait_for_id(struct MooseJobManager * jm, int job_id);

/**
 * @brief Get the result of a job.
 *
 * Note: NULL might be returned either if you really returned NULL,
 *       or if the result is not computed yet!
 *
 * @param jm The job manager to operate on.
 * @param job_id Id of the job, obtained from moose_jm_send()
 *
 * @return the void * pointer returned by your callback.
 */
void * moose_jm_get_result(struct MooseJobManager * jm, int job_id);

/**
 * @brief Free all data associated with this Job Manager.
 *
 * Note: This will send a termination job, with highest priority.
 *       Call moose_jm_wait() before if you want to wait for the jobs to finish.
 *
 * @param jm Job Manager to close.
 */
void moose_jm_close(struct MooseJobManager * jm);

#endif /* end of include guard: MOOSE_JOB_MANAGER_H */
