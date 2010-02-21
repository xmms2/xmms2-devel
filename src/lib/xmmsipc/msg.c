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

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <time.h>
#include <assert.h>

#include "xmmspriv/xmms_list.h"
#include "xmmsc/xmmsc_ipc_transport.h"
#include "xmmsc/xmmsc_ipc_msg.h"
#include "xmmsc/xmmsc_util.h"
#include "xmmsc/xmmsc_sockets.h"
#include "xmmsc/xmmsc_stdint.h"
#include "xmmsc/xmmsv_coll.h"

struct xmms_ipc_msg_St {
	xmmsv_t *bb;
	uint32_t xfered;
};


static bool _internal_get_from_bb_error_alloc (xmmsv_t *bb, char **buf, unsigned int *len);
static bool _internal_get_from_bb_uint32 (xmmsv_t *bb, uint32_t *v);
static bool _internal_get_from_bb_int32 (xmmsv_t *bb, int32_t *v);
static bool _internal_get_from_bb_string_alloc (xmmsv_t *bb, char **buf, unsigned int *len);
static bool _internal_get_from_bb_collection_alloc (xmmsv_t *bb, xmmsv_coll_t **coll);
static bool _internal_get_from_bb_bin_alloc (xmmsv_t *bb, unsigned char **buf, unsigned int *len);

static bool _internal_get_from_bb_value_alloc (xmmsv_t *bb, xmmsv_t **val);
static bool _internal_get_from_bb_value_of_type_alloc (xmmsv_t *bb, xmmsv_type_t type, xmmsv_t **val);


xmms_ipc_msg_t *
xmms_ipc_msg_alloc (void)
{
	xmms_ipc_msg_t *msg;
	static unsigned char empty[16] = {0,};

	msg = x_new0 (xmms_ipc_msg_t, 1);
	msg->bb = xmmsv_bitbuffer_new ();
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
	int len, p;
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
	int obj, p;
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
	int cmd, p;
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
	int cookie, p;
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
			xmmsv_bitbuffer_put_data (msg->bb, buf, ret);
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


static bool
_internal_get_from_bb_data (xmmsv_t *bb, void *buf, unsigned int len)
{
	if (!bb)
		return false;

	return xmmsv_bitbuffer_get_data (bb, buf, len);
}

static bool
_internal_get_from_bb_error_alloc (xmmsv_t *bb, char **buf,
                              unsigned int *len)
{
	/* currently, an error is just a string, so reuse that */
	return _internal_get_from_bb_string_alloc (bb, buf, len);
}

static bool
_internal_get_from_bb_uint32 (xmmsv_t *bb, uint32_t *v)
{
	bool ret;

	ret = _internal_get_from_bb_data (bb, v, sizeof (*v));

	if (v) {
		*v = ntohl (*v);
	}

	return ret;
}

static bool
_internal_get_from_bb_int32 (xmmsv_t *bb, int32_t *v)
{
	bool ret;

	ret = _internal_get_from_bb_data (bb, v, sizeof (*v));

	if (v) {
		*v = ntohl (*v);
	}

	return ret;
}

static bool
_internal_get_from_bb_string_alloc (xmmsv_t *bb, char **buf,
                               unsigned int *len)
{
	char *str;
	unsigned int l;

	if (!_internal_get_from_bb_uint32 (bb, &l)) {
		return false;
	}

	str = x_malloc (l + 1);
	if (!str) {
		return false;
	}

	if (!_internal_get_from_bb_data (bb, str, l)) {
		free (str);
		return false;
	}

	str[l] = '\0';

	*buf = str;
	*len = l;

	return true;
}

static bool
_internal_get_from_bb_bin_alloc (xmmsv_t *bb,
                            unsigned char **buf,
                            unsigned int *len)
{
	unsigned char *b;
	unsigned int l;

	if (!_internal_get_from_bb_uint32 (bb, &l)) {
		return false;
	}

	b = x_malloc (l);
	if (!b) {
		return false;
	}

	if (!_internal_get_from_bb_data (bb, b, l)) {
		free (b);
		return false;
	}

	*buf = b;
	*len = l;

	return true;
}

static bool
_internal_get_from_bb_collection_alloc (xmmsv_t *bb, xmmsv_coll_t **coll)
{
	unsigned int i;
	unsigned int type;
	unsigned int n_items;
	int id;
	int32_t *idlist = NULL;
	char *key, *val;

	/* Get the type and create the collection */
	if (!_internal_get_from_bb_uint32 (bb, &type)) {
		return false;
	}

	*coll = xmmsv_coll_new (type);

	/* Get the list of attributes */
	if (!_internal_get_from_bb_uint32 (bb, &n_items)) {
		goto err;
	}

	for (i = 0; i < n_items; i++) {
		unsigned int len;
		if (!_internal_get_from_bb_string_alloc (bb, &key, &len)) {
			goto err;
		}
		if (!_internal_get_from_bb_string_alloc (bb, &val, &len)) {
			free (key);
			goto err;
		}

		xmmsv_coll_attribute_set (*coll, key, val);
		free (key);
		free (val);
	}

	/* Get the idlist */
	if (!_internal_get_from_bb_uint32 (bb, &n_items)) {
		goto err;
	}

	if (!(idlist = x_new (int32_t, n_items + 1))) {
		goto err;
	}

	for (i = 0; i < n_items; i++) {
		if (!_internal_get_from_bb_int32 (bb, &id)) {
			goto err;
		}

		idlist[i] = id;
	}

	idlist[i] = 0;
	xmmsv_coll_set_idlist (*coll, idlist);
	free (idlist);
	idlist = NULL;

	/* Get the operands */
	if (!_internal_get_from_bb_uint32 (bb, &n_items)) {
		goto err;
	}

	for (i = 0; i < n_items; i++) {
		xmmsv_coll_t *operand;
		xmmsv_type_t type;

		if (!_internal_get_from_bb_uint32 (bb, &type) ||
		    type != XMMSV_TYPE_COLL ||
		    !_internal_get_from_bb_collection_alloc (bb, &operand)) {
			goto err;
		}

		xmmsv_coll_add_operand (*coll, operand);
		xmmsv_coll_unref (operand);
	}

	return true;

err:
	if (idlist != NULL) {
		free (idlist);
	}

	xmmsv_coll_unref (*coll);

	return false;
}


static int
xmmsc_deserialize_dict (xmmsv_t *bb, xmmsv_t **val)
{
	xmmsv_t *dict;
	unsigned int len, ignore;
	char *key;

	dict = xmmsv_new_dict ();

	if (!_internal_get_from_bb_uint32 (bb, &len)) {
		goto err;
	}

	while (len--) {
		xmmsv_t *v;

		if (!_internal_get_from_bb_string_alloc (bb, &key, &ignore)) {
			goto err;
		}

		if (!_internal_get_from_bb_value_alloc (bb, &v)) {
			free (key);
			goto err;
		}

		xmmsv_dict_set (dict, key, v);
		free (key);
		xmmsv_unref (v);
	}

	*val = dict;

	return true;

err:
	x_internal_error ("Message from server did not parse correctly!");
	xmmsv_unref (dict);
	return false;
}

static int
xmmsc_deserialize_list (xmmsv_t *bb, xmmsv_t **val)
{
	xmmsv_t *list;
	unsigned int len;

    list = xmmsv_new_list ();

	if (!_internal_get_from_bb_uint32 (bb, &len)) {
		goto err;
	}

	while (len--) {
		xmmsv_t *v;
		if (_internal_get_from_bb_value_alloc (bb, &v)) {
			xmmsv_list_append (list, v);
		} else {
			goto err;
		}
		xmmsv_unref (v);
	}

	*val = list;

	return true;

err:
	x_internal_error ("Message from server did not parse correctly!");
	xmmsv_unref (list);
	return false;
}

static bool
_internal_get_from_bb_value_alloc (xmmsv_t *bb, xmmsv_t **val)
{
	int32_t type;

	if (!_internal_get_from_bb_int32 (bb, &type)) {
		return false;
	}

	return _internal_get_from_bb_value_of_type_alloc (bb, type, val);
}

static bool
_internal_get_from_bb_value_of_type_alloc (xmmsv_t *bb, xmmsv_type_t type,
                                      xmmsv_t **val)
{
	int32_t i;
	uint32_t len;
	char *s;
	xmmsv_coll_t *c;
	unsigned char *d;

	switch (type) {
		case XMMSV_TYPE_ERROR:
			if (!_internal_get_from_bb_error_alloc (bb, &s, &len)) {
				return false;
			}
			*val = xmmsv_new_error (s);
			free (s);
			break;
		case XMMSV_TYPE_INT32:
			if (!_internal_get_from_bb_int32 (bb, &i)) {
				return false;
			}
			*val = xmmsv_new_int (i);
			break;
		case XMMSV_TYPE_STRING:
			if (!_internal_get_from_bb_string_alloc (bb, &s, &len)) {
				return false;
			}
			*val = xmmsv_new_string (s);
			free (s);
			break;
		case XMMSV_TYPE_DICT:
			if (!xmmsc_deserialize_dict (bb, val)) {
				return false;
			}
			break;

		case XMMSV_TYPE_LIST :
			if (!xmmsc_deserialize_list (bb, val)) {
				return false;
			}
			break;

		case XMMSV_TYPE_COLL:
			if (!_internal_get_from_bb_collection_alloc (bb, &c)) {
				return false;
			}
			*val = xmmsv_new_coll (c);
			xmmsv_coll_unref (c);
			break;

		case XMMSV_TYPE_BIN:
			if (!_internal_get_from_bb_bin_alloc (bb, &d, &len)) {
				return false;
			}
			*val = xmmsv_new_bin (d, len);
			free (d);
			break;

		case XMMSV_TYPE_NONE:
			*val = xmmsv_new_none ();
			break;
		default:
			x_internal_error ("Got message of unknown type!");
			return false;
	}

	return true;
}

bool
xmms_ipc_msg_get_value (xmms_ipc_msg_t *msg, xmmsv_t **val)
{
	return _internal_get_from_bb_value_alloc (msg->bb, val);
}
