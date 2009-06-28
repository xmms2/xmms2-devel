/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#ifndef __VISUALIZATION_COMMON_H__
#define __VISUALIZATION_COMMON_H__

#include <glib.h>

#include "xmmspriv/xmms_log.h"
#include "xmmspriv/xmms_visualization.h"
#include "xmmsc/xmmsc_visualization.h"

/**
 * The structures for a vis client
 */

typedef struct {
	union {
		xmmsc_vis_unixshm_t shm;
		xmmsc_vis_udp_t udp;
	} transport;
	xmmsc_vis_transport_t type;
	unsigned short format;
	xmmsc_vis_properties_t prop;
} xmms_vis_client_t;

/* provided by object.c */
xmms_vis_client_t *get_client (int32_t id);
void delete_client (int32_t id);
void send_data (int channels, int size, int16_t *buf);

/* provided by unixshm.c / dummy.c */
int32_t init_shm (xmms_visualization_t *vis, int32_t id, int32_t shmid, xmms_error_t *err);
void cleanup_shm (xmmsc_vis_unixshm_t *t);
gboolean write_start_shm (int32_t id, xmmsc_vis_unixshm_t *t, xmmsc_vischunk_t **dest);
void write_finish_shm (int32_t id, xmmsc_vis_unixshm_t *t, xmmsc_vischunk_t *dest);

gboolean write_shm (xmmsc_vis_unixshm_t *t, xmms_vis_client_t *c, int32_t id, struct timeval *time, int channels, int size, short *buf);

/* provided by udp.c */
int32_t init_udp (xmms_visualization_t *vis, int32_t id, xmms_error_t *err);
void cleanup_udp (xmmsc_vis_udp_t *t, xmms_socket_t socket);
gboolean write_udp (xmmsc_vis_udp_t *t, xmms_vis_client_t *c, int32_t id, struct timeval *time, int channels, int size, short *buf, int socket);

/* provided by format.c */
void fft_init (void);
short fill_buffer (int16_t *dest, xmmsc_vis_properties_t* prop, int channels, int size, short *src);

/* never call a fetch without a guaranteed release following! */
#define x_fetch_client(id) \
	g_mutex_lock (vis->clientlock); \
	c = get_client (id); \
	if (!c) { \
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid server-side identifier provided"); \
		g_mutex_unlock (vis->clientlock); \
		return -1; \
	}
#define x_release_client() \
	g_mutex_unlock (vis->clientlock);

/**
 * The structures for the vis module
 */

struct xmms_visualization_St {
	xmms_object_t object;
	xmms_output_t *output;
	xmms_socket_t socket;
	GIOChannel *socketio;

	GMutex *clientlock;
	int32_t clientc;
	xmms_vis_client_t **clientv;
};

#endif
