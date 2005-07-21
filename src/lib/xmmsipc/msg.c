/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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


#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>

#include "xmmsc/xmmsc_ipc_transport.h"
#include "xmmsc/xmmsc_ipc_msg.h"
#include "xmmsc/xmmsc_util.h"

typedef union {
	struct {
		uint32_t object;
		uint32_t cmd;
		uint32_t cid;
		uint32_t length;
		uint8_t data[0];
	} header;
	uint8_t rawdata[0];
} xmms_ipc_msg_data_t;


struct xmms_ipc_msg_St {
	xmms_ipc_msg_data_t *data;
	uint32_t get_pos;
	uint32_t size;
	uint32_t xfered;
};

xmms_ipc_msg_t *
xmms_ipc_msg_alloc (void)
{
	xmms_ipc_msg_t *msg;
	
	msg = x_new0 (xmms_ipc_msg_t, 1);
	msg->data = x_malloc0 (XMMS_IPC_MSG_DEFAULT_SIZE);
	msg->size = XMMS_IPC_MSG_DEFAULT_SIZE;
	return msg;
}

xmms_ipc_msg_t *
xmms_ipc_msg_new (uint32_t object, uint32_t cmd)
{
	xmms_ipc_msg_t *msg = xmms_ipc_msg_alloc ();

	xmms_ipc_msg_set_cmd (msg, cmd);
	xmms_ipc_msg_set_object (msg, object);

	return msg;
}

void
xmms_ipc_msg_destroy (xmms_ipc_msg_t *msg)
{
	x_return_if_fail (msg);
	free (msg->data);
	free (msg);
}

void
xmms_ipc_msg_set_length (xmms_ipc_msg_t *msg, uint32_t len)
{
	x_return_if_fail (msg);
	msg->data->header.length = htonl (len);
}

uint32_t
xmms_ipc_msg_get_length (const xmms_ipc_msg_t *msg)
{
	x_return_val_if_fail (msg, 0);

	return ntohl (msg->data->header.length);
}

uint32_t
xmms_ipc_msg_get_object (const xmms_ipc_msg_t *msg)
{
	x_return_val_if_fail (msg, 0);

	return ntohl (msg->data->header.object);
}

void
xmms_ipc_msg_set_object (xmms_ipc_msg_t *msg, uint32_t object)
{
	x_return_if_fail (msg);

	msg->data->header.object = htonl (object);
}


uint32_t
xmms_ipc_msg_get_cmd (const xmms_ipc_msg_t *msg)
{
	x_return_val_if_fail (msg, 0);

	return ntohl (msg->data->header.cmd);
}

void
xmms_ipc_msg_set_cmd (xmms_ipc_msg_t *msg, uint32_t cmd)
{
	x_return_if_fail (msg);

	msg->data->header.cmd = htonl (cmd);
}

void
xmms_ipc_msg_set_cid (xmms_ipc_msg_t *msg, uint32_t cid)
{
	msg->data->header.cid = htonl (cid);
}

uint32_t
xmms_ipc_msg_get_cid (const xmms_ipc_msg_t *msg)
{
	x_return_val_if_fail (msg, 0);

	return ntohl (msg->data->header.cid);
}

/**
 * Try to write message to transport. If full message isn't written
 * the message will keep track of the amount of data written and not
 * write already written data next time.
 *
 * @returns TRUE if full message was written, FALSE otherwise.
 *               disconnected is set if transport was disconnected
 */
bool
xmms_ipc_msg_write_transport (xmms_ipc_msg_t *msg, xmms_ipc_transport_t *transport, bool *disconnected)
{
	unsigned int ret, len;

	x_return_val_if_fail (msg, false);
	x_return_val_if_fail (msg->data, false);
	x_return_val_if_fail (transport, false);

	len = xmms_ipc_msg_get_length (msg) + XMMS_IPC_MSG_HEAD_LEN;
	
	x_return_val_if_fail (len > msg->xfered, true);
	
	ret = xmms_ipc_transport_write (transport, 
					(char *)(msg->data->rawdata + msg->xfered),
					len - msg->xfered);
	if (ret == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return false;
		if (disconnected)
			*disconnected = true;
		return false;
	} else if (ret == 0) {
		if (disconnected)
			*disconnected = true;
	} else {
		msg->xfered += ret;
	}

	return len == msg->xfered;
}

/**
 * Try to read message from transport into msg.
 *
 * @returns TRUE if message is fully read.
 */
bool
xmms_ipc_msg_read_transport (xmms_ipc_msg_t *msg, xmms_ipc_transport_t *transport, bool *disconnected)
{
	unsigned int ret, len;

	x_return_val_if_fail (msg, false);
	x_return_val_if_fail (transport, false);

	while (true) {
		len = XMMS_IPC_MSG_HEAD_LEN;
		if (msg->xfered >= XMMS_IPC_MSG_HEAD_LEN) {
			len += xmms_ipc_msg_get_length (msg);
			if (len > msg->size) {
				msg->size = len;
				msg->data = realloc (msg->data, msg->size);
			}
			if (msg->xfered == len)
				return true;
		}


		x_return_val_if_fail (msg->xfered < len, false);

		ret = xmms_ipc_transport_read (transport, 
					       (char *)(msg->data->rawdata + msg->xfered),
					       len - msg->xfered);
		
		if (ret == -1) {
			if (errno == EAGAIN || errno == EINTR)
				return false;
			if (disconnected)
				*disconnected = true;
			return false;
		} else if (ret == 0) {
			if (disconnected)
				*disconnected = true;
			return false;
		} else {
			msg->xfered += ret;
		}
	}
}


static void*
xmms_ipc_msg_put_data (xmms_ipc_msg_t *msg, const void *data, unsigned int len)
{
	x_return_val_if_fail (msg, NULL);
	
	if ((xmms_ipc_msg_get_length (msg) + XMMS_IPC_MSG_HEAD_LEN + len) > msg->size) {
		int reallocsize = XMMS_IPC_MSG_DEFAULT_SIZE;

		if (len > XMMS_IPC_MSG_DEFAULT_SIZE) 
			reallocsize = len;

		/* Realloc data portion */
		msg->data = realloc (msg->data, msg->size + reallocsize);
		msg->size += reallocsize;
	}
	memcpy (&msg->data->header.data[xmms_ipc_msg_get_length (msg)], data, len);
	xmms_ipc_msg_set_length (msg, xmms_ipc_msg_get_length (msg) + len);

	return &msg->data->rawdata[xmms_ipc_msg_get_length (msg) - len];
}

void*
xmms_ipc_msg_put_uint32 (xmms_ipc_msg_t *msg, uint32_t v)
{
	v = htonl (v);
	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

void*
xmms_ipc_msg_put_int32 (xmms_ipc_msg_t *msg, int32_t v)
{
	v = htonl (v);
	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

void*
xmms_ipc_msg_put_float (xmms_ipc_msg_t *msg, float v)
{
	/** @todo do we need to convert ? */
	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

void*
xmms_ipc_msg_put_string (xmms_ipc_msg_t *msg, const char *str)
{
	if (!msg)
		return NULL;

	if (!str)
		return xmms_ipc_msg_put_uint32 (msg, 0);
	
	xmms_ipc_msg_put_uint32 (msg, strlen (str) + 1);

	return xmms_ipc_msg_put_data (msg, str, strlen (str) + 1);
}

void
xmms_ipc_msg_get_reset (xmms_ipc_msg_t *msg)
{
	msg->get_pos = 0;
}

static bool
xmms_ipc_msg_get_data (xmms_ipc_msg_t *msg, void *buf, unsigned int len)
{
	if (!msg || ((msg->get_pos + len) > xmms_ipc_msg_get_length (msg)))
		return false;

	if (buf)
		memcpy (buf, &msg->data->header.data[msg->get_pos], len);
	msg->get_pos += len;

	return true;
}

bool
xmms_ipc_msg_get_uint32 (xmms_ipc_msg_t *msg, uint32_t *v)
{
	bool ret;
	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));
	if (v)
		*v = ntohl (*v);
	return ret;
}

bool
xmms_ipc_msg_get_int32 (xmms_ipc_msg_t *msg, int32_t *v)
{
	bool ret;
	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));
	if (v)
		*v = ntohl (*v);
	return ret;
}

bool
xmms_ipc_msg_get_float (xmms_ipc_msg_t *msg, float *v)
{
	bool ret;
	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));
	/** @todo do we need to convert? */
	return ret;
}

bool
xmms_ipc_msg_get_string_alloc (xmms_ipc_msg_t *msg, char **buf, unsigned int *len)
{
	if (!xmms_ipc_msg_get_uint32 (msg, len))
		return false;
	
	*buf = x_malloc0 (*len+1);

	if (!*buf)
		return false;

	if (!xmms_ipc_msg_get_data (msg, *buf, *len))
		return false;

	(*buf)[*len] = '\0';

	return true;
}

bool
xmms_ipc_msg_get_string (xmms_ipc_msg_t *msg, char *buf, unsigned int maxlen)
{
	uint32_t len;

	if (buf) {
		buf[maxlen - 1] = '\0';
		maxlen--;
	}
	if (!xmms_ipc_msg_get_uint32 (msg, &len))
		return false;

	if(len == 0) {
		buf[0] = '\0';
		return true;
	}

	if (!xmms_ipc_msg_get_data (msg, buf, MIN (maxlen, len)))
		return false;
	if (maxlen < len) {
		xmms_ipc_msg_get_data (msg, NULL, len - maxlen);
	}
	return true;
}

bool
xmms_ipc_msg_get (xmms_ipc_msg_t *msg, ...)
{
	va_list ap;
	void *dest;
	xmms_ipc_msg_arg_type_t type;
	int len;

	va_start (ap, msg);

	while (42) {
		type = va_arg (ap, xmms_ipc_msg_arg_type_t);
		switch (type){
			case XMMS_IPC_MSG_ARG_TYPE_UINT32:
				dest = va_arg (ap, uint32_t *);
				if (!xmms_ipc_msg_get_uint32 (msg, dest)) {
					return false;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_INT32:
				dest = va_arg (ap, int32_t *);
				if (!xmms_ipc_msg_get_int32 (msg, dest)) {
					return false;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_FLOAT:
				dest = va_arg (ap, float *);
				if (!xmms_ipc_msg_get_float (msg, dest)) {
					return false;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_STRING:
				len = va_arg (ap, int);
				dest = va_arg (ap, char *);
				if (!xmms_ipc_msg_get_string (msg, dest, len)) {
					return false;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_DATA:
				len = va_arg (ap, int);
				dest = va_arg (ap, void *);
				if (!xmms_ipc_msg_get_data (msg, dest, len)) {
					return false;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_END:
				va_end (ap);
				return true;
		}
		
	}
}
