#ifndef MC_JOB_MANAGER_H
#define MC_JOB_MANAGER_H

#include <stdbool.h>

/**
 * @brief A class to implement a Job Queue with cancellation and Priority in a
 * dead simple way.
 */
struct mc_JobManager;

/**
 * @brief Callback that is called when one job is executed.
 *
 * @param jm: The jobmanager structure.
 * @param cancel: a pointer to a bollean that can be checked with mc_jm_check_cancel()
 * @param user_data: Data that can be passed to mc_jm_send()
 */
typedef void * (* mc_JobManagerCallback)(
        struct mc_JobManager *jm, /* JobManger executing the Job                                  */
        volatile bool *cancel,    /* Pointer to check periodically if the operation was cancelled */
        void *user_data           /* user data passed to the executor                             */
);

/**
 * @brief Create a new JobManger instance.
 *
 * @param on_execute: Callback to call on execution of a job (may not be NULL.)
 *
 * @return a newly allocated mc_JobManager, pass to mc_store_close() when done.
 */
struct mc_JobManager *mc_jm_create(mc_JobManagerCallback on_execute);

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
bool mc_jm_check_cancel(struct mc_JobManager *jm, volatile bool *cancel);

/**
 * @brief Send a new Job to the Manger.
 *
 * @param jm the job manger to send jobs to.
 * @param priority Priority from -INT_MAX to INT_MAX. 
 * @param user_data user_data to be passed to the on_exec callback.
 *
 * @return a unique integer, being the id of the job.
 */
int mc_jm_send(struct mc_JobManager *jm, int priority, void *user_data);

/**
 * @brief Blocks until the internal Queue is empty. (No jobs currently processed)
 *
 * @param jm: Jobmanager to wait on.
 */
void mc_jm_wait(struct mc_JobManager *jm);

/**
 * @brief Wait for the Job specified by ID to finish.
 *
 * @param jm The job manager you started the job on.
 * @param job_id the ID obtained by mc_jm_send()
 */
void mc_jm_wait_for_id(struct mc_JobManager *jm, int job_id);

/**
 * @brief Get the result of a job.
 *
 * Note: NULL might be returned either if you really returned NULL,
 *       or if the result is not computed yet!
 *
 * @param jm The job manager to operate on.
 * @param job_id Id of the job, obtained from mc_jm_send()
 *
 * @return the void * pointer returned by your callback.
 */
void * mc_jm_get_result(struct mc_JobManager *jm, int job_id);

/**
 * @brief Free all data associated with this Job Manager.
 *
 * Note: This will send a termination job, with highest priority.
 *       Call mc_jm_wait() before if you want to wait for the jobs to finish.
 *  
 * @param jm Job Manager to close.
 */
void mc_jm_close(struct mc_JobManager *jm);

#endif /* end of include guard: MC_JOB_MANAGER_H */

