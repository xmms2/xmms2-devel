/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include <stdlib.h>
#include <assert.h>

#include <xmmscpriv/xmmsv.h>
#include <xmmscpriv/xmmsc_util.h>

static xmmsv_t *duplicate_dict_value (xmmsv_t *val);
static xmmsv_t *duplicate_list_value (xmmsv_t *val);
static xmmsv_t *duplicate_coll_value (xmmsv_t *val);

/**
 * Return a new value object which is a deep copy of the input value
 *
 * @param val #xmmsv_t to copy.
 * @return 1 the address to the new copy of the value.
 */
xmmsv_t *
xmmsv_copy (xmmsv_t *val)
{
	xmmsv_t *cur_val = NULL;
	xmmsv_type_t type;
	int32_t i;
	const char *s;

	x_return_val_if_fail (val, 0);
	type = xmmsv_get_type (val);
	switch (type) {
		case XMMSV_TYPE_DICT:
			cur_val = duplicate_dict_value (val);
			break;
		case XMMSV_TYPE_LIST:
			cur_val = duplicate_list_value (val);
			break;
		case XMMSV_TYPE_INT32:
			xmmsv_get_int (val, &i);
			cur_val = xmmsv_new_int (i);
			break;
		case XMMSV_TYPE_STRING:
			xmmsv_get_string (val, &s);
			cur_val = xmmsv_new_string (s);
			break;
		case XMMSV_TYPE_ERROR:
			xmmsv_get_error (val, &s);
			cur_val = xmmsv_new_error (s);
			break;
		case XMMSV_TYPE_COLL:
			cur_val = duplicate_coll_value (val);
			break;
		case XMMSV_TYPE_BIN:
			cur_val = xmmsv_new_bin (val->value.bin.data, val->value.bin.len);
			break;
		case XMMSV_TYPE_BITBUFFER:
			cur_val = xmmsv_new_bitbuffer ();
			xmmsv_bitbuffer_put_data (cur_val, val->value.bit.buf, val->value.bit.len / 8);
			xmmsv_bitbuffer_goto (cur_val, xmmsv_bitbuffer_pos (val));
			break;
		default:
			cur_val = xmmsv_new_none ();
			break;
	}
	assert (cur_val);
	return cur_val;
}

xmmsv_t *
duplicate_dict_value (xmmsv_t *val)
{
	xmmsv_t *dup_val;
	xmmsv_dict_iter_t *it;
	const char *key;
	xmmsv_t *v;
	xmmsv_t *new_elem;

	x_return_val_if_fail (xmmsv_get_dict_iter (val, &it), NULL);
	dup_val = xmmsv_new_dict ();
	while (xmmsv_dict_iter_valid (it)) {
		xmmsv_dict_iter_pair (it, &key, &v);
		new_elem = xmmsv_copy (v);
		xmmsv_dict_set (dup_val, key, new_elem);
		xmmsv_unref (new_elem);
		xmmsv_dict_iter_next (it);
	}

	xmmsv_dict_iter_explicit_destroy (it);

	return dup_val;
}

xmmsv_t *
duplicate_list_value (xmmsv_t *val)
{
	xmmsv_t *dup_val;
	xmmsv_list_iter_t *it;
	xmmsv_t *v;
	xmmsv_t *new_elem;

	x_return_val_if_fail (xmmsv_get_list_iter (val, &it), NULL);
	dup_val = xmmsv_new_list ();
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &v);
		new_elem = xmmsv_copy (v);
		xmmsv_list_append (dup_val, new_elem);
		xmmsv_unref (new_elem);
		xmmsv_list_iter_next (it);
	}

	xmmsv_list_iter_explicit_destroy (it);

	return dup_val;

}

xmmsv_coll_t *
xmmsv_coll_copy (xmmsv_coll_t *orig_coll)
{
	xmmsv_coll_t *new_coll, *coll_elem;
	xmmsv_list_iter_t *it;
	xmmsv_dict_iter_t *itd;
	xmmsv_t *v, *list, *dict, *copy;
	const char *key;
	int32_t i;
	const char *s;

	new_coll = xmmsv_coll_new (xmmsv_coll_get_type (orig_coll));

	list = xmmsv_coll_idlist_get (orig_coll);
	x_return_val_if_fail (xmmsv_get_list_iter (list, &it), NULL);
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &v);
		xmmsv_get_int (v, &i);
		xmmsv_coll_idlist_append (new_coll, i);
		xmmsv_list_iter_next (it);
	}
	xmmsv_list_iter_explicit_destroy (it);

	list = xmmsv_coll_operands_get (orig_coll);
	x_return_val_if_fail (xmmsv_get_list_iter (list, &it), NULL);
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &v);
		xmmsv_get_coll (v, &coll_elem);
		copy = xmmsv_coll_copy (coll_elem);
		xmmsv_coll_add_operand (new_coll, copy);
		xmmsv_unref (copy);
		xmmsv_list_iter_next (it);
	}
	xmmsv_list_iter_explicit_destroy (it);

	dict = xmmsv_coll_attributes_get (orig_coll);
	x_return_val_if_fail (xmmsv_get_dict_iter (dict, &itd), NULL);
	while (xmmsv_dict_iter_valid (itd)) {
		xmmsv_dict_iter_pair (itd, &key, &v);
		xmmsv_get_string (v, &s);
		xmmsv_coll_attribute_set (new_coll, key, s);
		xmmsv_dict_iter_next (itd);
	}
	xmmsv_dict_iter_explicit_destroy (itd);
	return new_coll;
}

static xmmsv_t *
duplicate_coll_value (xmmsv_t *val)
{
	x_return_val_if_fail (xmmsv_is_type (val, XMMSV_TYPE_COLL), NULL);
	return xmmsv_coll_copy (val);
}
