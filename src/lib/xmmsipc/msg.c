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


#include <glib.h>
#include <string.h>
#include <unistd.h>
#include "xmms/ipc_msg.h"
#include "xmms/ringbuf.h"
#include "xmms/util.h"

xmms_ipc_msg_t *
xmms_ipc_msg_new (guint32 object, guint32 cmd)
{
	xmms_ipc_msg_t *msg;
	
	msg = g_new0 (xmms_ipc_msg_t, 1);
	msg->cmd = cmd;
	msg->object = object;
	msg->get_pos = 0;
	msg->data_length = 0;
	msg->data = g_malloc0 (XMMS_IPC_MSG_DEFAULT_SIZE);
	msg->size = XMMS_IPC_MSG_DEFAULT_SIZE;

	return msg;
}

void
xmms_ipc_msg_destroy (xmms_ipc_msg_t *msg)
{
	g_return_if_fail (msg);
	g_free (msg->data);
	g_free (msg);
}

gboolean
xmms_ipc_msg_can_read (xmms_ringbuf_t *ringbuf)
{
        guint32 cmd;
        guint32 object;
	guint32 cmdid;
        guint32 length;
        gboolean ret = FALSE;

        if (xmms_ringbuf_bytes_used (ringbuf) < 16)
                return FALSE;

        xmms_ringbuf_read (ringbuf, &object, sizeof (guint32));
        xmms_ringbuf_read (ringbuf, &cmd, sizeof (guint32));
        xmms_ringbuf_read (ringbuf, &cmdid, sizeof (guint32));
        xmms_ringbuf_read (ringbuf, &length, sizeof (guint32));

        length = g_ntohl (length);
        if (xmms_ringbuf_bytes_used (ringbuf) >= length)
                ret = TRUE;

        xmms_ringbuf_unread (ringbuf, 16);

        return ret;
}

xmms_ipc_msg_t *
xmms_ipc_msg_read (xmms_ringbuf_t *ringbuf)
{
	xmms_ipc_msg_t *msg;
	guint32 cmd;
	guint32 cmdid;
	guint32 object;

	if (xmms_ringbuf_bytes_used (ringbuf) < 16)
		return NULL;

	xmms_ringbuf_read (ringbuf, &object, sizeof (guint32));
	object = g_ntohl (object);
	xmms_ringbuf_read (ringbuf, &cmd, sizeof (guint32));
	cmd = g_ntohl (cmd);
	xmms_ringbuf_read (ringbuf, &cmdid, sizeof (guint32));
	cmdid = g_ntohl (cmdid);

	msg = xmms_ipc_msg_new (object, cmd);
	msg->cid = cmdid;

	g_return_val_if_fail (msg, NULL);
	
	xmms_ringbuf_read (ringbuf, &msg->data_length, sizeof (msg->data_length));
	msg->data_length = g_ntohl (msg->data_length);
	
	if (xmms_ringbuf_bytes_used (ringbuf) < msg->data_length) {
		g_printerr ("Not enough data in buffer (had %d, want %d)\n", xmms_ringbuf_bytes_used (ringbuf), msg->data_length);
		xmms_ipc_msg_destroy (msg);
        	xmms_ringbuf_unread (ringbuf, 16);
		return NULL;
	}

	if (msg->data_length > msg->size) {
		msg->data = g_realloc (msg->data, msg->data_length);
	}

	xmms_ringbuf_read (ringbuf, msg->data, msg->data_length);
	return msg;
}

gboolean
xmms_ipc_msg_write (xmms_ringbuf_t *ringbuf, const xmms_ipc_msg_t *msg, guint32 cid)
{
	guint32 cmd;
	guint32 object;
	guint32 len;
	guint32 cmdid;

	g_return_val_if_fail (xmms_ringbuf_bytes_free (ringbuf) > (msg->data_length +10), FALSE);

	object = g_htonl (msg->object);
	cmd = g_htonl (msg->cmd);
	len = g_htonl (msg->data_length);
	cmdid = g_htonl (cid);

	xmms_ringbuf_write (ringbuf, &object, sizeof (object));
	xmms_ringbuf_write (ringbuf, &cmd, sizeof (cmd));
	xmms_ringbuf_write (ringbuf, &cmdid, sizeof (cmdid));
	xmms_ringbuf_write (ringbuf, &len, sizeof (len));
	xmms_ringbuf_write (ringbuf, msg->data, msg->data_length);

	return TRUE;
}

gboolean
xmms_ipc_msg_write_fd (gint fd, const xmms_ipc_msg_t *msg, guint32 cid)
{
	guint32 cmd;
	guint32 object;
	guint32 cmdid;
	guint32 len;
	guint32 data_len;
	
	cmd = g_htonl (msg->cmd);
	cmdid = g_htonl (cid);
	object = g_htonl (msg->object);
	len = g_htonl (msg->data_length);

	if (write (fd, &object, sizeof (object)) != sizeof (object))
		return FALSE;
	if (write (fd, &cmd, sizeof (cmd)) != sizeof (cmd))
		return FALSE;
	if (write (fd, &cmdid, sizeof (cmdid)) != sizeof (cmdid))
		return FALSE;
	if (write (fd, &len, sizeof (len)) != sizeof (len))
		return FALSE;
	data_len = 0;
	while (data_len < msg->data_length) {
		guint32 ret;

		ret = write (fd, msg->data + data_len, msg->data_length - data_len);
		if (ret == 0)
			return FALSE;
		if (ret == -1)
			continue;

		data_len += ret;
	}
	
	return TRUE;
}

gpointer
xmms_ipc_msg_put_data (xmms_ipc_msg_t *msg, gconstpointer data, guint len)
{
	g_return_val_if_fail (msg, NULL);
	g_return_val_if_fail ((msg->data_length + len) < XMMS_IPC_MSG_MAX_SIZE, NULL);
	
	if ((msg->data_length + len) > msg->size) {
		gint reallocsize = XMMS_IPC_MSG_DEFAULT_SIZE;

		if (len > XMMS_IPC_MSG_DEFAULT_SIZE) 
			reallocsize = len;

		/* Realloc data portion */
		msg->data = g_realloc (msg->data, msg->size + reallocsize);
		msg->size += reallocsize;
		XMMS_DBG ("Realloc to size %d", msg->size);
	}
	memcpy (&msg->data[msg->data_length], data, len);
	msg->data_length += len;

	return &msg->data[msg->data_length - len];
}

gpointer
xmms_ipc_msg_put_uint32 (xmms_ipc_msg_t *msg, guint32 v)
{
	v = g_htonl (v);
	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

gpointer
xmms_ipc_msg_put_int32 (xmms_ipc_msg_t *msg, gint32 v)
{
	v = g_htonl (v);
	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

gpointer
xmms_ipc_msg_put_float (xmms_ipc_msg_t *msg, gfloat v)
{
	/** @todo do we need to convert ? */
	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

gpointer
xmms_ipc_msg_put_string (xmms_ipc_msg_t *msg, const char *str)
{
	if (!msg)
		return NULL;

	if (!str)
		return xmms_ipc_msg_put_uint32 (msg, 0);
	
	if ((msg->data_length + strlen (str) + 5) > XMMS_IPC_MSG_MAX_SIZE)
		return NULL;

	xmms_ipc_msg_put_uint32 (msg, strlen (str) + 1);

	return xmms_ipc_msg_put_data (msg, str, strlen (str) + 1);
}

void
xmms_ipc_msg_get_reset (xmms_ipc_msg_t *msg)
{
	msg->get_pos = 0;
}

gboolean
xmms_ipc_msg_get_data (xmms_ipc_msg_t *msg, gpointer buf, guint len)
{
	if (!msg || ((msg->get_pos + len) > msg->data_length))
		return FALSE;

	if (buf)
		memcpy (buf, &msg->data[msg->get_pos], len);
	msg->get_pos += len;

	return TRUE;
}

gboolean
xmms_ipc_msg_get_uint32 (xmms_ipc_msg_t *msg, guint32 *v)
{
	gboolean ret;
	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));
	if (v)
		*v = g_ntohl (*v);
	return ret;
}

gboolean
xmms_ipc_msg_get_int32 (xmms_ipc_msg_t *msg, gint32 *v)
{
	gboolean ret;
	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));
	if (v)
		*v = g_ntohl (*v);
	return ret;
}

gboolean
xmms_ipc_msg_get_float (xmms_ipc_msg_t *msg, gfloat *v)
{
	gboolean ret;
	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));
	/** @todo do we need to convert? */
	return ret;
}

gboolean
xmms_ipc_msg_get_string_alloc (xmms_ipc_msg_t *msg, char **buf, guint *len)
{
	if (!xmms_ipc_msg_get_uint32 (msg, len))
		return FALSE;
	
	*buf = g_malloc0 (*len+1);

	if (!*buf)
		return FALSE;

	if (!xmms_ipc_msg_get_data (msg, *buf, *len))
		return FALSE;

	(*buf)[*len] = '\0';

	return TRUE;
}

gboolean
xmms_ipc_msg_get_string (xmms_ipc_msg_t *msg, char *buf, guint maxlen)
{
	guint32 len;

	if (buf) {
		buf[maxlen - 1] = '\0';
		maxlen--;
	}
	if (!xmms_ipc_msg_get_uint32 (msg, &len))
		return FALSE;
	if (!xmms_ipc_msg_get_data (msg, buf, MIN (maxlen, len)))
		return FALSE;
	if (maxlen < len) {
		xmms_ipc_msg_get_data (msg, NULL, len - maxlen);
	}
	return TRUE;
}

gboolean
xmms_ipc_msg_get (xmms_ipc_msg_t *msg, ...)
{
	va_list ap;
	void *dest;
	xmms_ipc_msg_arg_type_t type;
	gint len;

	va_start (ap, msg);

	while (42) {
		type = va_arg (ap, xmms_ipc_msg_arg_type_t);
		switch (type){
			case XMMS_IPC_MSG_ARG_TYPE_UINT32:
				dest = va_arg (ap, guint32 *);
				if (!xmms_ipc_msg_get_uint32 (msg, dest)) {
					return FALSE;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_INT32:
				dest = va_arg (ap, gint32 *);
				if (!xmms_ipc_msg_get_int32 (msg, dest)) {
					return FALSE;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_FLOAT:
				dest = va_arg (ap, gfloat *);
				if (!xmms_ipc_msg_get_float (msg, dest)) {
					return FALSE;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_STRING:
				len = va_arg (ap, gint);
				dest = va_arg (ap, char *);
				if (!xmms_ipc_msg_get_string (msg, dest, len)) {
					return FALSE;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_DATA:
				len = va_arg (ap, gint);
				dest = va_arg (ap, void *);
				if (!xmms_ipc_msg_get_data (msg, dest, len)) {
					return FALSE;
				}
				break;
			case XMMS_IPC_MSG_ARG_TYPE_END:
				va_end (ap);
				return TRUE;
		}
		
	}
}
