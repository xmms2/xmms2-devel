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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include <time.h>

#include "xmmsc/xmmsc_ipc_transport.h"
#include "xmmsc/xmmsc_idnumbers.h"

#include "common.h"

/**
 * @defgroup Visualization Visualization
 * @ingroup XMMSClient
 * @brief This manages the visualization transfer
 *
 * @{
 */

xmmsc_visualization_t *
get_dataset (xmmsc_connection_t *c, int vv)
{
	if (vv < 0 || vv >= c->visc)
		return NULL;

	return c->visv[vv];
}

/**
 * Querys the visualization version
 */

xmmsc_result_t *
xmmsc_visualization_version (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_VISUALIZATION, XMMS_IPC_CMD_VISUALIZATION_QUERY_VERSION);
}

/**
 * Initializes a new visualization dataset
 */

xmmsc_result_t *
xmmsc_visualization_init (xmmsc_connection_t *c)
{
	xmmsc_result_t *res = NULL;

	x_check_conn (c, 0);

	c->visc++;
	c->visv = realloc (c->visv, sizeof (xmmsc_visualization_t*) * c->visc);
	if (!c->visv) {
		x_oom ();
		c->visc = 0;
	}
	if (c->visc > 0) {
		int vv = c->visc-1;
		if (!(c->visv[vv] = x_new0 (xmmsc_visualization_t, 1))) {
			x_oom ();
		} else {
			c->visv[vv]->idx = vv;
			c->visv[vv]->state = VIS_NEW;
			res = xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_VISUALIZATION, XMMS_IPC_CMD_VISUALIZATION_REGISTER);
			if (res) {
				xmmsc_result_visc_set (res, c->visv[vv]);
			}
		}
	}
	return res;
}

int
xmmsc_visualization_init_handle (xmmsc_result_t *res)
{
	xmmsc_visualization_t *visc;

	if (xmmsc_result_iserror (res)) {
		return -1;
	}
	visc = xmmsc_result_visc_get (res);
	if (!visc) {
		x_api_error_if (1, "non vis result?", -1);
	}
	xmmsc_result_get_int (res, &visc->id);
	visc->type = VIS_NONE;

	return visc->idx;

}

/**
 * Initializes a new visualization connection
 */

xmmsc_result_t *
xmmsc_visualization_start (xmmsc_connection_t *c, int vv)
{
	xmmsc_result_t *res;
	xmmsc_visualization_t *v;

	x_check_conn (c, 0);
	v = get_dataset (c, vv);
	x_api_error_if (!v, "with unregistered/unconnected visualization dataset", 0);

	switch (v->state) {
	case VIS_WORKING:
	case VIS_ERRORED:
		break;
	case VIS_NEW:
#ifdef HAVE_UNIXSHMSUPPORT
		/* first try unixshm */
		v->type = VIS_UNIXSHM;
		res = setup_shm_prepare (c, vv);
		v->state = VIS_TRYING_UNIXSHM;
		break;
#endif
	case VIS_TO_TRY_UDP:
		v->type = VIS_UDP;
		res = setup_udp_prepare (c, vv);
		v->state = VIS_TRYING_UDP;
		break;
	default:
		v->state = VIS_ERRORED;
		x_api_warning ("out of sequence");
		break;
	}

	return res;
}

void
xmmsc_visualization_start_handle (xmmsc_connection_t *c, xmmsc_result_t *res)
{
	xmmsc_visualization_t *v;
	bool ret;

	v = xmmsc_result_visc_get (res);
	if (!v) {
		x_api_error_if (1, "non vis result?", );
	}

	switch (v->state) {
	case VIS_WORKING:
	case VIS_ERRORED:
		break;
	case VIS_TRYING_UNIXSHM:
		ret = setup_shm_handle (res);
		if (!ret) {
			c->error = strdup ("Server doesn't support or couldn't attach shared memory!");
			v->state = VIS_TO_TRY_UDP;
		} else {
			v->state = VIS_WORKING;
		}
		break;
	case VIS_TRYING_UDP:
		ret = setup_udp_handle (res);
		if (!ret) {
			xmms_ipc_msg_t *msg;
			c->error = strdup ("Server doesn't support or couldn't setup UDP!");
			v->state = VIS_ERRORED;
			v->type = VIS_NONE;
			msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_VISUALIZATION, XMMS_IPC_CMD_VISUALIZATION_SHUTDOWN);
			xmms_ipc_msg_put_int32 (msg, v->id);
			xmmsc_send_msg (c, msg);
		} else {
			v->state = VIS_WORKING;
		}
		break;
	default:
		v->state = VIS_ERRORED;
		x_api_warning ("out of sequence");
		break;
	}
}

bool
xmmsc_visualization_started (xmmsc_connection_t *c, int vv)
{
	xmmsc_visualization_t *v;

	x_check_conn (c, 0);
	v = get_dataset (c, vv);
	x_api_error_if (!v, "with unregistered/unconnected visualization dataset", 0);

	return (v->state == VIS_WORKING);
}

bool
xmmsc_visualization_errored (xmmsc_connection_t *c, int vv)
{
	xmmsc_visualization_t *v;

	x_check_conn (c, 0);
	v = get_dataset (c, vv);
	x_api_error_if (!v, "with unregistered/unconnected visualization dataset", 0);

	return (v->state == VIS_ERRORED);
}


/**
 * Deliver one property
 */
xmmsc_result_t *
xmmsc_visualization_property_set (xmmsc_connection_t *c, int vv, const char *key, const char *value)
{
	xmms_ipc_msg_t *msg;
	xmmsc_visualization_t *v;

	x_check_conn (c, NULL);
	v = get_dataset (c, vv);
	x_api_error_if (!v, "with unregistered visualization dataset", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_VISUALIZATION, XMMS_IPC_CMD_VISUALIZATION_PROPERTY);
	xmms_ipc_msg_put_int32 (msg, v->id);
	xmms_ipc_msg_put_string (msg, key);
	xmms_ipc_msg_put_string (msg, value);
	return xmmsc_send_msg (c, msg);
}

/**
 * Deliver some properties
 */
xmmsc_result_t *
xmmsc_visualization_properties_set (xmmsc_connection_t *c, int vv, xmmsc_visualization_properties_t prop)
{
	xmms_ipc_msg_t *msg;
	xmmsc_visualization_t *v;

	x_check_conn (c, NULL);
	v = get_dataset (c, vv);
	x_api_error_if (!v, "with unregistered visualization dataset", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_VISUALIZATION, XMMS_IPC_CMD_VISUALIZATION_PROPERTIES);
	xmms_ipc_msg_put_int32 (msg, v->id);
	xmms_ipc_msg_put_string_list (msg, prop);
	return xmmsc_send_msg (c, msg);
}

/**
 * Says goodbye and cleans up
 */

void
xmmsc_visualization_shutdown (xmmsc_connection_t *c, int vv)
{
	xmms_ipc_msg_t *msg;
	xmmsc_visualization_t *v;

	x_check_conn (c,);
	v = get_dataset (c, vv);
	x_api_error_if (!v, "with unregistered visualization dataset",);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_VISUALIZATION, XMMS_IPC_CMD_VISUALIZATION_SHUTDOWN);
	xmms_ipc_msg_put_int32 (msg, v->id);
	xmmsc_send_msg (c, msg);

	/* detach from shm, close socket.. */
	if (v->type == VIS_UNIXSHM) {
		cleanup_shm (&v->transport.shm);
	}
	if (v->type == VIS_UDP) {
		cleanup_udp (&v->transport.udp);
	}

	free (v);
	c->visv[vv] = NULL;
}

static int
package_read_start (xmmsc_visualization_t *v, unsigned int blocking, xmmsc_vischunk_t **dest)
{
	if (v->type == VIS_UNIXSHM) {
		return read_start_shm (&v->transport.shm, blocking, dest);
	} else if (v->type == VIS_UDP) {
		return read_start_udp (&v->transport.udp, blocking, dest, v->id);
	}
	return -1;
}

static void
package_read_finish (xmmsc_visualization_t *v, xmmsc_vischunk_t *dest)
{
	if (v->type == VIS_UNIXSHM) {
		read_finish_shm (&v->transport.shm, dest);
	} else if (v->type == VIS_UDP) {
		read_finish_udp (&v->transport.udp, dest);
	}
}

/**
 * Fetches the next available data chunk
 */

int
xmmsc_visualization_chunk_get (xmmsc_connection_t *c, int vv, short *buffer, int drawtime, unsigned int blocking)
{
	xmmsc_visualization_t *v;
	xmmsc_vischunk_t *src;
	struct timeval time;
	double diff;
	struct timespec sleeptime;
	int old;
	int i, ret, size;

	x_check_conn (c, 0);
	v = get_dataset (c, vv);
	x_api_error_if (!v, "with unregistered visualization dataset", 0);

	while (1) {
		ret = package_read_start (v, blocking, &src);
		if (ret < 1) {
			return ret;
		}

		if (drawtime >= 0) {
			gettimeofday (&time, NULL);
			diff = net2ts (src->timestamp) - tv2ts (&time);
			if (diff >= 0) {
				double dontcare;
				old = 0;
				/* nanosleep has a garantueed granularity of 10 ms.
				   to not sleep too long, we sleep 10 ms less than intended */
				diff -= (drawtime + 10) * 0.001;
				if (diff < 0) {
					diff = 0;
				}
				sleeptime.tv_sec = diff;
				sleeptime.tv_nsec = modf (diff, &dontcare) * 1000000000;
				while (nanosleep (&sleeptime, &sleeptime) == -1) {
					if (errno != EINTR) {
						break;
					}
				};
			} else {
				old = 1;
			}
		} else {
			old = 0;
		}
		if (!old) {
			size = ntohs (src->size);
			for (i = 0; i < size; ++i) {
				buffer[i] = (int16_t)ntohs (src->data[i]);
			}
		}
		package_read_finish (v, src);
		if (!old) {
			return size;
		}
	}
}

/** @} */

