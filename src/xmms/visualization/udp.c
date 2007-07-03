/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include "common.h"

gboolean
udpwatcher (GIOChannel *src, GIOCondition cond, xmms_visualization_t *vis)
{
	struct sockaddr_storage from;
	socklen_t sl = sizeof (from);
	xmmsc_vis_udp_data_t buf;

	if (recvfrom (vis->socket, &buf, sizeof (buf), 0, (struct sockaddr *)&from, &sl) > 0) {
		if (buf.type == 'H') {
			xmms_vis_client_t *c;
			xmmsc_vis_udp_timing_t *content = (xmmsc_vis_udp_timing_t*)&buf;
			int32_t id = ntohl (content->id);

			/* debug code starts
			char adrb[INET6_ADDRSTRLEN];
			struct sockaddr_in6 *a = (struct sockaddr_in6 *)&from;
			printf ("Client address: %s:%d, %d\n", inet_ntop (AF_INET6, &a->sin6_addr,
					adrb, INET6_ADDRSTRLEN), a->sin6_port, id);
			 debug code ends */
			g_mutex_lock (vis->clientlock);
			c = get_client (id);
			if (!c || c->type != VIS_UDP) {
				g_mutex_unlock (vis->clientlock);
				return TRUE;
			}
			/* save client address according to id */
			memcpy (&c->transport.udp.addr, &from, sizeof (from));
			c->transport.udp.socket[0] = 1;
			c->transport.udp.grace = 500;
			g_mutex_unlock (vis->clientlock);
		} else if (buf.type == 'T') {
			struct timeval time;
			xmms_vis_client_t *c;
			xmmsc_vis_udp_timing_t *content = (xmmsc_vis_udp_timing_t*)&buf;
			int32_t id = ntohl (content->id);

			g_mutex_lock (vis->clientlock);
			c = get_client (id);
			if (!c || c->type != VIS_UDP) {
				g_mutex_unlock (vis->clientlock);
				return TRUE;
			}
			c->transport.udp.grace = 500;
			g_mutex_unlock (vis->clientlock);

			/* give pong */
			gettimeofday (&time, NULL);
			ts2net (content->serverstamp, tv2ts (&time) - net2ts (content->clientstamp));
			sendto (vis->socket, content, sizeof (xmmsc_vis_udp_timing_t), 0, (struct sockaddr *)&from, sl);

			/* new debug:
			printf ("Timings: local %f, remote %f, diff %f\n", ts2tv (&time), net2tv (&buf[1]), net2tv (&buf[1]) - ts2tv (&time));
			 ends */
		} else {
			xmms_log_error ("Received invalid UDP package!");
		}
	}
	return TRUE;
}

int32_t
init_udp (xmms_visualization_t *vis, int32_t id, xmms_error_t *err)
{
	// TODO: we need the currently used port, not only the default one! */
	int32_t port = XMMS_DEFAULT_TCP_PORT;
	xmms_vis_client_t *c;

	// setup socket if needed
	if (vis->socket == XMMS_INVALID_SOCKET) {
		struct addrinfo hints;
		struct addrinfo *result, *rp;
		int s;

		memset (&hints, 0, sizeof (hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_protocol = 0;

		if ((s = getaddrinfo (NULL, G_STRINGIFY (XMMS_DEFAULT_TCP_PORT), &hints, &result)) != 0)
		{
			xmms_log_error ("Could not setup socket! getaddrinfo: %s", gai_strerror (s));
			xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "Could not setup socket!");
			return -1;
		}

		for (rp = result; rp != NULL; rp = rp->ai_next) {
			vis->socket = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (vis->socket == XMMS_INVALID_SOCKET) {
				continue;
			}
			if (bind (vis->socket, rp->ai_addr, rp->ai_addrlen) != -1) {
				break;
			} else {
				close (vis->socket);
			}
		}
		if (rp == NULL) {
			xmms_log_error ("Could not bind socket!");
			xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "Could not bind socket!");
			freeaddrinfo (result);
			return -1;
		}
		freeaddrinfo (result);

		/* register into mainloop: */
/* perhaps needed, perhaps not .. #ifdef __WIN32__
		vis->socketio = g_io_channel_win32_new_socket (vis->socket);
#else */
		vis->socketio = g_io_channel_unix_new (vis->socket);
/*#endif */
		g_io_channel_set_encoding (vis->socketio, NULL, NULL);
		g_io_channel_set_buffered (vis->socketio, FALSE);
		g_io_add_watch (vis->socketio, G_IO_IN, (GIOFunc) udpwatcher, vis);
	}

	/* set up client structure */
	x_fetch_client (id);
	c->type = VIS_UDP;
	memset (&c->transport.udp.addr, 0, sizeof (c->transport.udp.addr));
	c->transport.udp.socket[0] = 0;
	x_release_client ();

	xmms_log_info ("Visualization client %d initialised using UDP", id);
	// return socketport
	return port;
}

void
cleanup_udp (xmmsc_vis_udp_t *t)
{
	// TODO: issue killer packet
}

gboolean
write_start_udp (int32_t id, xmmsc_vis_udp_t *t, xmmsc_vischunk_t **dest)
{
	/* first check if the client is still there */
	if (t->grace == 0) {
		delete_client (id);
		return FALSE;
	}
	if (t->socket == 0) {
		return FALSE;
	}
	xmmsc_vis_udp_data_t *packet = g_new (xmmsc_vis_udp_data_t, 1);
	packet->type = 'V';
	packet->grace = --t->grace;
	*dest = &packet->data;
	return TRUE;
}

void
write_finish_udp (int32_t id, xmmsc_vis_udp_t *t, xmmsc_vischunk_t *dest, int socket)
{
	socklen_t sl = sizeof (t->addr);
	/* debug code starts
	char adrb[INET6_ADDRSTRLEN];
	struct sockaddr_in6 *a = (struct sockaddr_in6 *)&c->transport.udp.addr;
	printf ("Destination: %s:%d, %d\n", inet_ntop (AF_INET6, &a->sin6_addr,
			adrb, INET6_ADDRSTRLEN), a->sin6_port, id);
	 debug code ends */
	/* nasty trick that won't get through code review */
	int offset[2];
	xmmsc_vis_udp_data_t *packet = 0;
	offset[0] = ((int)&packet->data - (int)packet);
	packet = (xmmsc_vis_udp_data_t*)((char*)dest - offset[0]);
	offset[1] = ((int)&packet->data.data - (int)&packet->data);
	sendto (socket, packet, offset[0] + offset[1] + ntohs (dest->size) * sizeof(int16_t), 0, (struct sockaddr *)&t->addr, sl);
	g_free (packet);
}
