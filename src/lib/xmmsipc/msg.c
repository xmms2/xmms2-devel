/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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
#include <stdlib.h>

#include <errno.h>
#include <time.h>
#include <assert.h>

#include <xmmscpriv/xmms_list.h>
#include <xmmsc/xmmsc_ipc_transport.h>
#include <xmmsc/xmmsc_ipc_msg.h>
#include <xmmsc/xmmsc_util.h>
#include <xmmsc/xmmsc_sockets.h>
#include <xmmsc/xmmsc_stdint.h>
#include <xmmsc/xmmsv_coll.h>

struct xmms_ipc_msg_St {
	xmmsv_t *bb;
	uint32_t xfered;
};



xmms_ipc_msg_t *
xmms_ipc_msg_alloc (void)
{
	xmms_ipc_msg_t *msg;
	static unsigned char empty[16] = {0,};

	msg = x_new0 (xmms_ipc_msg_t, 1);
	msg->bb = xmmsv_new_bitbuffer ();
	xmmsv_bitbuffer_put_data (msg->bb, empty, 16);

	return msg;
}

void
xmms_ipc_msg_destroy (xmms_ipc_msg_t *msg)
{
	x_return_if_fail (msg);

	xmmsv_unref (msg->bb);
	free (msg);
}

static void
xmms_ipc_msg_update_length (xmmsv_t *bb)
{
	int len;

	len = xmmsv_bitbuffer_len (bb);

	len /= 8;
	len -= XMMS_IPC_MSG_HEAD_LEN;

	xmmsv_bitbuffer_goto (bb, 12*8);
	xmmsv_bitbuffer_put_bits (bb, 32, len);
	xmmsv_bitbuffer_end (bb);
}

static uint32_t
xmms_ipc_msg_get_length (const xmms_ipc_msg_t *msg)
{
	int64_t len;
	int32_t p;

	x_return_val_if_fail (msg, 0);

	p = xmmsv_bitbuffer_pos (msg->bb);
	xmmsv_bitbuffer_goto (msg->bb, 12*8);
	xmmsv_bitbuffer_get_bits (msg->bb, 32, &len);
	xmmsv_bitbuffer_goto (msg->bb, p);
	return len;
}

uint32_t
xmms_ipc_msg_get_object (const xmms_ipc_msg_t *msg)
{
	int64_t obj;
	int32_t p;

	x_return_val_if_fail (msg, 0);

	p = xmmsv_bitbuffer_pos (msg->bb);
	xmmsv_bitbuffer_goto (msg->bb, 0);
	xmmsv_bitbuffer_get_bits (msg->bb, 32, &obj);
	xmmsv_bitbuffer_goto (msg->bb, p);
	return obj;
}

static void
xmms_ipc_msg_set_object (xmms_ipc_msg_t *msg, uint32_t object)
{
	x_return_if_fail (msg);

	xmmsv_bitbuffer_goto (msg->bb, 0);
	xmmsv_bitbuffer_put_bits (msg->bb, 32, object);
	xmmsv_bitbuffer_end (msg->bb);
}

uint32_t
xmms_ipc_msg_get_cmd (const xmms_ipc_msg_t *msg)
{
	int64_t cmd;
	int32_t p;

	x_return_val_if_fail (msg, 0);

	p = xmmsv_bitbuffer_pos (msg->bb);
	xmmsv_bitbuffer_goto (msg->bb, 4 * 8);
	xmmsv_bitbuffer_get_bits (msg->bb, 32, &cmd);
	xmmsv_bitbuffer_goto (msg->bb, p);
	return cmd;
}

static void
xmms_ipc_msg_set_cmd (xmms_ipc_msg_t *msg, uint32_t cmd)
{
	x_return_if_fail (msg);

	xmmsv_bitbuffer_goto (msg->bb, 4 * 8);
	xmmsv_bitbuffer_put_bits (msg->bb, 32, cmd);
	xmmsv_bitbuffer_end (msg->bb);
}

void
xmms_ipc_msg_set_cookie (xmms_ipc_msg_t *msg, uint32_t cookie)
{
	xmmsv_bitbuffer_goto (msg->bb, 8 * 8);
	xmmsv_bitbuffer_put_bits (msg->bb, 32, cookie);
	xmmsv_bitbuffer_end (msg->bb);
}

uint32_t
xmms_ipc_msg_get_cookie (const xmms_ipc_msg_t *msg)
{
	int64_t cookie;
	int32_t p;

	x_return_val_if_fail (msg, 0);

	p = xmmsv_bitbuffer_pos (msg->bb);
	xmmsv_bitbuffer_goto (msg->bb, 8 * 8);
	xmmsv_bitbuffer_get_bits (msg->bb, 32, &cookie);
	xmmsv_bitbuffer_goto (msg->bb, p);
	return cookie;
}

xmms_ipc_msg_t *
xmms_ipc_msg_new (uint32_t object, uint32_t cmd)
{
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_alloc ();

	xmms_ipc_msg_set_cmd (msg, cmd);
	xmms_ipc_msg_set_object (msg, object);

	return msg;
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
xmms_ipc_msg_write_transport (xmms_ipc_msg_t *msg,
                              xmms_ipc_transport_t *transport,
                              bool *disconnected)
{
	char *buf;
	unsigned int ret, len;

	x_return_val_if_fail (msg, false);
	x_return_val_if_fail (transport, false);

	xmmsv_bitbuffer_align (msg->bb);

	len = xmmsv_bitbuffer_len (msg->bb) / 8;

	x_return_val_if_fail (len > msg->xfered, true);

	buf = (char *) (xmmsv_bitbuffer_buffer (msg->bb) + msg->xfered);
	ret = xmms_ipc_transport_write (transport, buf, len - msg->xfered);

	if (ret == SOCKET_ERROR) {
		if (xmms_socket_error_recoverable ()) {
			return false;
		}

		if (disconnected) {
			*disconnected = true;
		}

		return false;
	} else if (!ret) {
		if (disconnected) {
			*disconnected = true;
		}
	} else {
		msg->xfered += ret;
	}

	return (len == msg->xfered);
}

/**
 * Try to read message from transport into msg.
 *
 * @returns TRUE if message is fully read.
 */
bool
xmms_ipc_msg_read_transport (xmms_ipc_msg_t *msg,
                             xmms_ipc_transport_t *transport,
                             bool *disconnected)
{
	char buf[512];
	unsigned int ret, len, rlen;

	x_return_val_if_fail (msg, false);
	x_return_val_if_fail (transport, false);

	while (true) {
		len = XMMS_IPC_MSG_HEAD_LEN;

		if (msg->xfered >= XMMS_IPC_MSG_HEAD_LEN) {
			len += xmms_ipc_msg_get_length (msg);

			if (msg->xfered == len) {
				return true;
			}
		}

		x_return_val_if_fail (msg->xfered < len, false);

		rlen = len - msg->xfered;
		if (rlen > sizeof (buf))
			rlen = sizeof (buf);

		ret = xmms_ipc_transport_read (transport, buf, rlen);

		if (ret == SOCKET_ERROR) {
			if (xmms_socket_error_recoverable ()) {
				return false;
			}

			if (disconnected) {
				*disconnected = true;
			}

			return false;
		} else if (ret == 0) {
			if (disconnected) {
				*disconnected = true;
			}

			return false;
		} else {
			xmmsv_bitbuffer_goto (msg->bb, msg->xfered * 8);
			xmmsv_bitbuffer_put_data (msg->bb, (unsigned char *) buf, ret);
			msg->xfered += ret;
			xmmsv_bitbuffer_goto (msg->bb, XMMS_IPC_MSG_HEAD_LEN * 8);
		}
	}
}

uint32_t
xmms_ipc_msg_put_value (xmms_ipc_msg_t *msg, xmmsv_t *v)
{
	if (!xmmsv_bitbuffer_serialize_value (msg->bb, v))
		return false;
	xmms_ipc_msg_update_length (msg->bb);
	return xmmsv_bitbuffer_pos (msg->bb);
}


bool
xmms_ipc_msg_get_value (xmms_ipc_msg_t *msg, xmmsv_t **val)
{
	return xmmsv_bitbuffer_deserialize_value (msg->bb, val);
}
