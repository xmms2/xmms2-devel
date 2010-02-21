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

#include "xmmsc/xmmsc_stdbool.h"
#include "xmmsc/xmmsc_util.h"
#include "xmmsc/xmmsv.h"
#include "xmmsclientpriv/xmmsclient_util.h"

static void _internal_store_on_bb_uint32 (xmmsv_t *bb, uint32_t offset, uint32_t v);
static bool _internal_put_on_bb_bin (xmmsv_t *bb, const unsigned char *data, unsigned int len);
static bool _internal_put_on_bb_error (xmmsv_t *bb, const char *errmsg);
static bool _internal_put_on_bb_int32 (xmmsv_t *bb, int32_t v);
static bool _internal_put_on_bb_string (xmmsv_t *bb, const char *str);
static bool _internal_put_on_bb_collection (xmmsv_t *bb, xmmsv_coll_t *coll);
static bool _internal_put_on_bb_value_list (xmmsv_t *bb, xmmsv_t *v);
static bool _internal_put_on_bb_value_dict (xmmsv_t *bb, xmmsv_t *v);


static void
_internal_put_on_bb_append_coll_attr (const char *key, xmmsv_t *value, void *userdata)
{
	xmmsv_t *bb = (xmmsv_t *)userdata;
	const char *s;
	int r;

	r = xmmsv_get_string (value, &s);
	x_return_if_fail (r);

	_internal_put_on_bb_string (bb, key);
	_internal_put_on_bb_string (bb, s);
}

static void
_internal_put_on_bb_count_coll_attr (const char *key, xmmsv_t *value, void *userdata)
{
	int *n = (int *)userdata;
	++(*n);
}

static bool
_internal_put_on_bb_bin (xmmsv_t *bb,
                          const unsigned char *data,
                          unsigned int len)
{
	if (!xmmsv_bitbuffer_put_bits (bb, 32, len))
		return false;

	return xmmsv_bitbuffer_put_data (bb, data, len);
}

static bool
_internal_put_on_bb_error (xmmsv_t *bb, const char *errmsg)
{
	if (!bb) {
		return -1;
	}

	if (!errmsg) {
		return xmmsv_bitbuffer_put_bits (bb, 32, 0);
	}

	if (!xmmsv_bitbuffer_put_bits (bb, 32, strlen (errmsg) + 1))
		return false;

	return xmmsv_bitbuffer_put_data (bb, errmsg, strlen (errmsg) + 1);
}

static void
_internal_store_on_bb_uint32 (xmmsv_t *bb,
                           uint32_t offset, uint32_t v)
{

	xmmsv_bitbuffer_goto (bb, offset);
	xmmsv_bitbuffer_put_bits (bb, 32, v);
	xmmsv_bitbuffer_end (bb);
}

static bool
_internal_put_on_bb_int32 (xmmsv_t *bb, int32_t v)
{
	return xmmsv_bitbuffer_put_bits (bb, 32, v);
}

static bool
_internal_put_on_bb_string (xmmsv_t *bb, const char *str)
{
	if (!bb) {
		return false;
	}

	if (!str) {
		return xmmsv_bitbuffer_put_bits (bb, 32, 0);
	}

	if (!xmmsv_bitbuffer_put_bits (bb, 32, strlen (str) + 1))
		return false;

	return xmmsv_bitbuffer_put_data (bb, str, strlen (str) + 1);
}

static bool
_internal_put_on_bb_collection (xmmsv_t *bb, xmmsv_coll_t *coll)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *v, *attrs;
	int n;
	uint32_t ret;
	int32_t entry;
	xmmsv_coll_t *op;

	if (!bb || !coll) {
		return false;
	}

	/* push type */
	if (!xmmsv_bitbuffer_put_bits (bb, 32, xmmsv_coll_get_type (coll)))
		return false;

	/* attribute counter and values */
	attrs = xmmsv_coll_attributes_get (coll);
	n = 0;

	xmmsv_dict_foreach (attrs, _internal_put_on_bb_count_coll_attr, &n);
	if (!xmmsv_bitbuffer_put_bits (bb, 32, n))
		return false;

	/* needs error checking! */
	xmmsv_dict_foreach (attrs, _internal_put_on_bb_append_coll_attr, bb);

	attrs = NULL; /* no unref needed. */

	/* idlist counter and content */
	xmmsv_bitbuffer_put_bits (bb, 32, xmmsv_coll_idlist_get_size (coll));

	xmmsv_get_list_iter (xmmsv_coll_idlist_get (coll), &it);
	for (xmmsv_list_iter_first (it);
	     xmmsv_list_iter_valid (it);
	     xmmsv_list_iter_next (it)) {

		if (!xmmsv_list_iter_entry_int (it, &entry)) {
			x_api_error ("Non integer in idlist", 0);
		}
		xmmsv_bitbuffer_put_bits (bb, 32, entry);
	}
	xmmsv_list_iter_explicit_destroy (it);

	/* operands counter and objects */
	n = 0;
	if (xmmsv_coll_get_type (coll) != XMMS_COLLECTION_TYPE_REFERENCE) {
		n = xmmsv_list_get_size (xmmsv_coll_operands_get (coll));
	}

	ret = xmmsv_bitbuffer_pos (bb);
	xmmsv_bitbuffer_put_bits (bb, 32, n);

	if (n > 0) {
		xmmsv_get_list_iter (xmmsv_coll_operands_get (coll), &it);

		while (xmmsv_list_iter_entry (it, &v)) {
			if (!xmmsv_get_coll (v, &op)) {
				x_api_error ("Non collection operand", 0);
			}

			_internal_put_on_bb_int32 (bb, XMMSV_TYPE_COLL);

			ret = _internal_put_on_bb_collection (bb, op);
			xmmsv_list_iter_next (it);
		}
	}

	return ret;
}

static bool
_internal_put_on_bb_value_list (xmmsv_t *bb, xmmsv_t *v)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *entry;
	uint32_t offset, count;
	bool ret = true;

	if (!xmmsv_get_list_iter (v, &it)) {
		return false;
	}

	/* store a dummy value, store the real count once it's known */
	offset = xmmsv_bitbuffer_pos (bb);
	xmmsv_bitbuffer_put_bits (bb, 32, 0);

	count = 0;
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &entry);
		ret = xmmsv_bitbuffer_serialize_value (bb, entry);
		xmmsv_list_iter_next (it);
		count++;
	}

	/* overwrite with real size */
	_internal_store_on_bb_uint32 (bb, offset, count);

	return ret;
}

static bool
_internal_put_on_bb_value_dict (xmmsv_t *bb, xmmsv_t *v)
{
	xmmsv_dict_iter_t *it;
	const char *key;
	xmmsv_t *entry;
	uint32_t ret, offset, count;

	if (!xmmsv_get_dict_iter (v, &it)) {
		return false;
	}

	/* store a dummy value, store the real count once it's known */
	offset = xmmsv_bitbuffer_pos (bb);
	xmmsv_bitbuffer_put_bits (bb, 32, 0);

	count = 0;
	while (xmmsv_dict_iter_valid (it)) {
		xmmsv_dict_iter_pair (it, &key, &entry);
		ret = _internal_put_on_bb_string (bb, key);
		ret = xmmsv_bitbuffer_serialize_value (bb, entry);
		xmmsv_dict_iter_next (it);
		count++;
	}

	/* overwrite with real size */
	_internal_store_on_bb_uint32 (bb, offset, count);

	return ret;
}

int
xmmsv_bitbuffer_serialize_value (xmmsv_t *bb, xmmsv_t *v)
{
	bool ret;
	int32_t i;
	const char *s;
	xmmsv_coll_t *c;
	const unsigned char *bc;
	unsigned int bl;
	xmmsv_type_t type;

	type = xmmsv_get_type (v);
	ret = _internal_put_on_bb_int32 (bb, type);
	if (!ret)
		return ret;

	switch (type) {
	case XMMSV_TYPE_ERROR:
		if (!xmmsv_get_error (v, &s)) {
			return false;
		}
		ret = _internal_put_on_bb_error (bb, s);
		break;
	case XMMSV_TYPE_INT32:
		if (!xmmsv_get_int (v, &i)) {
			return false;
		}
		ret = _internal_put_on_bb_int32 (bb, i);
		break;
	case XMMSV_TYPE_STRING:
		if (!xmmsv_get_string (v, &s)) {
			return false;
		}
		ret = _internal_put_on_bb_string (bb, s);
		break;
	case XMMSV_TYPE_COLL:
		if (!xmmsv_get_coll (v, &c)) {
			return false;
		}
		ret = _internal_put_on_bb_collection (bb, c);
		break;
	case XMMSV_TYPE_BIN:
		if (!xmmsv_get_bin (v, &bc, &bl)) {
			return false;
		}
		ret = _internal_put_on_bb_bin (bb, bc, bl);
		break;
	case XMMSV_TYPE_LIST:
		ret = _internal_put_on_bb_value_list (bb, v);
		break;
	case XMMSV_TYPE_DICT:
		ret = _internal_put_on_bb_value_dict (bb, v);
		break;

	case XMMSV_TYPE_NONE:
		break;
	default:
		x_internal_error ("Tried to serialize value of unsupported type");
		return false;
	}

	return ret;
}

xmmsv_t *
xmmsv_serialize (xmmsv_t *v)
{
	xmmsv_t *bb;

	if (!v)
		return NULL;

	bb = xmmsv_bitbuffer_new ();

	if (!xmmsv_bitbuffer_serialize_value (bb, v)) {
		xmmsv_unref (bb);
		return NULL;
	}

	/* this is internally in xmmsv implementation,
	   so we could just switch the type,
	   but thats for later */
	return xmmsv_new_bin (xmmsv_bitbuffer_buffer (bb), xmmsv_bitbuffer_len (bb) / 8);
}
