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

#include <ruby.h>
#include <xmmsc/xmmsc_stdbool.h>

#include "rb_xmmsclient.h"
#include "rb_result.h"

#define DEF_CONST(mod, prefix, name) \
	rb_define_const ((mod), #name, \
	                 INT2FIX (prefix##name));

typedef struct {
	xmmsc_result_t *real;
	xmmsc_result_t *orig;
	VALUE xmms;
	VALUE callback;
} RbResult;

static VALUE cResult, cBroadcastResult, cSignalResult,
             eResultError, eValueError;

static void c_mark (RbResult *res)
{
	rb_gc_mark (res->xmms);

	if (!NIL_P (res->callback))
		rb_gc_mark (res->callback);
}

static void c_free (RbResult *res)
{
	xmmsc_result_unref (res->real);

	free (res);
}

VALUE TO_XMMS_CLIENT_RESULT (VALUE xmms, xmmsc_result_t *res,
                             ResultType type)
{
	VALUE self, klass;
	RbResult *rbres = NULL;

	if (!res)
		return Qnil;

	switch (type) {
		case RESULT_TYPE_SIGNAL:
			klass = cSignalResult;
			break;
		case RESULT_TYPE_BROADCAST:
			klass = cBroadcastResult;
			break;
		default:
			klass = cResult;
			break;
	}

	self = Data_Make_Struct (klass, RbResult, c_mark, c_free, rbres);

	rbres->real = rbres->orig = res;
	rbres->xmms = xmms;
	rbres->callback = Qnil;

	rb_obj_call_init (self, 0, NULL);

	return self;
}

static void on_signal (xmmsc_result_t *res2, void *data)
{
	VALUE self = (VALUE) data;
	RbResult *res = NULL;
	RbXmmsClient *xmms = NULL;
	bool is_bc = (CLASS_OF (self) == cBroadcastResult);

	Data_Get_Struct (self, RbResult, res);

	/* if this result isn't restarted automatically, delete the
	 * reference. see c_sig_restart
	 */
	if (!is_bc) {
		Data_Get_Struct (res->xmms, RbXmmsClient, xmms);
		rb_ary_delete (xmms->results, self);
	}

	rb_funcall (res->callback, rb_intern ("call"), 1, self);

	if (!is_bc)
		xmmsc_result_unref (res2);
}

/*
 * call-seq:
 *  res.notifier { |res| }
 *
 * Sets the block that's executed when _res_ is handled.
 * Used by asyncronous results only.
 */
static VALUE c_notifier_set (VALUE self)
{
	RbResult *res = NULL;
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbResult, res);

	if (!rb_block_given_p ())
		return Qnil;

	res->callback = rb_block_proc ();

	Data_Get_Struct (res->xmms, RbXmmsClient, xmms);
	rb_ary_push (xmms->results, self);

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
 *  res.restart -> self
 *
 * Restarts _res_. If _res_ is not restartable, a +ResultError+ is
 * raised.
 */
static VALUE c_sig_restart (VALUE self)
{
	xmmsc_result_t *res2;
	RbResult *res = NULL;
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbResult, res);

	if (!(res2 = xmmsc_result_restart (res->real))) {
		rb_raise (eResultError, "cannot restart result");
		return Qnil;
	}

	res->real = res2;

	Data_Get_Struct (res->xmms, RbXmmsClient, xmms);
	rb_ary_push (xmms->results, self);

	xmmsc_result_unref (res->real);

	return self;
}

static VALUE c_bc_disconnect (VALUE self)
{
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	xmmsc_broadcast_disconnect (res->orig);

	return self;
}

static VALUE c_sig_disconnect (VALUE self)
{
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	xmmsc_signal_disconnect (res->orig);

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

static VALUE cast_result_value (xmmsc_result_value_type_t type,
                                const void *value)
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
			val = Qnil;
			break;
	}

	return val;
}

static void dict_to_hash (const void *key,
                          xmmsc_result_value_type_t type,
                          const void *value, void *udata)
{
	VALUE *h = udata;

	rb_hash_aset (*h, ID2SYM (rb_intern (key)),
	              cast_result_value (type, value));
}

static void propdict_to_hash (const void *key,
                              xmmsc_result_value_type_t type,
                              const void *value, const char *src,
                              void *udata)
{
	VALUE *h = udata, h2, rbsrc;

	rbsrc = rb_str_new2 (src);

	h2 = rb_hash_aref (*h, rbsrc);
	if (NIL_P (h2)) {
		h2 = rb_hash_new ();
		rb_hash_aset (*h, rbsrc, h2);
	}

	rb_hash_aset (h2, ID2SYM (rb_intern (key)),
	              cast_result_value (type, value));
}

static VALUE hashtable_get (RbResult *res)
{
	VALUE hash = rb_hash_new ();

	if (!xmmsc_result_dict_foreach (res->real,
	                                dict_to_hash, &hash))
		rb_raise (eValueError, "cannot retrieve value");

	return hash;
}

static VALUE propdict_get (RbResult *res)
{
	VALUE hash = rb_hash_new ();

	if (!xmmsc_result_propdict_foreach (res->real,
	                                    propdict_to_hash, &hash))
		rb_raise (eValueError, "cannot retrieve value");

	return hash;
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
		case XMMS_OBJECT_CMD_ARG_PROPDICT:
			ret = propdict_get (res);
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

	if (xmmsc_result_is_list (res->real))
		return list_get (res);
	else
		return value_get (res);
}

void Init_Result (VALUE mXmmsClient)
{
	cResult = rb_define_class_under (mXmmsClient, "Result", rb_cObject);

	/* ugh, we have to define the "new" method,
	 * so we can remove it again :(
	 */
	rb_define_singleton_method (cResult, "new", NULL, 0);
	rb_undef_method (rb_singleton_class (cResult), "new");

	rb_define_method (cResult, "notifier", c_notifier_set, 0);
	rb_define_method (cResult, "wait", c_wait, 0);
	rb_define_method (cResult, "value", c_value_get, 0);

	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_ADD);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_INSERT);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_SHUFFLE);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_REMOVE);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_CLEAR);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_MOVE);
	DEF_CONST (cResult, XMMS_, PLAYLIST_CHANGED_SORT);

	cBroadcastResult = rb_define_class_under (mXmmsClient,
	                                          "BroadcastResult",
	                                          cResult);
	rb_define_method (cBroadcastResult, "disconnect", c_bc_disconnect, 0);

	cSignalResult = rb_define_class_under (mXmmsClient, "SignalResult",
	                                       cResult);
	rb_define_method (cSignalResult, "restart", c_sig_restart, 0);
	rb_define_method (cSignalResult, "disconnect", c_sig_disconnect, 0);

	eResultError = rb_define_class_under (cResult, "ResultError",
	                                      rb_eStandardError);
	eValueError = rb_define_class_under (cResult, "ValueError",
	                                     eResultError);
}
