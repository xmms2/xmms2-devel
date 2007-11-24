#include "common.h"

static double
udp_timediff (int32_t id, int socket) {
	int i;
	double lag;
	struct timeval time;
	double diff = 0.0;
	int diffc = 0;
	xmmsc_vis_udp_timing_t packet_d;
	char* packet = packet_init_timing (&packet_d);

	gettimeofday (&time, NULL);
	*packet_d.id = htonl (id);
	tv2net (packet_d.clientstamp, &time);
	/* TODO: handle lost packages! */
	for (i = 0; i < 10; ++i) {
		send (socket, packet, packet_d.size, 0);
	}
	printf ("Syncing ");
	do {
		if ((recv (socket, packet, packet_d.size, 0) == packet_d.size) && (*packet_d.type == 'T')) {
			gettimeofday (&time, NULL);
			lag = (tv2ts (&time) - net2ts (packet_d.clientstamp)) / 2.0;
			diffc++;
			diff += net2ts (packet_d.serverstamp) - lag;
			/* debug output
			printf("server diff: %f \t old timestamp: %f, new timestamp %f\n",
			       net2ts (packet_d.serverstamp), net2ts (packet_d.clientstamp), tv2ts (&time));
			 end of debug */
			putchar('.');
		}
	} while (diffc < 10);
	free (packet);

	puts (" done.");
	return diff / (double)diffc;
}

static bool
setup_socket (xmmsc_connection_t *c, xmmsc_vis_udp_t *t, int32_t id, int32_t port) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	char *host;
	char portstr[10];
	char packet[1 + sizeof(int32_t)];
	int32_t* packet_id = (int32_t*)&packet[1];
	sprintf (portstr, "%d", port);

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	host = xmms_ipc_hostname (c->path);
	if (!host) {
		host = strdup ("localhost");
	}

	if (getaddrinfo (host, portstr, &hints, &result) != 0)
	{
		c->error = strdup("Couldn't setup socket!");
		return false;
	}
	free (host);

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		if (!xmms_socket_valid (t->socket[0] = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol))) {
			continue;
		}
		if (connect (t->socket[0], rp->ai_addr, rp->ai_addrlen) != -1) {
			/* Windows doesn't support MSG_DONTWAIT.
			   Why should it? Ripping off BSD, but without mutilating it? No! */
			xmms_socket_set_nonblock (t->socket[0]);
			/* init fallback socket for timing stuff */
			t->socket[1] = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			connect (t->socket[1], rp->ai_addr, rp->ai_addrlen);
			break;
		} else {
			xmms_socket_close (t->socket[0]);
		}
	}
	if (rp == NULL) {
		c->error = strdup("Could not connect!");
		return false;
	}
	freeaddrinfo (result);

	packet[0] = 'H';
	*packet_id = htonl (id);
	send (t->socket[0], &packet, sizeof (packet), 0);

	t->timediff = udp_timediff (id, t->socket[1]);
/*	printf ("diff: %f\n", t->timediff); */
	return true;
}

bool
setup_udp (xmmsc_connection_t *c, xmmsc_vis_udp_t *t, int32_t id)
{
	xmms_ipc_msg_t *msg;
	xmmsc_result_t *res;
	bool ret;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_VISUALIZATION, XMMS_IPC_CMD_VISUALIZATION_INIT_UDP);
	xmms_ipc_msg_put_int32 (msg, id);
	res = xmmsc_send_msg (c, msg);
	xmmsc_result_wait (res);
	if (!xmmsc_result_iserror (res)) {
		int port;
		xmmsc_result_get_int (res, &port);
		ret = setup_socket (c, t, id, port);
	} else {
		ret = false;
	}
	xmmsc_result_unref (res);

	return ret;
}

void
cleanup_udp (xmmsc_vis_udp_t *t)
{
	xmms_socket_close (t->socket[0]);
	xmms_socket_close (t->socket[1]);
}

int
read_start_udp (xmmsc_vis_udp_t *t, unsigned int blocking, xmmsc_vischunk_t **dest, int32_t id)
{
	int ret;
	xmmsc_vis_udp_data_t packet_d;
	char* packet = packet_init_data (&packet_d);

	if (blocking) {
		fd_set rfds;
		struct timeval time;
		FD_ZERO (&rfds);
		FD_SET (t->socket[0], &rfds);
		time.tv_sec = blocking / 1000;
		time.tv_usec = (blocking % 1000) * 1000;
		ret = select (t->socket[0] + 1, &rfds, NULL, NULL, &time);
		if (ret == -1) {
			return -1;
		}
	}
	ret = recv (t->socket[0], packet, packet_d.size, MSG_DONTWAIT);
	if ((ret > 0) && (*packet_d.type == 'V')) {
		*dest = packet_d.data;
		/* resync connection */
		if (ntohs (*packet_d.grace) < 1000) {
			if (t->grace != 0) {
				t->grace = 0;
				/* use second socket here, so vis packets don't get lost */
				t->timediff = udp_timediff (id, t->socket[1]);
			}
		} else {
			t->grace = ntohs (*packet_d.grace);
		}
		/* include the measured time difference */
		double interim = net2ts (packet_d.data->timestamp);
		interim -= t->timediff;
		ts2net (packet_d.data->timestamp, interim);
		return 1;
	} else {
		if (ret == 1 && *packet_d.type == 'K') {
			ret = -1;
		} else if (ret > -1 || xmms_socket_error_recoverable ()) {
			ret = 0;
		} else {
			ret = -1;
		}
		free (packet);
		return ret;
	}
}

void
read_finish_udp (xmmsc_vis_udp_t *t, xmmsc_vischunk_t *dest) {
	char *packet = (char*)dest - XMMS_VISPACKET_UDP_OFFSET;
	free (packet);
}
