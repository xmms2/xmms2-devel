#include <glib.h>

#include <stdio.h>

struct xmmsc_ipc_glib_St {
	GSource *source;
	GPollFD *pollfd;
};

typedef struct xmmsc_ipc_glib_St xmmsc_ipc_glib_t;

gboolean
xmmsc_ipc_setup_with_gmain (xmmsc_ipc_t *ipc)
{
	xmmsc_ipc_glib_t *gipc;
	gipc = g_new0 (xmms_ipc_glib_t, 1);

	gipc->pollfd = g_new0 (GPollFD, 1);
	gipc->pollfd->fd = xmmsc_ipc_fd_get (ipc);
	gipc->pollfd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;

	gipc->source = g_source_new (&xmms_ipc_server_funcs, sizeof (GSource));
	gipc->source = source;

	g_source_set_callback (gipc->source, 
			       (GSourceFunc)xmmsc_ipc_glib_callback,
			       (gpointer) gipc, NULL);

	g_source_add_poll gipc->(source, gipc->pollfd);
	g_source_attach (source, NULL);

	return TRUE;
}

