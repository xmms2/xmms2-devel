#include <glib.h>

xmms_ipc_t *
xmms_ipc_init ()
{
	xmms_ipc_t *ipc;

	ipc = g_new0 (xmms_ipc_t, 1);

	return ipc;
}

