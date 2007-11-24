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

XMMS_CMD_DEFINE (query_version, xmms_visualization_version, xmms_visualization_t *, UINT32, NONE, NONE);
XMMS_CMD_DEFINE (registercl, xmms_visualization_register_client, xmms_visualization_t *, INT32, NONE, NONE);
XMMS_CMD_DEFINE (init_shm, xmms_visualization_init_shm, xmms_visualization_t *, INT32, INT32, INT32);
XMMS_CMD_DEFINE (init_udp, xmms_visualization_init_udp, xmms_visualization_t *, INT32, INT32, NONE);
XMMS_CMD_DEFINE3 (property_set, xmms_visualization_property_set, xmms_visualization_t *, INT32, INT32, STRING, STRING);
XMMS_CMD_DEFINE (properties_set, xmms_visualization_properties_set, xmms_visualization_t *, INT32, INT32, STRINGLIST);
XMMS_CMD_DEFINE (shutdown, xmms_visualization_shutdown_client, xmms_visualization_t *, NONE, INT32, NONE);

/* create an uninitialised vis client. don't use this method without mutex! */
int32_t
create_client ()
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
		cleanup_udp (&c->transport.udp);
	}

	g_free (c);
	vis->clientv[id] = NULL;

	xmms_log_info ("Removed visualization client %d", id);
}

/**
 * Initialize the Vis module.
 */
void
xmms_visualization_init (xmms_output_t *output)
{
	vis = xmms_object_new (xmms_visualization_t, xmms_visualization_destroy);
	vis->clientlock = g_mutex_new ();
	vis->clientc = 0;
	vis->output = output;

	xmms_ipc_object_register (XMMS_IPC_OBJECT_VISUALIZATION, XMMS_OBJECT (vis));
	xmms_object_cmd_add (XMMS_OBJECT (vis),
	                     XMMS_IPC_CMD_VISUALIZATION_QUERY_VERSION,
	                     XMMS_CMD_FUNC (query_version));
	xmms_object_cmd_add (XMMS_OBJECT (vis),
	                     XMMS_IPC_CMD_VISUALIZATION_REGISTER,
	                     XMMS_CMD_FUNC (registercl));
	xmms_object_cmd_add (XMMS_OBJECT (vis),
	                     XMMS_IPC_CMD_VISUALIZATION_INIT_SHM,
	                     XMMS_CMD_FUNC (init_shm));
	xmms_object_cmd_add (XMMS_OBJECT (vis),
	                     XMMS_IPC_CMD_VISUALIZATION_INIT_UDP,
	                     XMMS_CMD_FUNC (init_udp));
	xmms_object_cmd_add (XMMS_OBJECT (vis),
	                     XMMS_IPC_CMD_VISUALIZATION_PROPERTY,
	                     XMMS_CMD_FUNC (property_set));
	xmms_object_cmd_add (XMMS_OBJECT (vis),
	                     XMMS_IPC_CMD_VISUALIZATION_PROPERTIES,
	                     XMMS_CMD_FUNC (properties_set));
	xmms_object_cmd_add (XMMS_OBJECT (vis),
	                     XMMS_IPC_CMD_VISUALIZATION_SHUTDOWN,
	                     XMMS_CMD_FUNC (shutdown));

	xmms_socket_invalidate (&vis->socket);
}

/**
 * Free all resoures used by visualization module.
 * TODO: Fill this in properly, unregister etc!
 */

void
xmms_visualization_destroy ()
{
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
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_VISUALIZATION);
}

uint32_t
xmms_visualization_version (xmms_visualization_t *vis, xmms_error_t *err) {
	/* if there is a way to disable visualization support on the server side,
	   we could return 0 here, or we could return an error? */

	return XMMS_VISPACKET_VERSION;
}

void
properties_init (xmmsc_vis_properties_t *p) {
	p->type = VIS_PCM;
	p->stereo = 1;
	p->pcm_hardwire = 0;
}

gboolean
property_set (xmmsc_vis_properties_t *p, gchar* key, gchar* data) {

	if (!g_strcasecmp (key, "type")) {
		if (!g_strcasecmp (data, "pcm")) {
			p->type = VIS_PCM;
		} else if (!g_strcasecmp (data, "fft")) {
			p->type = VIS_FFT;
		} else if (!g_strcasecmp (data, "peak")) {
			p->type = VIS_PEAK;
		} else {
			return FALSE;
		}
	} else if (!g_strcasecmp (key, "stereo")) {
		p->stereo = (atoi (data) > 0);
	} else if (!g_strcasecmp (key, "pcm.hardwire")) {
		p->pcm_hardwire = (atoi (data) > 0);
	} else if (!g_strcasecmp (key, "timeframe")) {
		p->timeframe = g_strtod (data, NULL);
		if (p->timeframe == 0.0) {
			return FALSE;
		}
		/* TODO: all the stuff here, eh */
	} else {
		return FALSE;
	}
	return TRUE;
}

int32_t
xmms_visualization_register_client (xmms_visualization_t *vis, xmms_error_t *err)
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


int32_t
xmms_visualization_property_set (xmms_visualization_t *vis, int32_t id, gchar* key, gchar* value, xmms_error_t *err)
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

int32_t
xmms_visualization_properties_set (xmms_visualization_t *vis, int32_t id, GList* prop, xmms_error_t *err)
{
	GList* n;
	xmms_vis_client_t *c;

	x_fetch_client (id);

	for (n = prop; n; n = n->next) {
		if (!property_set (&c->prop, (gchar*)n->data, (gchar*)n->next->data)) {
			xmms_error_set (err, XMMS_ERROR_INVAL, "property could not be set!");
		}
		n = n->next;
	}
	/* TODO: propagate new format to xform! */

	x_release_client ();

	return (++c->format);
}

int32_t
xmms_visualization_init_shm (xmms_visualization_t *vis, int32_t id, int32_t shmid, xmms_error_t *err)
{
	return init_shm (vis, id, shmid, err);
}

int32_t
xmms_visualization_init_udp (xmms_visualization_t *vis, int32_t id, xmms_error_t *err)
{
	return init_udp (vis, id, err);
}

void
xmms_visualization_shutdown_client (xmms_visualization_t *vis, int32_t id, xmms_error_t *err)
{
	g_mutex_lock (vis->clientlock);
	delete_client (id);
	g_mutex_unlock (vis->clientlock);
}

gboolean
package_write_start (int32_t id, xmms_vis_client_t* c, xmmsc_vischunk_t **dest) {
	if (c->type == VIS_UNIXSHM) {
		return write_start_shm (id, &c->transport.shm, dest);
	} else if (c->type == VIS_UDP) {
		return write_start_udp (id, &c->transport.udp, dest);
	}
	return FALSE;
}

void
package_write_finish (int32_t id, xmms_vis_client_t* c, xmmsc_vischunk_t *dest) {
	if (c->type == VIS_UNIXSHM) {
		write_finish_shm (id, &c->transport.shm, dest);
	} else if (c->type == VIS_UDP) {
		write_finish_udp (id, &c->transport.udp, dest, vis->socket);
	}
}

/* you know ... */
short
fill_buffer (int16_t *dest, xmmsc_vis_properties_t* prop, int channels, int size, short *src) {
	int i, j;
	if (prop->type == VIS_PEAK) {
		short l = 0, r = 0;
		for (i = 0; i < size; i += channels) {
			if (src[i] > 0 && src[i] > l) {
				l = src[i];
			}
			if (src[i] < 0 && -src[i] > l) {
				l = -src[i];
			}
			if (channels > 1) {
				if (src[i+1] > 0 && src[i+1] > r) {
					r = src[i+1];
				}
				if (src[i+1] < 0 && -src[i+1] > r) {
					r = -src[i+1];
				}
			}
		}
		if (channels == 1) {
			r = l;
		}
		if (prop->stereo) {
			dest[0] = htons (l);
			dest[1] = htons (r);
			size = 2;
		} else {
			dest[0] = htons ((l + r) / 2);
			size = 1;
		}
	}
	if (prop->type == VIS_PCM) {
		for (i = 0, j = 0; i < size; i += channels, j++) {
			short *l, *r;
			if (prop->pcm_hardwire) {
				l = &dest[j*2];
				r = &dest[j*2 + 1];
			} else {
				l = &dest[j];
				r = &dest[size/channels + j];
			}
			*l = htons (src[i]);
			if (prop->stereo) {
				if (channels > 1) {
					*r = htons (src[i+1]);
				} else {
					*r = htons (src[i]);
				}
			}
		}
		size /= channels;
		if (prop->stereo) {
			size *= 2;
		}
	}
	return size;
}


void
send_data (int channels, int size, short *buf) {
	int i;
	xmmsc_vischunk_t *dest;
	struct timeval time;
	guint32 latency = xmms_output_latency (vis->output);

	if (!vis) {
		return;
	}

	gettimeofday (&time, NULL);
	g_mutex_lock (vis->clientlock);
	for (i = 0; i < vis->clientc; ++i) {
		if (vis->clientv[i]) {
			if (!package_write_start (i, vis->clientv[i], &dest)) {
				continue;
			}

			ts2net (dest->timestamp, tv2ts (&time) + latency * 0.001);
			dest->format = htons (vis->clientv[i]->format);

			dest->size = htons (fill_buffer (dest->data, &vis->clientv[i]->prop, channels, size, buf));

			package_write_finish (i, vis->clientv[i], dest);
		}
	}
	g_mutex_unlock (vis->clientlock);
}

/** @} */
