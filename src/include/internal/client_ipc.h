#ifndef __XMMSC_IPC_H__
#define __XMMSC_IPC_H__

#include "xmms/ipc_msg.h"
#include "xmms/xmmsclient.h"

typedef struct xmmsc_ipc_St xmmsc_ipc_t;

typedef void (*xmmsc_ipc_wakeup_t) (xmmsc_ipc_t *);

xmmsc_ipc_t *xmmsc_ipc_init (void);
void xmmsc_ipc_wakeup_set (xmmsc_ipc_t *ipc, xmmsc_ipc_wakeup_t wakeupfunc);
gboolean xmmsc_ipc_io_in_callback (xmmsc_ipc_t *ipc);
gboolean xmmsc_ipc_io_out_callback (xmmsc_ipc_t *ipc);
gboolean xmmsc_ipc_msg_write (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg, guint32 cid);
void xmmsc_ipc_disconnect (xmmsc_ipc_t *ipc);
gpointer xmmsc_ipc_private_data_get (xmmsc_ipc_t *ipc);
void xmmsc_ipc_private_data_set (xmmsc_ipc_t *ipc, gpointer data);
void xmmsc_ipc_destroy (xmmsc_ipc_t *ipc);
gboolean xmmsc_ipc_connect (xmmsc_ipc_t *ipc, gchar *path);
void xmmsc_ipc_error_set (xmmsc_ipc_t *ipc, gchar *error);
gint xmmsc_ipc_fd_get (xmmsc_ipc_t *ipc);
gboolean xmmsc_ipc_want_io_out (xmmsc_ipc_t *ipc);
gboolean xmmsc_ipc_flush (xmmsc_ipc_t *ipc);

void xmmsc_ipc_result_register (xmmsc_ipc_t *ipc, xmmsc_result_t *res);
xmmsc_result_t *xmmsc_ipc_result_lookup (xmmsc_ipc_t *ipc, guint cid);
void xmmsc_ipc_result_unregister (xmmsc_ipc_t *ipc, xmmsc_result_t *res);
void xmmsc_ipc_wait_for_event (xmmsc_ipc_t *ipc, guint timeout);

enum {
	XMMSC_IPC_IO_IN,
	XMMSC_IPC_IO_OUT,
};

#endif

