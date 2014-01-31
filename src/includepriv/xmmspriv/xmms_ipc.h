#ifndef __XMMS_PRIV_IPC_H__
#define __XMMS_PRIV_IPC_H__

#include <xmms/xmms_ipc.h>
#include <xmmsc/xmmsc_ipc_msg.h>

typedef struct xmms_ipc_St xmms_ipc_t;

xmms_ipc_t *xmms_ipc_init (void);
void xmms_ipc_shutdown (void);
void on_config_ipcsocket_change (xmms_object_t *object, xmmsv_t *data, gpointer udata);
gboolean xmms_ipc_setup_server (const gchar *path);

typedef struct xmms_ipc_manager_St xmms_ipc_manager_t;
xmms_ipc_manager_t *xmms_ipc_manager_get (void);

gboolean xmms_ipc_has_pending (guint signalid);
void xmms_ipc_send_message (gint cli, xmms_ipc_msg_t *msg, xmms_error_t *err);
void xmms_ipc_send_broadcast (guint broadcastid, gint cli, xmmsv_t *arg, xmms_error_t *err);
GList *xmms_ipc_get_connected_clients (void);

#endif
