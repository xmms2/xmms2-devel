#ifndef __XMMS_PRIV_IPC_H__
#define __XMMS_PRIV_IPC_H__

#include "xmms/xmms_ipc.h"

typedef struct xmms_ipc_St xmms_ipc_t;

xmms_ipc_t *xmms_ipc_init (void);
void xmms_ipc_shutdown (void);
void on_config_ipcsocket_change (xmms_object_t *object, gconstpointer data, gpointer udata);
gboolean xmms_ipc_setup_server (const gchar *path);
gboolean xmms_ipc_setup_with_gmain (xmms_ipc_t *ipc);

gboolean xmms_ipc_has_pending (guint signalid);

#endif
