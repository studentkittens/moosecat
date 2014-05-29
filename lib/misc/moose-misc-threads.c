#include "moose-misc-threads.h"
#include "../moose-config.h"

enum {
    SIGNAL_THREAD,
    SIGNAL_DELIVER,
    N_SIGNALS
};

enum {
    PROP_MAX_THREADS = 1,
    N_PROPS
};

static guint SIGNALS[N_SIGNALS];

typedef struct _MooseThreadsPrivate {
    /* Thread Pool to distribute the jobs */
    GThreadPool * pool;

    /* Queue to pass back the results to the mainloop */
    GAsyncQueue * queue;

    /* max threads to use. */
    int max_threads;
} MooseThreadsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
    MooseThreads, moose_threads, G_TYPE_OBJECT
);

static void moose_threads_dispatch(gpointer data, gpointer user_data) {
    g_assert(user_data);

    MooseThreads * self = user_data;

    /* Probably some python object, just pass it */
    gpointer result = NULL;
    g_signal_emit(self, SIGNALS[SIGNAL_THREAD], 0, data, &result);

    moose_threads_forward(self, result);
}

static gboolean moose_threads_mainloop_callback(gpointer user_data) {
    g_assert(user_data);

    gpointer result = NULL;
    MooseThreads * self = user_data;

    /* It's possible that there happended more events */
    while ((result = g_async_queue_try_pop(self->priv->queue)) != NULL) {
        g_signal_emit(self, SIGNALS[SIGNAL_DELIVER], 0, result);
    }

    return FALSE;
}

MooseThreads * moose_threads_new(int max_threads) {
    return g_object_new(MOOSE_TYPE_THREADS, "max-threads", max_threads, NULL);
}

void moose_threads_push(MooseThreads * self, void * data) {
    g_assert(self);

    /* Check if not destroyed yet */
    if (self->priv->pool != NULL) {
        g_thread_pool_push(self->priv->pool, data, NULL);
    }

}

void moose_threads_forward(MooseThreads * self, void * result) {
    if (result != NULL) {
        g_async_queue_push(self->priv->queue, result);
        g_idle_add(moose_threads_mainloop_callback, self);
    }

}

void moose_threads_unref(MooseThreads * self) {
    if(self != NULL) {
        g_object_unref(self);
    }
}

static void moose_threads_finalize(GObject * gobject) {
    MooseThreads * self = MOOSE_THREADS(gobject);
    g_assert(self);

    /* Stop the ThreadPool */
    g_thread_pool_free(self->priv->pool, TRUE, FALSE);
    self->priv->pool = NULL;

    /* Get rid of the Queue */
    g_async_queue_unref(self->priv->queue);

    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)))->finalize(gobject);
}

static void moose_threads_get_property(
    GObject * object,
    guint property_id,
    GValue * value,
    GParamSpec * pspec) {
    MooseThreads *self = MOOSE_THREADS(object);

    switch (property_id) {
    case PROP_MAX_THREADS:
         g_value_set_int(value, self->priv->max_threads);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_threads_set_property(
    GObject * object,
    guint property_id,
    const GValue * value,
    GParamSpec * pspec) {
    MooseThreads * self = MOOSE_THREADS(object);

    switch (property_id) {
    case PROP_MAX_THREADS:
        self->priv->max_threads = g_value_get_int(value);
        g_thread_pool_set_max_threads(self->priv->pool, self->priv->max_threads, NULL);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void moose_threads_class_init(MooseThreadsClass * klass) {
    GObjectClass * gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = moose_threads_finalize;
    gobject_class->get_property = moose_threads_get_property;
    gobject_class->set_property = moose_threads_set_property;
    
    /**
     * MooseThreads:max-threads: (type int)
     *
     * Maximum number of threads to be launched.
     */
    g_object_class_install_property(gobject_class, PROP_MAX_THREADS, 
        g_param_spec_int(
                "max-threads",
                "Max. Threads",
                "Maximum number of threads to launch",
                0, G_MAXINT, 10, G_PARAM_READWRITE | G_PARAM_CONSTRUCT
            )
    );

    SIGNALS[SIGNAL_THREAD] = g_signal_new("thread",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_LAST,
            0, NULL, NULL, NULL,
            G_TYPE_POINTER /* return_type */,
            1 /* n_params */,
            G_TYPE_POINTER /* param_types */
    );

    SIGNALS[SIGNAL_DELIVER] = g_signal_new("deliver",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_LAST,
            0, NULL, NULL, NULL,
            G_TYPE_NONE /* return_type */,
            1 /* n_params */,
            G_TYPE_POINTER /* param_types */
    );
}

static void moose_threads_init(MooseThreads * self) {
    self->priv = moose_threads_get_instance_private(self);

    /* Allocated data */
    GError * error = NULL;
    self->priv->queue = g_async_queue_new();
    self->priv->pool = g_thread_pool_new(
                     moose_threads_dispatch,
                     self,
                     10,
                     FALSE,
                     &error
                 );

    /* Be nice and check for the error */
    if (error != NULL) {
        moose_warning("Failed to instance GThreadPool: %s\n", error->message);
        g_error_free(error);
    }
}
