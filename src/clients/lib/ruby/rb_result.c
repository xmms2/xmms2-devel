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
#include <xmms/xmms_object.h>

#include "xmmspriv/xmms_playlist.h"

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
 * Sets the block that's executed when _res_' is handled.
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
 * Restarts _res_. If _res_ is not restartable, an ResultError is raised,
 * else a new Result object is returned.
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

/*
 * call-seq:
 *  res.int -> integer
 *
 * Returns the integer from _res_.
 */
static VALUE c_int_get (VALUE self)
{
	RbResult *res = NULL;
	int id = 0;

	Data_Get_Struct (self, RbResult, res);

	if (!xmmsc_result_get_int (res->real, &id)) {
		rb_raise (eValueError, "cannot retrieve value");
		return Qnil;
	}

	return INT2FIX (id);
}

/*
 * call-seq:
 *  res.uint -> integer
 *
 * Returns the unsigned integer from _res_.
 */
static VALUE c_uint_get (VALUE self)
{
	RbResult *res = NULL;
	unsigned int id = 0;

	Data_Get_Struct (self, RbResult, res);

	if (!xmmsc_result_get_uint (res->real, &id)) {
		rb_raise (eValueError, "cannot retrieve value");
		return Qnil;
	}

	return UINT2NUM (id);
}

/*
 * call-seq:
 *  res.string -> string
 *
 * Returns the string from _res_.
 */
static VALUE c_string_get (VALUE self)
{
	RbResult *res = NULL;
	char *s = NULL;

	Data_Get_Struct (self, RbResult, res);

	if (!xmmsc_result_get_string (res->real, &s)) {
		rb_raise (eValueError, "cannot retrieve value");
		return Qnil;
	}

	return rb_str_new2 (s);
}

static void xhash_to_rhash (const void *key, const void *value,
                            VALUE *hash)
{
	VALUE val;

	if (!strcmp (key, "id"))
		val = rb_uint_new (strtoul (value, NULL, 10));
	else
		val = rb_str_new2 ((char *) value);

	rb_hash_aset (*hash, ID2SYM (rb_intern (key)), val);
}

/*
 * call-seq:
 *  res.hashtable -> hash
 *
 * Returns the hashtable from _res_.
 */
static VALUE c_hashtable_get (VALUE self)
{
	VALUE rhash = rb_hash_new ();
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	if (!xmmsc_result_dict_foreach (res->real,
	                                (xmmsc_foreach_func) xhash_to_rhash,
	                                &rhash))
		rb_raise (eValueError, "cannot retrieve value");

	return rhash;
}

/*
 * call-seq:
 *  res.intlist -> array
 *
 * Returns the intlist from _res_.
 */
static VALUE c_intlist_get (VALUE self)
{
	VALUE a;
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	a = rb_ary_new ();

	while (xmmsc_result_list_valid (res->real)) {
		int i = 0;

		if (!xmmsc_result_get_int (res->real, &i)) {
			rb_raise (eValueError, "cannot retrieve value");
			return Qnil;
		}

		rb_ary_push (a, INT2FIX (i));

		xmmsc_result_list_next (res->real);
	}

	return a;
}

/*
 * call-seq:
 *  res.uintlist -> array
 *
 * Returns the uintlist from _res_.
 */
static VALUE c_uintlist_get (VALUE self)
{
	VALUE a;
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	a = rb_ary_new ();

	while (xmmsc_result_list_valid (res->real)) {
		unsigned int i = 0;

		if (!xmmsc_result_get_uint (res->real, &i)) {
			rb_raise (eValueError, "cannot retrieve value");
			return Qnil;
		}

		rb_ary_push (a, UINT2NUM (i));

		xmmsc_result_list_next (res->real);
	}

	return a;
}

/*
 * call-seq:
 *  res.stringlist -> array
 *
 * Returns the stringlist from _res_.
 */
static VALUE c_stringlist_get (VALUE self)
{
	VALUE a;
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	a = rb_ary_new ();

	while (xmmsc_result_list_valid (res->real)) {
		char *s = NULL;

		if (!xmmsc_result_get_string (res->real, &s)) {
			rb_raise (eValueError, "cannot retrieve value");
			return Qnil;
		}

		rb_ary_push (a, rb_str_new2 (s));

		xmmsc_result_list_next (res->real);
	}

	return a;
}

/*
 * call-seq:
 *  res.hashlist -> array
 *
 * Returns the hashlist from _res_.
 */
static VALUE c_hashlist_get (VALUE self)
{
	VALUE a;
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	a = rb_ary_new ();

	while (xmmsc_result_list_valid (res->real)) {
		VALUE rhash = rb_hash_new ();

		if (!xmmsc_result_dict_foreach (res->real,
		                                (xmmsc_foreach_func) xhash_to_rhash,
		                                &rhash))
			rb_raise (eValueError, "cannot retrieve value");

		rb_ary_push (a, rhash);

		xmmsc_result_list_next (res->real);
	}

	return a;
}

static VALUE c_playlist_change_get (VALUE self)
{
	RbResult *res = NULL;
	unsigned int type = 0, id = 0, arg = 0;

	Data_Get_Struct (self, RbResult, res);

	if (!(xmmsc_result_get_playlist_change (res->real, &type, &id,
	                                        &arg))) {
		rb_raise (eValueError, "cannot retrieve value");
		return Qnil;
	}

	return rb_ary_new3 (3, UINT2NUM (type), UINT2NUM (id),
	                    UINT2NUM (arg));
}

/*
 * call-seq:
 *  res.value -> int or string or hash or array
 *
 * Returns the value from _res_.
 */
static VALUE c_value_get (VALUE self)
{
	VALUE ret = Qnil;
	RbResult *res;

	Data_Get_Struct (self, RbResult, res);

	if (xmmsc_result_iserror (res->real)) {
		rb_raise (eValueError, "cannot retrieve value");

		return Qnil;
	}

	switch (xmmsc_result_get_type (res->real)) {
		case XMMS_OBJECT_CMD_ARG_UINT32:
			ret = c_uint_get (self);
			break;
		case XMMS_OBJECT_CMD_ARG_INT32:
			ret = c_int_get (self);
			break;
		case XMMS_OBJECT_CMD_ARG_STRING:
			ret = c_string_get (self);
			break;
		case XMMS_OBJECT_CMD_ARG_UINT32LIST:
			ret = c_uintlist_get (self);
			break;
		case XMMS_OBJECT_CMD_ARG_INT32LIST:
			ret = c_intlist_get (self);
			break;
		case XMMS_OBJECT_CMD_ARG_STRINGLIST:
			ret = c_stringlist_get (self);
		case XMMS_OBJECT_CMD_ARG_DICT:
			ret = c_hashtable_get (self);
			break;
		case XMMS_OBJECT_CMD_ARG_DICTLIST:
			ret = c_hashlist_get (self);
			break;
		case XMMS_OBJECT_CMD_ARG_PLCH:
			ret = c_playlist_change_get (self);
			break;
		default:
			break;
	}

	return ret;
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
	rb_define_method (cResult, "int", c_int_get, 0);
	rb_define_method (cResult, "uint", c_uint_get, 0);
	rb_define_method (cResult, "string", c_string_get, 0);
	rb_define_method (cResult, "hashtable", c_hashtable_get, 0);
	rb_define_method (cResult, "intlist", c_intlist_get, 0);
	rb_define_method (cResult, "uintlist", c_uintlist_get, 0);
	rb_define_method (cResult, "stringlist", c_stringlist_get, 0);
	rb_define_method (cResult, "hashlist", c_hashlist_get, 0);
	rb_define_method (cResult, "playlist_change",
	                  c_playlist_change_get, 0);
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
