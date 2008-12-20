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
	XMMSC_VIS_UNALIGNED_WRITE (packet_d.__unaligned_id, (int32_t)htonl (id), int32_t);
	XMMSC_VIS_UNALIGNED_WRITE (&packet_d.__unaligned_clientstamp[0],
	                           (int32_t)htonl (time.tv_sec), int32_t);
	XMMSC_VIS_UNALIGNED_WRITE (&packet_d.__unaligned_clientstamp[1],
	                           (int32_t)htonl (time.tv_usec), int32_t);

	/* TODO: handle lost packages! */
	for (i = 0; i < 10; ++i) {
		send (socket, packet, packet_d.size, 0);
	}
	printf ("Syncing ");
	do {
		if ((recv (socket, packet, packet_d.size, 0) == packet_d.size) && (*packet_d.__unaligned_type == 'T')) {
			struct timeval rtv;
			gettimeofday (&time, NULL);
			XMMSC_VIS_UNALIGNED_READ (rtv.tv_sec, &packet_d.__unaligned_clientstamp[0], int32_t);
			XMMSC_VIS_UNALIGNED_READ (rtv.tv_usec, &packet_d.__unaligned_clientstamp[1], int32_t);
			rtv.tv_sec = ntohl (rtv.tv_sec);
			rtv.tv_usec = ntohl (rtv.tv_usec);

			lag = (tv2ts (&time) - tv2ts (&rtv)) / 2.0;
			diffc++;

			XMMSC_VIS_UNALIGNED_READ (rtv.tv_sec, &packet_d.__unaligned_serverstamp[0], int32_t);
			XMMSC_VIS_UNALIGNED_READ (rtv.tv_usec, &packet_d.__unaligned_serverstamp[1], int32_t);
			rtv.tv_sec = ntohl (rtv.tv_sec);
			rtv.tv_usec = ntohl (rtv.tv_usec);

			diff += tv2ts (&rtv) - lag;
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

	if (xmms_getaddrinfo (host, portstr, &hints, &result) != 0)
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
	xmms_freeaddrinfo (result);

	packet[0] = 'H';
	*packet_id = htonl (id);
	send (t->socket[0], &packet, sizeof (packet), 0);

	t->timediff = udp_timediff (id, t->socket[1]);
/*	printf ("diff: %f\n", t->timediff); */
	return true;
}

xmmsc_result_t *
setup_udp_prepare (xmmsc_connection_t *c, int32_t vv)
{
	xmms_ipc_msg_t *msg;
	xmmsc_result_t *res;
	xmmsc_visualization_t *v;

	x_check_conn (c, 0);
	v = get_dataset (c, vv);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_VISUALIZATION, XMMS_IPC_CMD_VISUALIZATION_INIT_UDP);
	xmms_ipc_msg_put_int32 (msg, v->id);
	res = xmmsc_send_msg (c, msg);
	if (res) {
		xmmsc_result_visc_set (res, v);
	}
	return res;
}

bool
setup_udp_handle (xmmsc_result_t *res)
{
	bool ret;
	xmmsc_vis_udp_t *t;
	xmmsc_visualization_t *visc;

	visc = xmmsc_result_visc_get (res);
	if (!visc) {
		x_api_error_if (1, "non vis result?", -1);
	}

	t = &visc->transport.udp;

	if (!xmmsc_result_iserror (res)) {
		xmmsv_t *val;
		int port;
		val = xmmsc_result_get_value (res);
		xmmsv_get_int (val, &port);
		ret = setup_socket (xmmsc_result_get_connection (res), t, visc->id, port);
	} else {
		ret = false;
	}

	return ret;
}

void
cleanup_udp (xmmsc_vis_udp_t *t)
{
	xmms_socket_close (t->socket[0]);
	xmms_socket_close (t->socket[1]);
}


int
wait_for_socket (xmmsc_vis_udp_t *t, unsigned int blocking)
{
	int ret;
	fd_set rfds;
	struct timeval time;
	FD_ZERO (&rfds);
	FD_SET (t->socket[0], &rfds);
	time.tv_sec = blocking / 1000;
	time.tv_usec = (blocking % 1000) * 1000;
	ret = select (t->socket[0] + 1, &rfds, NULL, NULL, &time);
	return ret;
}

int
read_do_udp (xmmsc_vis_udp_t *t, xmmsc_visualization_t *v, short *buffer, int drawtime, unsigned int blocking)
{
	int old;
	int ret;
	int i, size;
	xmmsc_vis_udp_data_t packet_d;
	char* packet = packet_init_data (&packet_d);
	xmmsc_vischunk_t data;

	if (blocking) {
		wait_for_socket (t, blocking);
	}

	ret = recv (t->socket[0], packet, packet_d.size, 0);
	if ((ret > 0) && (*packet_d.__unaligned_type == 'V')) {
		uint16_t grace;
		struct timeval rtv;

		XMMSC_VIS_UNALIGNED_READ (data, packet_d.__unaligned_data, xmmsc_vischunk_t);

		/* resync connection */
		XMMSC_VIS_UNALIGNED_READ (grace, packet_d.__unaligned_grace, uint16_t);
		grace = ntohs (grace);
		if (grace < 1000) {
			if (t->grace != 0) {
				t->grace = 0;
				/* use second socket here, so vis packets don't get lost */
				t->timediff = udp_timediff (v->id, t->socket[1]);
			}
		} else {
			t->grace = grace;
		}
		/* include the measured time difference */

		rtv.tv_sec = ntohl (data.timestamp[0]);
		rtv.tv_usec = ntohl (data.timestamp[1]);

		double interim = tv2ts (&rtv);
		interim -= t->timediff;
		ts2net (data.timestamp, interim);
		ret = 1;
	} else {
		if (ret == 1 && *packet_d.__unaligned_type == 'K') {
			ret = -1;
		} else if (ret > -1 || xmms_socket_error_recoverable ()) {
			ret = 0;
		} else {
			ret = -1;
		}
		free (packet);
		return ret;
	}

	old = check_drawtime (net2ts (data.timestamp), drawtime);

	if (!old) {
		size = ntohs (data.size);
		for (i = 0; i < size; ++i) {
			buffer[i] = (int16_t)ntohs (data.data[i]);
		}
	}

	free (packet);

	if (!old) {
		return size;
	}
	return 0;
}
