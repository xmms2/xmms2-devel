#ifndef __XMMS_IPC_H__
#define __XMMS_IPC_H__

#include "xmms/object.h"
#include "xmms/ipc_msg.h"

typedef enum {
	XMMS_IPC_CLIENT_STATUS_NEW,
} xmms_ipc_client_status_t;


typedef struct xmms_ipc_St xmms_ipc_t;
typedef struct xmms_ipc_client_St xmms_ipc_client_t;

void xmms_ipc_object_register (xmms_ipc_objects_t objectid, xmms_object_t *object);
void xmms_ipc_object_unregister (xmms_ipc_objects_t objectid);
xmms_ipc_t * xmms_ipc_init (void);
gboolean xmms_ipc_setup_server (gchar *path);
gboolean xmms_ipc_setup_with_gmain (xmms_ipc_t *ipc);
gboolean xmms_ipc_client_msg_write (xmms_ipc_client_t *client, xmms_ipc_msg_t *msg);

#endif

