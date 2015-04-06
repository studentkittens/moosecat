#ifndef MOOSE_IDLE_CORE_H
#define MOOSE_IDLE_CORE_H

#include "../moose-mpd-client.h"

G_BEGIN_DECLS

/*
 * Connector using two connections, one for sending, one for events.
 *
 * Sending a command:
 * - send it in the cmnd_con, read the response
 * - wait for incoming events asynchronously on idle_con
 */

/*
 * Type macros.
 */
#define MOOSE_TYPE_IDLE_CLIENT \
    (moose_idle_client_get_type())
#define MOOSE_IDLE_CLIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_IDLE_CLIENT, MooseIdleClient))
#define MOOSE_IS_IDLE_CLIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_IDLE_CLIENT))
#define MOOSE_IDLE_cmd_client_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_IDLE_CLIENT, MooseIdleClientClass))
#define MOOSE_IS_IDLE_cmd_client_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_IDLE_CLIENT))
#define MOOSE_IDLE_cmd_client_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_IDLE_CLIENT, MooseIdleClientClass))

GType moose_idle_client_get_type(void);

struct _MooseCmndClientPrivate;

typedef struct _MooseIdleClient {
    MooseClient parent;
    struct _MooseIdleClientPrivate * priv;
} MooseIdleClient;

typedef struct _MooseIdleClientClass {
    MooseClientClass parent_class;
} MooseIdleClientClass;

G_END_DECLS

#endif /* end of include guard: MOOSE_IDLE_CORE_H */
