#ifndef MOOSE_CMD_CORE_H
#define MOOSE_CMD_CORE_H

#include "../moose-mpd-client.h"

G_BEGIN_DECLS

/**
 * Connector using two connections, one for sending, one for events.
 *
 * Sending a command:
 * - send it in the cmnd_con, read the response
 * - wait for incoming events asynchronously on idle_con
 */

/*
 * Type macros.
 */
#define MOOSE_TYPE_CMD_CLIENT \
    (moose_cmd_client_get_type())
#define MOOSE_CMD_CLIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_CMD_CLIENT, MooseCmdClient))
#define MOOSE_IS_CMD_CLIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_CMD_CLIENT))
#define MOOSE_CMD_cmd_client_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_CMD_CLIENT, MooseCmdClientClass))
#define MOOSE_IS_CMD_cmd_client_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_CMD_CLIENT))
#define MOOSE_CMD_cmd_client_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_CMD_CLIENT, MooseCmdClientClass))

GType moose_cmd_client_get_type(void);

struct _MooseCmndClientPrivate;

typedef struct _MooseCmdClient {
    MooseClient parent;
    struct _MooseCmdClientPrivate * priv;
} MooseCmdClient;

typedef struct _MooseCmdClientClass {
    MooseClientClass parent_class;
} MooseCmdClientClass;

G_END_DECLS

#endif /* end of include guard: MOOSE_CMD_CORE_H */
