#include <Ecore.h>

#include <stdio.h>

#include "internal/client_ipc.h"
#include "xmms/xmmsclient.h"
#include "internal/xmmsclient_int.h"

static int
on_fd_data (void *udata, Ecore_Fd_Handler *handler)
{
	xmmsc_ipc_t *ipc = udata;
	int ret = 0;

	if (ecore_main_fd_handler_active_get (handler, ECORE_FD_ERROR)) {
		xmmsc_ipc_error_set (ipc, "Remote host disconnected, or something!");
		xmmsc_ipc_disconnect (ipc);
	} else if (ecore_main_fd_handler_active_get (handler, ECORE_FD_READ)) {
		ret = xmmsc_ipc_io_in_callback (ipc);
	}

	return ret;
}

gboolean
xmmsc_ipc_setup_with_ecore (xmmsc_connection_t *c)
{
	ecore_main_fd_handler_add (xmmsc_ipc_fd_get (c->ipc),
	                           ECORE_FD_READ | ECORE_FD_ERROR,
	                           on_fd_data, c->ipc, NULL, NULL);

	return TRUE;
}

