
#include <glib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

#include "ipc_transport.h"

int main ()
{
	xmms_ipc_transport_t *transport;
	gint fd;

	transport = xmms_ipc_server_init ("unix:///tmp/xmms2.socket");
	if (!transport) {
		printf  ("Korv!\n");
		exit (-1);
	}

	fd = xmms_ipc_transport_fd_get (transport);

	while (1) {
		fd_set set;

		FD_ZERO (&set);
		FD_SET (fd, &set);

		printf ("Select... %d\n", fd);
		if (select (fd+1, &set, NULL, NULL, NULL) > 0) {
			gchar buffer[5];

			xmms_ipc_transport_t *client = xmms_ipc_server_accept (transport);
			if (client == NULL)
				return 0;

			printf ("Client connected...\n");
			xmms_ipc_transport_read (client, buffer, 4);
			printf ("%s\n", buffer);
			return 0;
		} else {
			return 0;
		}
	}

}
