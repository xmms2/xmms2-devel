
#include <glib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

#include <xmms/ipc_transport.h>
#include <xmms/util.h>
#include <xmms/ipc_msg.h>

void xmms_log_debug (const gchar *fmt, ...)
{
	char buff[1024];
	va_list ap;

	va_start (ap, fmt);
#ifdef HAVE_VSNPRINTF
	vsnprintf (buff, 1024, fmt, ap);
#else
	vsprintf (buff, fmt, ap);
#endif
	va_end (ap);

	printf ("%s\n", buff);
}

int main (int argc, char **argv)
{
	xmms_ipc_transport_t *transport;
	xmms_ipc_msg_t *msg;
	gint fd;
	gint i;

	if (argc < 1)
		return 0;

	printf ("%s\n", argv[1]);

	transport = xmms_ipc_client_init (argv[1]);
	if (!transport) {
		printf  ("Korv!\n");
		exit (-1);
	}

	fd = xmms_ipc_transport_fd_get (transport);

	msg = xmms_ipc_msg_string_new (42, "korv!");

	i = 5;

	while (i) {
		if (!xmms_ipc_msg_write_fd (fd, msg))
			printf ("Korv?!");
		i--;
	}
	sleep (1);
}
