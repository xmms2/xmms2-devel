/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#include <stdlib.h>
#include <unistd.h>
#include "common.h"

static gboolean
udpwatcher (GIOChannel *src, GIOCondition cond, xmms_vis_server_t *s)
{
	struct sockaddr_storage from;
	socklen_t sl = sizeof (from);
	xmmsc_vis_udp_timing_t packet_d;
	char* packet = packet_init_timing (&packet_d);
	if ((recvfrom (s->socket, packet, packet_d.size, 0, (struct sockaddr *)&from, &sl)) > 0) {
		if (*packet_d.__unaligned_type == 'H') {
			xmms_vis_client_t *c;
			int32_t id;

			XMMSC_VIS_UNALIGNED_READ (id, packet_d.__unaligned_id, int32_t);
			id = ntohl (id);

			g_mutex_lock (&s->vis->clientlock);
			c = get_client (id);
			if (!c || c->type != VIS_UDP) {
				g_mutex_unlock (&s->vis->clientlock);
				return TRUE;
			}
			/* save client address according to id */
			memcpy (&c->transport.udp.addr, &from, sizeof (from));
			c->server = s;
			c->transport.udp.socket[0] = 1;
			c->transport.udp.grace = 2000;
			g_mutex_unlock (&s->vis->clientlock);
		} else if (*packet_d.__unaligned_type == 'T') {
			struct timeval time;
			xmms_vis_client_t *c;
			int32_t id;

			XMMSC_VIS_UNALIGNED_READ (id, packet_d.__unaligned_id, int32_t);
			id = ntohl (id);

			g_mutex_lock (&s->vis->clientlock);
			c = get_client (id);
			if (!c || c->type != VIS_UDP) {
				g_mutex_unlock (&s->vis->clientlock);
				free (packet);
				return TRUE;
			}
			c->transport.udp.grace = 2000;
			g_mutex_unlock (&s->vis->clientlock);

			/* give pong */
			gettimeofday (&time, NULL);

			struct timeval cts, sts;

			XMMSC_VIS_UNALIGNED_READ (cts.tv_sec, &packet_d.__unaligned_clientstamp[0], int32_t);
			XMMSC_VIS_UNALIGNED_READ (cts.tv_usec, &packet_d.__unaligned_clientstamp[1], int32_t);
			cts.tv_sec = ntohl (cts.tv_sec);
			cts.tv_usec = ntohl (cts.tv_usec);

			sts.tv_sec = time.tv_sec - cts.tv_sec;
			sts.tv_usec = time.tv_usec - cts.tv_usec;
			if (sts.tv_usec < 0) {
				sts.tv_sec--;
				sts.tv_usec += 1000000;
			}

			XMMSC_VIS_UNALIGNED_WRITE (&packet_d.__unaligned_serverstamp[0],
			                           (int32_t)htonl (sts.tv_sec), int32_t);
			XMMSC_VIS_UNALIGNED_WRITE (&packet_d.__unaligned_serverstamp[1],
			                           (int32_t)htonl (sts.tv_usec), int32_t);

			sendto (s->socket, packet, packet_d.size, 0, (struct sockaddr *)&from, sl);
		} else {
			xmms_log_error ("Received invalid UDP package!");
		}
	}
	free (packet);
	return TRUE;
}

int32_t
init_udp (xmms_visualization_t *vis, int32_t id, xmms_error_t *err)
{
	// TODO: we need the currently used port, not only the default one! */
	int32_t port = XMMS_DEFAULT_UDP_PORT;
	xmms_vis_client_t *c;

	// setup socket if needed
	// TODO: isn't there a race of multiple clients tying to bind()?
	if (vis->serverv == NULL) {
		struct addrinfo hints;
		struct addrinfo *result, *rp;
		int status;

		int32_t possible_servers = 0;
		int32_t opened_servers = 0;
		int32_t i;
		xmms_vis_server_t *servers = NULL;

		memset (&hints, 0, sizeof (hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_protocol = 0;

		if ((status = getaddrinfo (NULL, G_STRINGIFY (XMMS_DEFAULT_UDP_PORT), &hints, &result)) != 0)
		{
			xmms_log_error ("Could not setup socket! getaddrinfo: %s", gai_strerror (status));
			xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "Could not setup socket!");
			return -1;
		}

		for (rp = result; rp != NULL; rp = rp->ai_next) {
			possible_servers++;
		}
		servers = g_new0 (xmms_vis_server_t, possible_servers);
		if (servers == NULL) {
			xmms_log_error ("Could not allocate memory!");
			xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "Could not allocate memory!");
			freeaddrinfo (result);
			return -1;
		}

		for (rp = result; rp != NULL; rp = rp->ai_next) {
			int sock;
			xmms_vis_server_t *s = &servers[opened_servers];

			sock = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (!xmms_socket_valid (sock)) {
				continue;
			}
			if (bind (sock, rp->ai_addr, rp->ai_addrlen) == -1) {
				/* In case we already bound v4 socket
				 * and v6 are attempting to set up
				 * dual-stack v4+v6. Try again v6-only
				 * mode. */
				if (rp->ai_family == AF_INET6 && errno == EADDRINUSE) {
					int v6only = 1;
					if (setsockopt (sock, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof (v6only)) == -1) {
						close (socket);
						continue;
					}
					if (bind (sock, rp->ai_addr, rp->ai_addrlen) == -1) {
						close (socket);
						continue;
					}
				} else {
					close (socket);
					continue;
				}
			}
			opened_servers++;
			s->socket = sock;
		}
		if (opened_servers == 0) {
			xmms_log_error ("Could not bind any vis sockets!");
			xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "Could not bind any vis sockets!");
			freeaddrinfo (result);
			return -1;
		}
		freeaddrinfo (result);

		for (i = 0; i < opened_servers; i++) {
			xmms_vis_server_t *s = &servers[i];
			/* register into mainloop: */
			// TODO: on windows
			// vis->socketio = g_io_channel_win32_new_socket (vis->socket);
			s->socketio = g_io_channel_unix_new (s->socket);
			s->vis = vis;
			g_io_channel_set_encoding (s->socketio, NULL, NULL);
			g_io_channel_set_buffered (s->socketio, FALSE);
			g_io_add_watch (s->socketio, G_IO_IN, (GIOFunc) udpwatcher, s);
		}
		vis->serverc = opened_servers;
		vis->serverv = servers;
	}

	/* set up client structure */
	x_fetch_client (id);
	c->type = VIS_UDP;
	memset (&c->transport.udp.addr, 0, sizeof (c->transport.udp.addr));
	c->transport.udp.socket[0] = 0;
	x_release_client ();

	xmms_log_info ("Visualization client %d initialised using UDP", id);
	return port;
}

void
cleanup_udp (xmmsc_vis_udp_t *t, xmms_socket_t socket)
{
	socklen_t sl = sizeof (t->addr);
	char packet = 'K';
	sendto (socket, &packet, 1, 0, (struct sockaddr *)&t->addr, sl);
}

gboolean
write_udp (xmmsc_vis_udp_t *t, xmms_vis_client_t *c, int32_t id, struct timeval *time, int channels, int size, short *buf, int socket)
{
	xmmsc_vis_udp_data_t packet_d;
	xmmsc_vischunk_t *__unaligned_dest;
	short res;
	int offset;
	char* packet;

	/* first check if the client is still there */
	if (t->grace == 0) {
		delete_client (id);
		return FALSE;
	}
	if (t->socket == 0) {
		return FALSE;
	}

	packet = packet_init_data (&packet_d);
	t->grace--;
	XMMSC_VIS_UNALIGNED_WRITE (packet_d.__unaligned_grace, htons (t->grace), uint16_t);
	__unaligned_dest = packet_d.__unaligned_data;

	XMMSC_VIS_UNALIGNED_WRITE (&__unaligned_dest->timestamp[0],
	                           (int32_t)htonl (time->tv_sec), int32_t);
	XMMSC_VIS_UNALIGNED_WRITE (&__unaligned_dest->timestamp[1],
	                           (int32_t)htonl (time->tv_usec), int32_t);


	XMMSC_VIS_UNALIGNED_WRITE (&__unaligned_dest->format, (uint16_t)htons (c->format), uint16_t);
	res = fill_buffer (__unaligned_dest->data, &c->prop, channels, size, buf);
	XMMSC_VIS_UNALIGNED_WRITE (&__unaligned_dest->size, (uint16_t)htons (res), uint16_t);

	offset = ((char*)&__unaligned_dest->data - (char*)__unaligned_dest);

	sendto (socket, packet, XMMS_VISPACKET_UDP_OFFSET + offset + res * sizeof (int16_t), 0, (struct sockaddr *)&t->addr, sizeof (t->addr));
	free (packet);


	return TRUE;
}
