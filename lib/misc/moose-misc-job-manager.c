#include "moose-misc-job-manager.h"

#include <glib.h>

typedef struct _MooseJob {
    /* Priority of the job from -INT_MAX to INT_MAX */
    volatile int priority;

    /* Flag indication the callback function if it should cancel */
    volatile gboolean cancel;

    /* User data passed to the callback function */
    gpointer job_data;

    /* Integer ID of the Job (incrementing up from 0) */
    long id;
} MooseJob;

typedef struct _MooseJobManagerPrivate {
    /* Pointer to currently executing job */
    MooseJob * current_job;

    /* Const value used to check for as termination value (with max Priority) */
    MooseJob terminator;

    /* Mesasge Queue between control and executor */
    GAsyncQueue * job_queue;

    /* Thread moose_job_manager_executor runs in */
    GThread * execute_thread;

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
} MooseJobManagerPrivate;

enum {
    SIGNAL_DISPATCH,
    NUM_SIGNALS
};

static guint SIGNALS[NUM_SIGNALS];


G_DEFINE_TYPE_WITH_PRIVATE(
    MooseJobManager, moose_job_manager, G_TYPE_OBJECT
);

static MooseJob * moose_job_create(MooseJobManager * jm) {
    MooseJob * job = g_new0(MooseJob, 1);
    g_mutex_lock(&jm->priv->job_id_counter_mutex);
    job->id = (jm->priv->job_id_counter)++;
    g_mutex_unlock(&jm->priv->job_id_counter_mutex);
    return job;
}

static void moose_job_free(MooseJob * job) {
    g_free(job);
}

static int moose_job_manager_prio_sort_func(gconstpointer a, gconstpointer b, gpointer job_data) {
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

static gpointer moose_job_manager_executor(gpointer data) {
    MooseJob * job;
    MooseJobManager * jm = MOOSE_JOB_MANAGER(data);
    struct _MooseJobManagerPrivate *priv = jm->priv;

    while ((job = g_async_queue_pop(jm->priv->job_queue)) != &priv->terminator) {
        gboolean is_already_canceled = FALSE;

        /* Check if the previous job needs to be freed.
         * Also remember the current one and check if its
         * already cancelled */
        g_mutex_lock(&priv->current_job_mutex);
        {
            /* Free previous job (last job is freed in moose_job_manager_unref() */
            if (priv->current_job != NULL) {
                moose_job_free(priv->current_job);
            }

            priv->current_job = job;
            is_already_canceled = job->cancel;
            g_mutex_unlock(&priv->current_job_mutex);

            /* Do actual job */
            if (is_already_canceled == FALSE) {
                void *item = NULL;
                g_signal_emit(
                    jm, SIGNALS[SIGNAL_DISPATCH], 0,
                    &job->cancel,
                    job->job_data,
                    &item
                );
                // void * item = priv->callback(jm, &job->cancel, priv->user_data, job->job_data);

                g_mutex_lock(&priv->hash_table_mutex);
                {
                    g_hash_table_insert(priv->results, GINT_TO_POINTER(job->id), item);
                }
                g_mutex_unlock(&priv->hash_table_mutex);
            }
        }

        /* Signal that a job was finished */
        g_mutex_lock(&priv->finish_mutex);
        {
            priv->last_finished_job = job->id;
            g_cond_broadcast(&priv->finish_cond);
        }
        g_mutex_unlock(&priv->finish_mutex);
    }

    return NULL;
}

MooseJobManager * moose_job_manager_new(void) {
    return g_object_new(MOOSE_TYPE_JOB_MANAGER, NULL);
}

gboolean moose_job_manager_check_cancel(MooseJobManager * jm, volatile gboolean * cancel) {
    gboolean rc = FALSE;

    if (jm && cancel) {
        g_mutex_lock(&jm->priv->current_job_mutex);
        {
            rc = *cancel;
        }
        g_mutex_unlock(&jm->priv->current_job_mutex);
    }

    return rc;
}

long moose_job_manager_send(MooseJobManager * jm, int priority, gpointer job_data) {
    if (jm == NULL) {
        return -1;
    }

    MooseJobManagerPrivate *priv = jm->priv;

    /* Create a new job, with a unique job-id */
    MooseJob * job = moose_job_create(jm);
    job->priority = priority;
    job->job_data = job_data;

    /* Lock the current job structure, since it gets read in the main thread */
    g_mutex_lock(&priv->current_job_mutex);
    {
        if (priv->current_job != NULL) {
            if (priv->current_job->priority > priority) {
                priv->current_job->cancel = TRUE;
            }
        }

        /* Remember the last sended job */
        priv->last_send_job = job->id;
    }
    g_mutex_unlock(&priv->current_job_mutex);

    /* Push the item sorted with priority (small prio comes earlier) */
    g_async_queue_push_sorted(priv->job_queue, job, moose_job_manager_prio_sort_func, NULL);

    /* Return the Job ID, so users can get the result later */
    return job->id;
}

void moose_job_manager_wait(MooseJobManager * jm) {
    if (jm == NULL) {
        return;
    }

    MooseJobManagerPrivate *priv = jm->priv;

    g_mutex_lock(&priv->finish_mutex);
    {
        for (;;) {
            /* If there are no jobs in the Queue we can
             * expect that we're finished for now */
            if (g_async_queue_length(priv->job_queue) <= 0) {
                break;
            } else {
                g_cond_wait(&priv->finish_cond, &priv->finish_mutex);
            }
        }
    }
    g_mutex_unlock(&priv->finish_mutex);
}

void moose_job_manager_wait_for_id(MooseJobManager * jm, int job_id) {
    if (jm == NULL || job_id < 0) {
        /* Well, this is just down right invalid. */
        return;
    }

    MooseJobManagerPrivate *priv = jm->priv;

    if (job_id > priv->last_send_job) {
        /* Was not sended yet, also no need to wait */
        return;
    }

    gboolean has_key = FALSE;

    /* Lookup if the results hash table already has this key */
    g_mutex_lock(&priv->hash_table_mutex);
    {
        has_key = g_hash_table_contains(priv->results, GINT_TO_POINTER(job_id));
    }
    g_mutex_unlock(&priv->hash_table_mutex);

    if (has_key == TRUE) {
        /* Result was already computed */
        return;
    }

    /* Otherwise wait till we get notified upon execution */
    g_mutex_lock(&priv->finish_mutex);
    {
        for (;;) {
            if (priv->last_finished_job == job_id) {
                break;
            } else {
                g_cond_wait(&priv->finish_cond, &priv->finish_mutex);
            }
        }
    }
    g_mutex_unlock(&priv->finish_mutex);
}

void * moose_job_manager_get_result(MooseJobManager * jm, int job_id) {
    void * result = NULL;

    if (jm != NULL) {
        /* Lock the hashtable and lookup the result */
        g_mutex_lock(&jm->priv->hash_table_mutex);
        {
            result = g_hash_table_lookup(jm->priv->results, GINT_TO_POINTER(job_id));
        }
        g_mutex_unlock(&jm->priv->hash_table_mutex);
    }

    return result;
}

void moose_job_manager_unref(MooseJobManager * jm) {
    if (jm != NULL) {
        g_object_unref(jm);
    }
}

static void moose_job_manager_finalize(GObject * gobject) {
    MooseJobManager * self = MOOSE_JOB_MANAGER(gobject);
    MooseJobManagerPrivate *priv = self->priv;

    /* Send a terminating job to the Queue and wait for it finish */
    g_async_queue_push(priv->job_queue, (gpointer) &priv->terminator);
    g_thread_join(priv->execute_thread);

    /* Free ressources */
    g_async_queue_unref(priv->job_queue);

    g_cond_clear(&priv->finish_cond);
    g_mutex_clear(&priv->finish_mutex);
    g_mutex_clear(&priv->current_job_mutex);
    g_mutex_clear(&priv->hash_table_mutex);
    g_mutex_clear(&priv->job_id_counter_mutex);

    if (priv->current_job != NULL) {
        moose_job_free(priv->current_job);
    }

    g_hash_table_destroy(priv->results);

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

static void moose_job_manager_class_init(MooseJobManagerClass * klass) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_job_manager_finalize;

    SIGNALS[SIGNAL_DISPATCH] = g_signal_new("dispatch",
                                            G_TYPE_FROM_CLASS(klass),
                                            G_SIGNAL_RUN_FIRST,
                                            0,
                                            NULL /* accumulator */,
                                            NULL /* accumulator data */,
                                            NULL, /* generic marshaller */
                                            G_TYPE_POINTER /* return_type */,
                                            2 /* n_params */,
                                            G_TYPE_POINTER,
                                            G_TYPE_POINTER
                                           );
}


static void moose_job_manager_init(MooseJobManager * self) {
    MooseJobManagerPrivate *priv = self->priv = moose_job_manager_get_instance_private(self);

    /* Initialize Synchronisation Primitives */
    g_cond_init(&priv->finish_cond);
    g_mutex_init(&priv->finish_mutex);
    g_mutex_init(&priv->current_job_mutex);
    g_mutex_init(&priv->hash_table_mutex);
    g_mutex_init(&priv->job_id_counter_mutex);

    /* Save arguments */
    priv->job_queue = g_async_queue_new();
    priv->terminator.priority = -INT_MAX;
    priv->results = g_hash_table_new(g_direct_hash, g_direct_equal);

    /* Job ids */
    priv->last_finished_job = -1;
    priv->last_send_job = -1;

    /* Keep the thread running in the background */
    priv->execute_thread = g_thread_new(
                               "job-execute-thread", moose_job_manager_executor, priv
                           );
}
