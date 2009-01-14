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


typedef union {
	struct {
		uint32_t object;
		uint32_t cmd;
		uint32_t cookie;
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


void
xmms_ipc_append_coll_attr (const char* key, const char* value, void *userdata) {
	xmms_ipc_msg_t *msg = (xmms_ipc_msg_t *)userdata;
	xmms_ipc_msg_put_string (msg, key);
	xmms_ipc_msg_put_string (msg, value);
}

void
xmms_ipc_count_coll_attr (const char* key, const char* value, void *userdata) {
	int *n = (int *)userdata;
	++(*n);
}


xmms_ipc_msg_t *
xmms_ipc_msg_alloc (void)
{
	xmms_ipc_msg_t *msg;

	msg = x_new0 (xmms_ipc_msg_t, 1);
	msg->data = malloc (XMMS_IPC_MSG_DEFAULT_SIZE);
	memset (msg->data, 0, XMMS_IPC_MSG_HEAD_LEN);
	msg->size = XMMS_IPC_MSG_DEFAULT_SIZE;

	return msg;
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
xmms_ipc_msg_set_cookie (xmms_ipc_msg_t *msg, uint32_t cookie)
{
	msg->data->header.cookie = htonl (cookie);
}

uint32_t
xmms_ipc_msg_get_cookie (const xmms_ipc_msg_t *msg)
{
	x_return_val_if_fail (msg, 0);

	return ntohl (msg->data->header.cookie);
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
	x_return_val_if_fail (msg->data, false);
	x_return_val_if_fail (transport, false);

	len = xmms_ipc_msg_get_length (msg) + XMMS_IPC_MSG_HEAD_LEN;

	x_return_val_if_fail (len > msg->xfered, true);

	buf = (char *) (msg->data->rawdata + msg->xfered);
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
	char *buf;
	unsigned int ret, len;

	x_return_val_if_fail (msg, false);
	x_return_val_if_fail (transport, false);

	while (true) {
		len = XMMS_IPC_MSG_HEAD_LEN;

		if (msg->xfered >= XMMS_IPC_MSG_HEAD_LEN) {
			len += xmms_ipc_msg_get_length (msg);

			if (len > msg->size) {
				void *newbuf;
				newbuf = realloc (msg->data, len);
				if (!newbuf) {
					if (disconnected) {
						*disconnected = true;
					}
					return false;
				}
				msg->size = len;
				msg->data = newbuf;
			}

			if (msg->xfered == len) {
				return true;
			}
		}

		x_return_val_if_fail (msg->xfered < len, false);

		buf = (char *) (msg->data->rawdata + msg->xfered);
		ret = xmms_ipc_transport_read (transport, buf, len - msg->xfered);

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
			msg->xfered += ret;
		}
	}
}

static uint32_t
xmms_ipc_msg_put_data (xmms_ipc_msg_t *msg, const void *data, unsigned int len)
{
	uint32_t total;

	x_return_val_if_fail (msg, -1);

	total = xmms_ipc_msg_get_length (msg) + XMMS_IPC_MSG_HEAD_LEN + len;

	if (total > msg->size) {
		int realloc_size = XMMS_IPC_MSG_DEFAULT_SIZE;

		if (len > XMMS_IPC_MSG_DEFAULT_SIZE) {
			realloc_size = len;
		}

		/* Realloc data portion */
		msg->data = realloc (msg->data, msg->size + realloc_size);
		msg->size += realloc_size;
	}

	total = xmms_ipc_msg_get_length (msg);
	memcpy (&msg->data->header.data[total], data, len);
	xmms_ipc_msg_set_length (msg, total + len);

	/* return the offset that which we placed this value.
	 * If this logic is changed, make sure to update
	 * the return value of xmms_ipc_msg_put_value_data for
	 * NONE xmmsv's, too.
	 */
	return total;
}

uint32_t
xmms_ipc_msg_put_bin (xmms_ipc_msg_t *msg,
                      const unsigned char *data,
                      unsigned int len)
{
	xmms_ipc_msg_put_uint32 (msg, len);
	return xmms_ipc_msg_put_data (msg, data, len);
}

uint32_t
xmms_ipc_msg_put_error (xmms_ipc_msg_t *msg, const char *errmsg)
{
	if (!msg) {
		return -1;
	}

	if (!errmsg) {
		return xmms_ipc_msg_put_uint32 (msg, 0);
	}

	xmms_ipc_msg_put_uint32 (msg, strlen (errmsg) + 1);

	return xmms_ipc_msg_put_data (msg, errmsg, strlen (errmsg) + 1);
}

uint32_t
xmms_ipc_msg_put_uint32 (xmms_ipc_msg_t *msg, uint32_t v)
{
	v = htonl (v);

	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

void
xmms_ipc_msg_store_uint32 (xmms_ipc_msg_t *msg,
                           uint32_t offset, uint32_t v)
{
	v = htonl (v);

	memcpy (&msg->data->header.data[offset], &v, sizeof (v));
}

uint32_t
xmms_ipc_msg_put_int32 (xmms_ipc_msg_t *msg, int32_t v)
{
	v = htonl (v);

	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

uint32_t
xmms_ipc_msg_put_float (xmms_ipc_msg_t *msg, float v)
{
	/** @todo do we need to convert ? */
	return xmms_ipc_msg_put_data (msg, &v, sizeof (v));
}

uint32_t
xmms_ipc_msg_put_string (xmms_ipc_msg_t *msg, const char *str)
{
	if (!msg) {
		return -1;
	}

	if (!str) {
		return xmms_ipc_msg_put_uint32 (msg, 0);
	}

	xmms_ipc_msg_put_uint32 (msg, strlen (str) + 1);

	return xmms_ipc_msg_put_data (msg, str, strlen (str) + 1);
}

uint32_t
xmms_ipc_msg_put_collection (xmms_ipc_msg_t *msg, xmmsv_coll_t *coll)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *v;
	int n;
	uint32_t ret, *idlist;
	xmmsv_coll_t *op;

	if (!msg || !coll) {
		return -1;
	}

	/* push type */
	xmms_ipc_msg_put_uint32 (msg, xmmsv_coll_get_type (coll));

	/* attribute counter and values */
	n = 0;
	xmmsv_coll_attribute_foreach (coll, xmms_ipc_count_coll_attr, &n);
	xmms_ipc_msg_put_uint32 (msg, n);

	xmmsv_coll_attribute_foreach (coll, xmms_ipc_append_coll_attr, msg);

	/* idlist counter and content */
	idlist = xmmsv_coll_get_idlist (coll);
	for (n = 0; idlist[n] != 0; n++) { }

	xmms_ipc_msg_put_uint32 (msg, n);
	for (n = 0; idlist[n] != 0; n++) {
		xmms_ipc_msg_put_uint32 (msg, idlist[n]);
	}

	/* operands counter and objects */
	n = 0;
	if (xmmsv_coll_get_type (coll) != XMMS_COLLECTION_TYPE_REFERENCE) {
		n = xmmsv_list_get_size (xmmsv_coll_operands_list_get (coll));
	}

	ret = xmms_ipc_msg_put_uint32 (msg, n);

	if (n > 0) {
		xmmsv_get_list_iter (xmmsv_coll_operands_list_get (coll), &it);

		while (xmmsv_list_iter_entry (it, &v)) {
			if (!xmmsv_get_collection (v, &op)) {
				x_api_error ("Non collection operand", 0);
			}
			ret = xmms_ipc_msg_put_collection (msg, op);
			xmmsv_list_iter_next (it);
		}
	}

	return ret;
}

uint32_t
xmms_ipc_msg_put_value (xmms_ipc_msg_t *msg, xmmsv_t *v)
{
	xmmsv_type_t type;

	type = xmmsv_get_type (v);
	xmms_ipc_msg_put_int32 (msg, type);

	return xmms_ipc_msg_put_value_data (msg, v);
}

uint32_t
xmms_ipc_msg_put_value_data (xmms_ipc_msg_t *msg, xmmsv_t *v)
{
	uint32_t ret;
	uint32_t u;
	int32_t i;
	const char *s;
	xmmsv_coll_t *c;
	const unsigned char *bc;
	unsigned int bl;
	xmmsv_type_t type;

	type = xmmsv_get_type (v);

	/* FIXME: what to do if value fetching fails? */
	/* FIXME: return -1 unsigned int?? */

	switch (type) {
	case XMMSV_TYPE_ERROR:
		if (!xmmsv_get_error (v, &s)) {
			return -1;
		}
		ret = xmms_ipc_msg_put_error (msg, s);
		break;
	case XMMSV_TYPE_UINT32:
		if (!xmmsv_get_uint (v, &u)) {
			return -1;
		}
		ret = xmms_ipc_msg_put_uint32 (msg, u);
		break;
	case XMMSV_TYPE_INT32:
		if (!xmmsv_get_int (v, &i)) {
			return -1;
		}
		ret = xmms_ipc_msg_put_int32 (msg, i);
		break;
	case XMMSV_TYPE_STRING:
		if (!xmmsv_get_string (v, &s)) {
			return -1;
		}
		ret = xmms_ipc_msg_put_string (msg, s);
		break;
	case XMMSV_TYPE_COLL:
		if (!xmmsv_get_collection (v, &c)) {
			return -1;
		}
		ret = xmms_ipc_msg_put_collection (msg, c);
		break;
	case XMMSV_TYPE_BIN:
		if (!xmmsv_get_bin (v, &bc, &bl)) {
			return -1;
		}
		ret = xmms_ipc_msg_put_bin (msg, bc, bl);
		break;
	case XMMSV_TYPE_LIST:
		ret = xmms_ipc_msg_put_value_list (msg, v);
		break;
	case XMMSV_TYPE_DICT:
		ret = xmms_ipc_msg_put_value_dict (msg, v);
		break;

	case XMMSV_TYPE_NONE:
		/* just like the other _put_* functions, we
		 * return the offset that which we placed this value.
		 * See xmms_ipc_msg_put_data().
		 */
		ret = xmms_ipc_msg_get_length (msg);
		break;
	default:
		/* FIXME: weird, no? dump error? */
		return -1;
		break;
	}

	return ret;
}

uint32_t
xmms_ipc_msg_put_value_list (xmms_ipc_msg_t *msg, xmmsv_t *v)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *entry;
	uint32_t ret, offset, count;

	if (!xmmsv_get_list_iter (v, &it)) {
		return -1;
	}

	/* store a dummy value, store the real count once it's known */
	offset = xmms_ipc_msg_put_uint32 (msg, 0);

	count = 0;
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &entry);
		ret = xmms_ipc_msg_put_value (msg, entry);
		xmmsv_list_iter_next (it);
		count++;
	}

	/* overwrite with real size */
	xmms_ipc_msg_store_uint32 (msg, offset, count);

	return ret;
}

uint32_t
xmms_ipc_msg_put_value_dict (xmms_ipc_msg_t *msg, xmmsv_t *v)
{
	xmmsv_dict_iter_t *it;
	const char *key;
	xmmsv_t *entry;
	uint32_t ret, offset, count;

	if (!xmmsv_get_dict_iter (v, &it)) {
		return -1;
	}

	/* store a dummy value, store the real count once it's known */
	offset = xmms_ipc_msg_put_uint32 (msg, 0);

	count = 0;
	while (xmmsv_dict_iter_valid (it)) {
		xmmsv_dict_iter_pair (it, &key, &entry);
		ret = xmms_ipc_msg_put_string (msg, key);
		ret = xmms_ipc_msg_put_value (msg, entry);
		xmmsv_dict_iter_next (it);
		count++;
	}

	/* overwrite with real size */
	xmms_ipc_msg_store_uint32 (msg, offset, count);

	return ret;
}


static bool
xmms_ipc_msg_get_data (xmms_ipc_msg_t *msg, void *buf, unsigned int len)
{
	if (!msg)
		return false;

	if (len > xmms_ipc_msg_get_length (msg) - msg->get_pos)
		return false;

	if (buf) {
		memcpy (buf, &msg->data->header.data[msg->get_pos], len);
	}

	msg->get_pos += len;

	return true;
}

bool
xmms_ipc_msg_get_error_alloc (xmms_ipc_msg_t *msg, char **buf,
                              unsigned int *len)
{
	/* currently, an error is just a string, so reuse that */
	return xmms_ipc_msg_get_string_alloc (msg, buf, len);
}

bool
xmms_ipc_msg_get_uint32 (xmms_ipc_msg_t *msg, uint32_t *v)
{
	bool ret;

	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));

	if (v) {
		*v = ntohl (*v);
	}

	return ret;
}

bool
xmms_ipc_msg_get_int32 (xmms_ipc_msg_t *msg, int32_t *v)
{
	bool ret;

	ret = xmms_ipc_msg_get_data (msg, v, sizeof (*v));

	if (v) {
		*v = ntohl (*v);
	}

	return ret;
}

bool
xmms_ipc_msg_get_float (xmms_ipc_msg_t *msg, float *v)
{
	/** @todo do we need to convert? */
	return xmms_ipc_msg_get_data (msg, v, sizeof (*v));
}

bool
xmms_ipc_msg_get_string_alloc (xmms_ipc_msg_t *msg, char **buf,
                               unsigned int *len)
{
	char *str;
	unsigned int l;

	if (!xmms_ipc_msg_get_uint32 (msg, &l)) {
		return false;
	}

	if (l > xmms_ipc_msg_get_length (msg) - msg->get_pos)
		return false;

	str = x_malloc (l + 1);
	if (!str) {
		return false;
	}

	if (!xmms_ipc_msg_get_data (msg, str, l)) {
		free (str);
		return false;
	}

	str[l] = '\0';

	*buf = str;
	*len = l;

	return true;
}

bool
xmms_ipc_msg_get_bin_alloc (xmms_ipc_msg_t *msg,
                            unsigned char **buf,
                            unsigned int *len)
{
	unsigned char *b;
	unsigned int l;

	if (!xmms_ipc_msg_get_uint32 (msg, &l)) {
		return false;
	}

	if (l > xmms_ipc_msg_get_length (msg) - msg->get_pos)
		return false;

	b = x_malloc (l);
	if (!b) {
		return false;
	}

	if (!xmms_ipc_msg_get_data (msg, b, l)) {
		free (b);
		return false;
	}

	*buf = b;
	*len = l;

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

	if (!xmms_ipc_msg_get_uint32 (msg, &len)) {
		return false;
	}

	if (!len) {
		buf[0] = '\0';
		return true;
	}

	if (!xmms_ipc_msg_get_data (msg, buf, MIN (maxlen, len))) {
		return false;
	}

	if (maxlen < len) {
		xmms_ipc_msg_get_data (msg, NULL, len - maxlen);
	}

	return true;
}

bool
xmms_ipc_msg_get_collection_alloc (xmms_ipc_msg_t *msg, xmmsv_coll_t **coll)
{
	unsigned int i;
	unsigned int type;
	unsigned int n_items;
	unsigned int id;
	uint32_t *idlist = NULL;
	char *key, *val;

	/* Get the type and create the collection */
	if (!xmms_ipc_msg_get_uint32 (msg, &type)) {
		return false;
	}

	*coll = xmmsv_coll_new (type);

	/* Get the list of attributes */
	if (!xmms_ipc_msg_get_uint32 (msg, &n_items)) {
		goto err;
	}

	for (i = 0; i < n_items; i++) {
		unsigned int len;
		if (!xmms_ipc_msg_get_string_alloc (msg, &key, &len)) {
			goto err;
		}
		if (!xmms_ipc_msg_get_string_alloc (msg, &val, &len)) {
			free (key);
			goto err;
		}

		xmmsv_coll_attribute_set (*coll, key, val);
		free (key);
		free (val);
	}

	/* Get the idlist */
	if (!xmms_ipc_msg_get_uint32 (msg, &n_items)) {
		goto err;
	}

	if (!(idlist = x_new (uint32_t, n_items + 1))) {
		goto err;
	}

	for (i = 0; i < n_items; i++) {
		if (!xmms_ipc_msg_get_uint32 (msg, &id)) {
			goto err;
		}

		idlist[i] = id;
	}

	idlist[i] = 0;
	xmmsv_coll_set_idlist (*coll, idlist);
	free (idlist);
	idlist = NULL;

	/* Get the operands */
	if (!xmms_ipc_msg_get_uint32 (msg, &n_items)) {
		goto err;
	}

	for (i = 0; i < n_items; i++) {
		xmmsv_coll_t *operand;

		if (!xmms_ipc_msg_get_collection_alloc (msg, &operand)) {
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
xmmsc_deserialize_dict (xmms_ipc_msg_t *msg, xmmsv_t **val)
{
	xmmsv_t *dict;
	unsigned int len, ignore;
	char *key;

	dict = xmmsv_new_dict ();

	if (!xmms_ipc_msg_get_uint32 (msg, &len)) {
		goto err;
	}

	while (len--) {
		xmmsv_t *v;

		if (!xmms_ipc_msg_get_string_alloc (msg, &key, &ignore)) {
			goto err;
		}

		if (!xmms_ipc_msg_get_value_alloc (msg, &v)) {
			goto err;
		}

		xmmsv_dict_insert (dict, key, v);
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
xmmsc_deserialize_list (xmms_ipc_msg_t *msg, xmmsv_t **val)
{
	xmmsv_t *list;
	unsigned int len;

    list = xmmsv_new_list ();

	if (!xmms_ipc_msg_get_uint32 (msg, &len)) {
		goto err;
	}

	while (len--) {
		xmmsv_t *v;
		if (xmms_ipc_msg_get_value_alloc (msg, &v)) {
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


bool
xmms_ipc_msg_get_value_alloc (xmms_ipc_msg_t *msg, xmmsv_t **val)
{
	int32_t type;

	if (!xmms_ipc_msg_get_int32 (msg, (int32_t *) &type)) {
		return false;
	}

	return xmms_ipc_msg_get_value_of_type_alloc (msg, type, val);
}

bool
xmms_ipc_msg_get_value_of_type_alloc (xmms_ipc_msg_t *msg, xmmsv_type_t type,
                                      xmmsv_t **val)
{
	int32_t i;
	uint32_t len, u;
	char *s;
	xmmsv_coll_t *c;
	unsigned char *d;

	switch (type) {
		case XMMSV_TYPE_ERROR:
			if (!xmms_ipc_msg_get_error_alloc (msg, &s, &len)) {
				return false;
			}
			*val = xmmsv_new_error (s);
			free (s);
			break;
		case XMMSV_TYPE_UINT32:
			if (!xmms_ipc_msg_get_uint32 (msg, &u)) {
				return false;
			}
			*val = xmmsv_new_uint (u);
			break;
		case XMMSV_TYPE_INT32:
			if (!xmms_ipc_msg_get_int32 (msg, &i)) {
				return false;
			}
			*val = xmmsv_new_int (i);
			break;
		case XMMSV_TYPE_STRING:
			if (!xmms_ipc_msg_get_string_alloc (msg, &s, &len)) {
				return false;
			}
			*val = xmmsv_new_string (s);
			free (s);
			break;
		case XMMSV_TYPE_DICT:
			if (!xmmsc_deserialize_dict (msg, val)) {
				return false;
			}
			break;

		case XMMSV_TYPE_LIST :
			if (!xmmsc_deserialize_list (msg, val)) {
				return false;
			}
			break;

		case XMMSV_TYPE_COLL:
			xmms_ipc_msg_get_collection_alloc (msg, &c);
			if (!c) {
				return false;
			}
			*val = xmmsv_new_coll (c);
			xmmsv_coll_unref (c);
			break;

		case XMMSV_TYPE_BIN:
			if (!xmms_ipc_msg_get_bin_alloc (msg, &d, &len)) {
				return false;
			}
			*val = xmmsv_new_bin (d, len);
			free (d);
			break;

		case XMMSV_TYPE_NONE:
			*val = xmmsv_new_none ();
			break;
		default:
			return false;
			break;
	}

	return true;
}
