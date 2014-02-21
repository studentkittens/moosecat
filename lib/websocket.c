#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <sys/time.h>
#include <libwebsockets.h>

typedef struct session_data_http {
    GHashTable * pollfd_table;
} session_data_http;


int callback_ympd(struct libwebsocket_context *context,
        struct libwebsocket *wsi,
        enum libwebsocket_callback_reasons reason,
        void *user, void *in, size_t len)
{
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            lwsl_info("mpd_client: "
                    "LWS_CALLBACK_ESTABLISHED\n");
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            g_printerr("writable!\n");
            break;
    }

    return 0;
}


static const char http_header[] = 
    "HTTP/1.0 200 OK\x0d\x0a"
    "Server: libwebsockets\x0d\x0a"
    "Content-Type: application/json\x0d\x0a"
    "Content-Length: 000000\x0d\x0a"
    "\x0d\x0a"
    "{\"Hello\": \"World!\"}\x0d\x0a"
    ;



// TODO: This is global since the author of libwebsocket is a braintard.
GHashTable * pollfd_table = NULL; 



typedef struct ConnWatch {
    struct libwebsocket_context * ctx;
    GIOChannel * channel;
    int source_id;
    int events;
    int fd;
} ConnWatch;


static gboolean channel_poll_fd_watch_func(GIOChannel *channel, GIOCondition condition, gpointer data)
{
    struct pollfd pollfd;

    ConnWatch * watch = data;

    pollfd.fd =  watch->fd; 
    pollfd.events = watch->events;
    pollfd.revents = (short) condition;

    g_printerr("---- Serving FD: %d\n", watch->fd);
    libwebsocket_service_fd(watch->ctx, &pollfd);

    return TRUE;
}


static ConnWatch * add_watch_for_fd(struct libwebsocket_context * ctx, int fd, int events)
{
    ConnWatch * watch = g_malloc0(sizeof(ConnWatch));
    watch->fd = fd;
    watch->ctx = ctx;
    watch->events = events;

    /* Add a GIOSource that watches the socket for IO */
    watch->channel = g_io_channel_unix_new(fd);
    watch->source_id  = g_io_add_watch(
        watch->channel,
        events,
        channel_poll_fd_watch_func,
        watch
    );
    return watch;
}

static void del_watch(ConnWatch * watch)
{
    g_source_remove(watch->source_id);
    g_io_channel_unref(watch->channel);
    memset(watch, 0, sizeof(ConnWatch));
    g_free(watch);
}

int callback_http(struct libwebsocket_context *context,
        struct libwebsocket *wsi,
        enum libwebsocket_callback_reasons reason,
        void *user, void *in, size_t len)
{
    int fd = (int)(long)in;

    session_data_http * psd = user;

    g_printerr("REASON %d %p\n", reason, psd);

    switch (reason) {
        case LWS_CALLBACK_CLOSED:
            g_printerr("Closed session.");
            break;
        case LWS_CALLBACK_HTTP:
            g_printerr("INIT OF SESSION\n");
            libwebsocket_write(
                wsi, (char *)http_header,
                sizeof(http_header),
                LWS_WRITE_HTTP
            );
            break;

        case LWS_CALLBACK_LOCK_POLL:
                /*
                 * lock mutex to protect pollfd state
                 * called before any other POLL related callback
                 */
                break;

        case LWS_CALLBACK_UNLOCK_POLL:
                /*
                 * unlock mutex to protect pollfd state when
                 * called after any other POLL related callback
                 */
                break;

        case LWS_CALLBACK_ADD_POLL_FD: {
                    g_printerr("Added FD: %d %d\n", fd, len);
                    g_hash_table_insert(
                        pollfd_table,
                        GINT_TO_POINTER(fd),
                        add_watch_for_fd(context, fd, len)
                    );
                }
                break;

        case LWS_CALLBACK_DEL_POLL_FD: {
                    g_printerr("Delete FD: %d\n", fd);
                    ConnWatch * watch = g_hash_table_lookup(
                        pollfd_table,
                        GINT_TO_POINTER(fd)
                    );
                    g_hash_table_remove(
                        pollfd_table,
                        GINT_TO_POINTER(fd)
                    );
                    del_watch(watch);
                }
                break;

        case LWS_CALLBACK_SET_MODE_POLL_FD: {
                    g_printerr("Set mode fd: %d\n", fd);
                    ConnWatch * watch = g_hash_table_lookup(
                        pollfd_table,
                        GINT_TO_POINTER(fd)
                    );
                    watch->events |= (int)(long)len;
                }
                break;

        case LWS_CALLBACK_CLEAR_MODE_POLL_FD: {
                    g_printerr("clear mode fd: %d\n", fd);
                    ConnWatch * watch = g_hash_table_lookup(
                        pollfd_table,
                        GINT_TO_POINTER(fd)
                    );
                    watch->events &= ~(int)(long)len;
                }
                break;
    }

    return 0;
}


struct libwebsocket_protocols protocols[] = {
    /* first protocol must always be HTTP handler */
    {
        "http-only",                /* name */
        callback_http,                /* callback */
        sizeof (struct session_data_http),        /* per_session_data_size */
        0,                        /* max frame size / rx buffer */
    },
    {
        "ympd-client",
        callback_ympd,
        sizeof(struct session_data_http),
        255,
        0,
    },

    { NULL, NULL, 0, 0, 0 } /* terminator */
};



static gboolean loop_callback(gpointer data)
{
    g_printerr(".");
    libwebsocket_callback_on_writable_all_protocol(&protocols[1]);
    return TRUE;
}



int main(int argc, char const *argv[])
{

    pollfd_table = g_hash_table_new(g_direct_hash, g_direct_equal);

    struct libwebsocket_context *context;
    struct lws_context_creation_info info;
    const char *cert_filepath = NULL;
    const char *private_key_filepath = NULL;
    const char *iface = NULL;

    memset(&info, 0, sizeof(info));
    info.port = 8080;
    //mpd_host = "127.0.0.1";
    //mpd_port = 6600;
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);


    if(cert_filepath != NULL && private_key_filepath == NULL) {
        lwsl_err("private key filepath needed\n");
        return EXIT_FAILURE;
    }

    if(private_key_filepath != NULL && cert_filepath == NULL) {
        lwsl_err("public cert filepath needed\n");
        return EXIT_FAILURE;
    }
    int n, gid = -1, uid = -1;
    unsigned int oldus = 0;

    info.ssl_cert_filepath = cert_filepath;
    info.ssl_private_key_filepath = private_key_filepath;
    info.iface = iface;
    info.protocols = protocols;
    info.extensions = libwebsocket_get_internal_extensions();
    info.gid = gid;
    info.uid = uid;
    info.options = 0;

    context = libwebsocket_create_context(&info);
    if (context == NULL) {
        lwsl_err("libwebsocket init failed\n");
        return EXIT_FAILURE;
    }

#if 0
    while (n >= 0) {
        struct timeval tv;

        gettimeofday(&tv, NULL);

        /*
        * This provokes the LWS_CALLBACK_SERVER_WRITEABLE for every
        * live websocket connection using the DUMB_INCREMENT protocol,
        * as soon as it can take more packets (usually immediately)
        if (((unsigned int)tv.tv_usec - oldus) > 1000 * 500) {
            // mpd_loop();
            libwebsocket_callback_on_writable_all_protocol(&protocols[1]);
            oldus = tv.tv_usec;
        }
        */

        //g_printerr("SERVICE\n");
        n = libwebsocket_service(context, 50);
        //g_printerr("SERVICE DONE %d\n", n);
    }
#endif

    GMainLoop *loop = g_main_loop_new(NULL, TRUE);
    // g_timeout_add(10000, loop_callback, NULL);
    g_main_loop_run(loop);
    g_printerr("MAIN LOOP RUN OVER\n");
    g_main_loop_unref(loop);
    
    libwebsocket_context_destroy(context);
    return 0;
}
