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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "xmms/xmms_object.h"
#include "xmmspriv/xmms_ipc.h"
#include "xmmspriv/xmms_sample.h"

#include "common.h"

/** @defgroup Visualization Visualization
  * @ingroup XMMSServer
  * @brief Feeds playing data in various forms to the client.
  * @{
  */

static xmms_visualization_t *vis = NULL;

static int32_t xmms_visualization_client_query_version (xmms_visualization_t *vis, xmms_error_t *err);
static int32_t xmms_visualization_client_register (xmms_visualization_t *vis, xmms_error_t *err);
static int32_t xmms_visualization_client_init_shm (xmms_visualization_t *vis, int32_t id, const char *shmid, xmms_error_t *err);
static int32_t xmms_visualization_client_init_udp (xmms_visualization_t *vis, int32_t id, xmms_error_t *err);
static int32_t xmms_visualization_client_set_property (xmms_visualization_t *vis, int32_t id, const gchar *key, const gchar *value, xmms_error_t *err);
static int32_t xmms_visualization_client_set_properties (xmms_visualization_t *vis, int32_t id, xmmsv_t *prop, xmms_error_t *err);
static void xmms_visualization_client_shutdown (xmms_visualization_t *vis, int32_t id, xmms_error_t *err);
static void xmms_visualization_destroy (xmms_object_t *object);

#include "visualization/object_ipc.c"

/* create an uninitialised vis client. don't use this method without mutex! */
static int32_t
create_client (void)
{
	int32_t id;

	for (id = 0; id < vis->clientc; ++id) {
		if (!vis->clientv[id]) {
			break;
		}
	}

	if (id == vis->clientc) {
		vis->clientc++;
	}

	vis->clientv = g_renew (xmms_vis_client_t*, vis->clientv, vis->clientc);
	if (!vis->clientv || (!(vis->clientv[id] = g_new (xmms_vis_client_t, 1)))) {
		vis->clientc = 0;
		id = -1;
	}

	xmms_log_info ("Attached visualization client %d", id);
	return id;
}

xmms_vis_client_t *
get_client (int32_t id)
{
	if (id < 0 || id >= vis->clientc) {
		return NULL;
	}

	return vis->clientv[id];
}

/* delete a vis client. don't use this method without mutex! */
void
delete_client (int32_t id)
{
	xmms_vis_client_t *c;

	if (id < 0 || id >= vis->clientc) {
		return;
	}

	c = vis->clientv[id];
	if (c == NULL) {
		return;
	}

	if (c->type == VIS_UNIXSHM) {
		cleanup_shm (&c->transport.shm);
	} else if (c->type == VIS_UDP) {
		cleanup_udp (&c->transport.udp, vis->socket);
	}

	g_free (c);
	vis->clientv[id] = NULL;

	xmms_log_info ("Removed visualization client %d", id);
}

/**
 * Initialize the Vis module.
 */
xmms_visualization_t *
xmms_visualization_new (xmms_output_t *output)
{
	vis = xmms_object_new (xmms_visualization_t, xmms_visualization_destroy);
	vis->clientlock = g_mutex_new ();
	vis->clientc = 0;
	vis->output = output;

	xmms_object_ref (output);

	xmms_visualization_register_ipc_commands (XMMS_OBJECT (vis));

	xmms_socket_invalidate (&vis->socket);

	return vis;
}

/**
 * Free all resoures used by visualization module.
 * TODO: Fill this in properly, unregister etc!
 */

static void
xmms_visualization_destroy (xmms_object_t *object)
{
	xmms_object_unref (vis->output);

	/* TODO: assure that the xform is already dead! */
	g_mutex_free (vis->clientlock);
	xmms_log_debug ("starting cleanup of %d vis clients", vis->clientc);
	for (; vis->clientc > 0; --vis->clientc) {
		delete_client (vis->clientc - 1);
	}

	if (xmms_socket_valid (vis->socket)) {
		/* it seems there is no way to remove the watch */
		g_io_channel_shutdown (vis->socketio, FALSE, NULL);
		xmms_socket_close (vis->socket);
	}

	xmms_visualization_unregister_ipc_commands ();
}

static int32_t
xmms_visualization_client_query_version (xmms_visualization_t *vis, xmms_error_t *err)
{
	/* if there is a way to disable visualization support on the server side,
	   we could return 0 here, or we could return an error? */

	return XMMS_VISPACKET_VERSION;
}

static void
properties_init (xmmsc_vis_properties_t *p)
{
	p->type = VIS_PCM;
	p->stereo = 1;
	p->pcm_hardwire = 0;
}

static gboolean
property_set (xmmsc_vis_properties_t *p, const gchar* key, const gchar* data)
{

	if (!g_strcasecmp (key, "type")) {
		if (!g_strcasecmp (data, "pcm")) {
			p->type = VIS_PCM;
		} else if (!g_strcasecmp (data, "spectrum")) {
			p->type = VIS_SPECTRUM;
		} else if (!g_strcasecmp (data, "peak")) {
			p->type = VIS_PEAK;
		} else {
			return FALSE;
		}
	} else if (!g_strcasecmp (key, "stereo")) {
		p->stereo = (atoi (data) > 0);
	} else if (!g_strcasecmp (key, "pcm.hardwire")) {
		p->pcm_hardwire = (atoi (data) > 0);
	/* TODO: all the stuff following */
	} else if (!g_strcasecmp (key, "timeframe")) {
		p->timeframe = g_strtod (data, NULL);
		if (p->timeframe == 0.0) {
			return FALSE;
		}
	} else {
		return FALSE;
	}
	return TRUE;
}

static int32_t
xmms_visualization_client_register (xmms_visualization_t *vis, xmms_error_t *err)
{
	int32_t id;
	xmms_vis_client_t *c;

	g_mutex_lock (vis->clientlock);
	id = create_client ();
	if (id < 0) {
		xmms_error_set (err, XMMS_ERROR_OOM, "could not allocate dataset");
	} else {
		/* do necessary initialisations here */
		c = get_client (id);
		c->type = VIS_NONE;
		c->format = 0;
		properties_init (&c->prop);
	}
	g_mutex_unlock (vis->clientlock);
	return id;
}


static int32_t
xmms_visualization_client_set_property (xmms_visualization_t *vis, int32_t id, const gchar* key, const gchar* value, xmms_error_t *err)
{
	xmms_vis_client_t *c;

	x_fetch_client (id);

	if (!property_set (&c->prop, key, value)) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "property could not be set!");
	}

	x_release_client ();

	/* the format identifier (between client and server) changes. so the client can recognize the first packet
	   which is built using the new format according to the newly set property */
	return (++c->format);
}

static int32_t
xmms_visualization_client_set_properties (xmms_visualization_t *vis, int32_t id, xmmsv_t* prop, xmms_error_t *err)
{
	xmms_vis_client_t *c;
	xmmsv_dict_iter_t *it;
	const gchar *key, *valstr;
	xmmsv_t *value;

	x_fetch_client (id);

	if (!xmmsv_get_type (prop) == XMMSV_TYPE_DICT) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "properties must be sent as a dict!");
	} else {
		/* record every pair */
		xmmsv_get_dict_iter (prop, &it);
		while (xmmsv_dict_iter_valid (it)) {
			if (!xmmsv_dict_iter_pair (it, &key, &value)) {
				xmms_error_set (err, XMMS_ERROR_INVAL, "key-value property pair could not be read!");
			} else if (!xmmsv_get_string (value, &valstr)) {
				xmms_error_set (err, XMMS_ERROR_INVAL, "property value could not be read!");
			} else if (!property_set (&c->prop, key, valstr)) {
				xmms_error_set (err, XMMS_ERROR_INVAL, "property could not be set!");
			}
			xmmsv_dict_iter_next (it);
		}
		/* TODO: propagate new format to xform! */
	}

	x_release_client ();

	return (++c->format);
}

static int32_t
xmms_visualization_client_init_shm (xmms_visualization_t *vis, int32_t id, const char *shmidstr, xmms_error_t *err)
{
	int shmid;

	XMMS_DBG ("Trying to init shm!");

	if (sscanf (shmidstr, "%d", &shmid) != 1) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "couldn't parse shmid");
		return -1;
	}
	return init_shm (vis, id, shmid, err);
}

static int32_t
xmms_visualization_client_init_udp (xmms_visualization_t *vis, int32_t id, xmms_error_t *err)
{
	XMMS_DBG ("Trying to init udp!");
	return init_udp (vis, id, err);
}

static void
xmms_visualization_client_shutdown (xmms_visualization_t *vis, int32_t id, xmms_error_t *err)
{
	g_mutex_lock (vis->clientlock);
	delete_client (id);
	g_mutex_unlock (vis->clientlock);
}

static gboolean
package_write (xmms_vis_client_t *c, int32_t id, struct timeval *time, int channels, int size, short *buf)
{
	if (c->type == VIS_UNIXSHM) {
		return write_shm (&c->transport.shm, c, id, time, channels, size, buf);
	} else if (c->type == VIS_UDP) {
		return write_udp (&c->transport.udp, c, id, time, channels, size, buf, vis->socket);
	}
	return FALSE;
}

void
send_data (int channels, int size, short *buf)
{
	int i;
	struct timeval time;
	guint32 latency;

	if (!vis) {
		return;
	}

	latency = xmms_output_latency (vis->output);

	fft_init ();

	gettimeofday (&time, NULL);
	time.tv_sec += (latency / 1000);
	time.tv_usec += (latency % 1000) * 1000;
	if (time.tv_usec > 1000000) {
		time.tv_sec++;
		time.tv_usec -= 1000000;
	}

	g_mutex_lock (vis->clientlock);
	for (i = 0; i < vis->clientc; ++i) {
		if (vis->clientv[i]) {
			package_write (vis->clientv[i], i, &time, channels, size, buf);
		}
	}
	g_mutex_unlock (vis->clientlock);
}

/** @} */
