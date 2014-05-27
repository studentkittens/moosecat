#include "moose-misc-job-manager.h"

#include <glib.h>


typedef struct {
    /* Priority of the job from -INT_MAX to INT_MAX */
    volatile int priority;

    /* Flag indication the callback function if it should cancel */
    volatile bool cancel;

    /* User data passed to the callback function */
    gpointer job_data;

    /* Integer ID of the Job (incrementing up from 0) */
    long id;

} MooseJob;



struct MooseJobManager {
    /* Pointer to currently executing job */
    MooseJob * current_job;

    /* Const value used to check for as termination value (with max Priority) */
    MooseJob terminator;

    /* Mesasge Queue between control and executor */
    GAsyncQueue * job_queue;

    /* Thread moose_jm_executor runs in */
    GThread * execute_thread;

    /* Callback to call on execute */
    MooseJobManagerCallback callback;

    /* CondVar that gets signaled on every finished job */
    GCond finish_cond;

    /* Mutex that is locked/unlocked with finish_cond */
    GMutex finish_mutex;

    /* Mutex to protect the current job member */
    GMutex current_job_mutex;

    /* Mutex to protext access to the hash table */
    GMutex hash_table_mutex;

    /* A hashtable of the results, with a job id as key and a void* as value */
    GHashTable * results;

    /* Job IDs are created by incrementing this counter */
    long job_id_counter;

    /* Mutex to protext job id counter */
    GMutex job_id_counter_mutex;

    /* The ID of the most recently finished job (-1 initially) */
    long last_finished_job;

    /* The ID of the most recently send job (-1 initially) */
    long last_send_job;

    /* User data that is passed to callback (settable per manager) */
    gpointer user_data;
};


//                       //
//      Job Functions    //
//                       //


static MooseJob * moose_job_create(struct MooseJobManager * jm) {
    MooseJob * job = g_new0(MooseJob, 1);
    g_mutex_lock(&jm->job_id_counter_mutex);
    job->id = (jm->job_id_counter)++;
    g_mutex_unlock(&jm->job_id_counter_mutex);
    return job;
}



static void moose_job_free(MooseJob * job) {
    g_free(job);
}


//                       //
//  Jobmanager Functions //
//                       //


static int moose_jm_prio_sort_func(gconstpointer a, gconstpointer b, gpointer job_data) {
    (void)job_data;

    const MooseJob * ja = a, * jb = b;
    if (ja->priority > jb->priority) {
        return +1;
    }

    if (ja->priority < jb->priority) {
        return -1;
    }

    if (ja->id > jb->id) {
        return +1;
    }

    if (ja->id < jb->id) {
        return -1;
    }

    return 0;
}



static gpointer moose_jm_executor(gpointer data) {
    MooseJob * job;
    struct MooseJobManager * jm = data;

    while ((job = g_async_queue_pop(jm->job_queue)) != &jm->terminator) {
        bool is_already_canceled = false;

        /* Check if the previous job needs to be freed.
         * Also remember the current one and check if its
         * already cancelled */
        g_mutex_lock(&jm->current_job_mutex);
        {
            /* Free previous job (last job is freed in moose_jm_close() */
            if (jm->current_job != NULL) {
                moose_job_free(jm->current_job);
            }

            jm->current_job = job;
            is_already_canceled = job->cancel;
            g_mutex_unlock(&jm->current_job_mutex);

            /* Do actual job */
            if (is_already_canceled == false) {
                void * item = jm->callback(jm, &job->cancel, jm->user_data, job->job_data);

                g_mutex_lock(&jm->hash_table_mutex);
                {
                    g_hash_table_insert(jm->results, GINT_TO_POINTER(job->id), item);
                }
                g_mutex_unlock(&jm->hash_table_mutex);
            }
        }

        /* Signal that a job was finished */
        g_mutex_lock(&jm->finish_mutex);
        {
            jm->last_finished_job = job->id;
            g_cond_broadcast(&jm->finish_cond);
        }
        g_mutex_unlock(&jm->finish_mutex);
    }

    return NULL;
}



struct MooseJobManager * moose_jm_create(MooseJobManagerCallback on_execute, gpointer user_data) {
    struct MooseJobManager * jm = g_new0(struct MooseJobManager, 1);

    /* Initialize Synchronisation Primitives */
    g_cond_init(&jm->finish_cond);
    g_mutex_init(&jm->finish_mutex);
    g_mutex_init(&jm->current_job_mutex);
    g_mutex_init(&jm->hash_table_mutex);
    g_mutex_init(&jm->job_id_counter_mutex);

    /* Save arguments */
    jm->callback = on_execute;
    jm->job_queue = g_async_queue_new();
    jm->terminator.priority = -INT_MAX;
    jm->results = g_hash_table_new(g_direct_hash, g_direct_equal);
    jm->user_data = user_data;

    /* Job ids */
    jm->last_finished_job = -1;
    jm->last_send_job = -1;

    /* Keep the thread running in the background */
    jm->execute_thread = g_thread_new("job-execute-thread", moose_jm_executor, jm);
    return jm;
}



bool moose_jm_check_cancel(struct MooseJobManager * jm, volatile bool * cancel) {
    bool rc = false;

    if (jm && cancel) {
        g_mutex_lock(&jm->current_job_mutex);
        {
            rc = *cancel;
        }
        g_mutex_unlock(&jm->current_job_mutex);
    }

    return rc;
}



long moose_jm_send(struct MooseJobManager * jm, int priority, gpointer job_data) {
    if (jm == NULL) {
        return -1;
    }

    /* Create a new job, with a unique job-id */
    MooseJob * job = moose_job_create(jm);
    job->priority = priority;
    job->job_data = job_data;

    /* Lock the current job structure, since it gets read in the main thread */
    g_mutex_lock(&jm->current_job_mutex);
    {
        if (jm->current_job != NULL) {
            if (jm->current_job->priority > priority) {
                jm->current_job->cancel = true;
            }
        }

        /* Remember the last sended job */
        jm->last_send_job = job->id;
    }
    g_mutex_unlock(&jm->current_job_mutex);

    /* Push the item sorted with priority (small prio comes earlier) */
    g_async_queue_push_sorted(jm->job_queue, job, moose_jm_prio_sort_func, NULL);

    /* Return the Job ID, so users can get the result later */
    return job->id;
}



void moose_jm_wait(struct MooseJobManager * jm) {
    if (jm == NULL) {
        return;
    }

    g_mutex_lock(&jm->finish_mutex);
    {
        for (;;) {
            /* If there are no jobs in the Queue we can
             * expect that we're finished for now */
            if (g_async_queue_length(jm->job_queue) <= 0) {
                break;
            } else {
                g_cond_wait(&jm->finish_cond, &jm->finish_mutex);
            }
        }
    }
    g_mutex_unlock(&jm->finish_mutex);
}



void moose_jm_wait_for_id(struct MooseJobManager * jm, int job_id) {
    if (jm == NULL || job_id < 0) {
        /* Well, this is just down right invalid. */
        return;
    }

    if (job_id > jm->last_send_job) {
        /* Was not sended yet, also no need to wait */
        return;
    }

    bool has_key = false;

    /* Lookup if the results hash table already has this key */
    g_mutex_lock(&jm->hash_table_mutex);
    {
        has_key = g_hash_table_contains(jm->results, GINT_TO_POINTER(job_id));
    }
    g_mutex_unlock(&jm->hash_table_mutex);

    if (has_key == true) {
        /* Result was already computed */
        return;
    }

    /* Otherwise wait till we get notified upon execution */
    g_mutex_lock(&jm->finish_mutex);
    {
        for (;;) {
            if (jm->last_finished_job == job_id) {
                break;
            } else {
                g_cond_wait(&jm->finish_cond, &jm->finish_mutex);
            }
        }
    }
    g_mutex_unlock(&jm->finish_mutex);
}



void * moose_jm_get_result(struct MooseJobManager * jm, int job_id) {
    void * result = NULL;

    if (jm != NULL) {
        /* Lock the hashtable and lookup the result */
        g_mutex_lock(&jm->hash_table_mutex);
        {
            result = g_hash_table_lookup(jm->results, GINT_TO_POINTER(job_id));
        }
        g_mutex_unlock(&jm->hash_table_mutex);
    }

    return result;
}



void moose_jm_close(struct MooseJobManager * jm) {
    if (jm == NULL) {
        return;
    }

    /* Send a terminating job to the Queue and wait for it finish */
    g_async_queue_push(jm->job_queue, (gpointer) & jm->terminator);
    g_thread_join(jm->execute_thread);

    /* Free ressources */
    g_async_queue_unref(jm->job_queue);

    g_cond_clear(&jm->finish_cond);
    g_mutex_clear(&jm->finish_mutex);
    g_mutex_clear(&jm->current_job_mutex);
    g_mutex_clear(&jm->hash_table_mutex);
    g_mutex_clear(&jm->job_id_counter_mutex);

    if (jm->current_job != NULL) {
        moose_job_free(jm->current_job);
    }

    g_hash_table_destroy(jm->results);
    g_free(jm);
}
