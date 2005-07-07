/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003, 2004 Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsc/xmmsc_idnumbers.h>
#include <xmms/xmms_object.h>

#include <ruby.h>
#include <stdbool.h>

#include "rb_xmmsclient.h"
#include "rb_result.h"

#define DEF_CONST(mod, prefix, name) \
	rb_define_const ((mod), #name, \
	                 INT2FIX (prefix##name));

typedef struct {
	xmmsc_result_t *real;
	VALUE children;
	VALUE callback;
	bool unref;
	bool unref_children;
} RbResult;

static VALUE cResult, eResultError, eValueError;

static void c_mark (RbResult *res)
{
	rb_gc_mark (res->children);

	if (!NIL_P (res->callback))
		rb_gc_mark (res->callback);
}

static void c_free (RbResult *res)
{
	if (res->real && res->unref)
		xmmsc_result_unref (res->real);

	free (res);
}

VALUE TO_XMMS_CLIENT_RESULT (xmmsc_result_t *res,
                             bool unref, bool unref_children)
{
	VALUE self;
	RbResult *rbres = NULL;

	if (!res)
		return Qnil;

	self = Data_Make_Struct (cResult, RbResult, c_mark, c_free, rbres);

	rbres->real = res;
	rbres->children = rb_ary_new ();
	rbres->callback = Qnil;
	rbres->unref = unref;
	rbres->unref_children = unref_children;

	rb_obj_call_init (self, 0, NULL);

	return self;
}

static void on_signal (xmmsc_result_t *res2, void *data)
{
	VALUE o, self = (VALUE) data;
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	o = TO_XMMS_CLIENT_RESULT (res2, res->unref_children,
	                           res->unref_children);
	rb_ary_push (res->children, o);

	rb_funcall (res->callback, rb_intern ("call"), 1, o);
}

/*
 * call-seq:
 *  res.notifier { |res2| }
 *
 * Sets the block that's executed when _res_ is handled.
 * Used by asyncronous results only.
 */
static VALUE c_notifier_set (VALUE self)
{
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	if (!rb_block_given_p ())
		return Qnil;

	res->callback = rb_block_proc ();

	xmmsc_result_notifier_set (res->real, on_signal, (void *) self);

	return Qnil;
}

/*
 * call-seq:
 *  res.wait -> self
 *
 * Waits for _res_ to be handled.
 */
static VALUE c_wait (VALUE self)
{
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	xmmsc_result_wait (res->real);

	return self;
}

/*
 * call-seq:
 *  res.restart -> res2 or nil
 *
 * Restarts _res_. If _res_ is not restartable, a +ResultError+ is
 * raised, else a new Result object is returned.
 */
static VALUE c_restart (VALUE self)
{
	VALUE o;
	xmmsc_result_t *res2;
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	if (!(res2 = xmmsc_result_restart (res->real))) {
		rb_raise (eResultError, "cannot restart result");
		return Qnil;
	}

	o = TO_XMMS_CLIENT_RESULT (res2, res->unref_children,
	                           res->unref_children);
	rb_ary_push (res->children, o);

	return o;
}

static VALUE c_disconnect_broadcast (VALUE self)
{
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	xmmsc_broadcast_disconnect (res->real);

	return self;
}

static VALUE c_disconnect_signal (VALUE self)
{
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	xmmsc_signal_disconnect (res->real);

	return self;
}

static VALUE int_get (RbResult *res)
{
	int id = 0;

	if (!xmmsc_result_get_int (res->real, &id)) {
		rb_raise (eValueError, "cannot retrieve value");
		return Qnil;
	}

	return INT2FIX (id);
}

static VALUE uint_get (RbResult *res)
{
	unsigned int id = 0;

	if (!xmmsc_result_get_uint (res->real, &id)) {
		rb_raise (eValueError, "cannot retrieve value");
		return Qnil;
	}

	return UINT2NUM (id);
}

static VALUE string_get (RbResult *res)
{
	char *s = NULL;

	if (!xmmsc_result_get_string (res->real, &s)) {
		rb_raise (eValueError, "cannot retrieve value");
		return Qnil;
	}

	return rb_str_new2 (s ? s : "");
}

static void xhash_to_rhash (const void *key,
                            xmmsc_result_value_type_t type,
                            const void *value, VALUE *hash)
{
	VALUE val;

	switch (type) {
		case XMMSC_RESULT_VALUE_TYPE_STRING:
			val = rb_str_new2 (value ? value : "");
			break;
		case XMMSC_RESULT_VALUE_TYPE_INT32:
			val = INT2NUM ((int32_t) value);
			break;
		case XMMSC_RESULT_VALUE_TYPE_UINT32:
			val = UINT2NUM ((uint32_t) value);
			break;
		default:
			break;
	}

	rb_hash_aset (*hash, ID2SYM (rb_intern (key)), val);
}

static VALUE hashtable_get (RbResult *res)
{
	VALUE rhash = rb_hash_new ();

	if (!xmmsc_result_dict_foreach (res->real,
	                                (xmmsc_foreach_func) xhash_to_rhash,
	                                &rhash))
		rb_raise (eValueError, "cannot retrieve value");

	return rhash;
}

static VALUE value_get (RbResult *res)
{
	VALUE ret;

	switch (xmmsc_result_get_type (res->real)) {
		case XMMS_OBJECT_CMD_ARG_UINT32:
			ret = uint_get (res);
			break;
		case XMMS_OBJECT_CMD_ARG_INT32:
			ret = int_get (res);
			break;
		case XMMS_OBJECT_CMD_ARG_STRING:
			ret = string_get (res);
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			ret = hashtable_get (res);
			break;
		/* don't check for XMMS_OBJECT_CMD_ARG_LIST here */
		default:
			ret = Qnil;
			break;
	}

	return ret;
}

static VALUE list_get (RbResult *res)
{
	VALUE ret;

	ret = rb_ary_new ();

	while (xmmsc_result_list_valid (res->real)) {
		rb_ary_push (ret, value_get (res));

		xmmsc_result_list_next (res->real);
	}

	return ret;
}

/*
 * call-seq:
 *  res.value -> int or string or hash or array
 *
 * Returns the value from _res_.
 */
static VALUE c_value_get (VALUE self)
{
	RbResult *res;

	Data_Get_Struct (self, RbResult, res);

	if (xmmsc_result_iserror (res->real)) {
		rb_raise (eValueError, "cannot retrieve value");

		return Qnil;
	}

	if (xmmsc_result_list_valid (res->real))
		return list_get (res);
	else
		return value_get (res);
}

void Init_Result (VALUE mXmmsClient, VALUE eXmmsClientError)
{
	cResult = rb_define_class_under (mXmmsClient, "Result", rb_cObject);

	/* ugh, we have to define the "new" method,
	 * so we can remove it again :(
	 */
	rb_define_singleton_method (cResult, "new", NULL, 0);
	rb_undef_method (rb_singleton_class (cResult), "new");

	rb_define_method (cResult, "notifier", c_notifier_set, 0);
	rb_define_method (cResult, "wait", c_wait, 0);
	rb_define_method (cResult, "restart", c_restart, 0);
	rb_define_method (cResult, "disconnect_broadcast",
	                  c_disconnect_broadcast, 0);
	rb_define_method (cResult, "disconnect_signal",
	                  c_disconnect_signal, 0);
	rb_define_method (cResult, "value", c_value_get, 0);

	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_ADD);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_SHUFFLE);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_REMOVE);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_CLEAR);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_MOVE);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_SORT);

	eResultError = rb_define_class_under (mXmmsClient, "ResultError",
	                                      eXmmsClientError);
	eValueError = rb_define_class_under (mXmmsClient, "ValueError",
	                                     eResultError);
}
