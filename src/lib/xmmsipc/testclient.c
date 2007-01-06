
#include <glib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

#include "ipc_transport.h"

int main ()
{

	xmms_ipc_transport_t *transport;
	gint fd;

	transport = xmms_ipc_client_init ("unix:///tmp/xmms2.socket");
	if (!transport) {
		printf  ("Korv!\n");
		exit (-1);
	}

	fd = xmms_ipc_transport_fd_get (transport);

	xmms_ipc_transport_write (transport, "korv", 4);
}
