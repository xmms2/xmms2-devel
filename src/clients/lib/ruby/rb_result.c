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

#include <xmms/xmmsclient.h>

#include <ruby.h>
#include <stdbool.h>

#include "rb_xmmsclient_main.h"
#include "rb_result.h"

typedef struct {
	xmmsc_result_t *real;
	VALUE parent;
	VALUE callback;
	bool unref_on_free;
} RbResult;

static VALUE cResult;

static void c_mark (RbResult *res)
{
	if (!NIL_P (res->parent))
		rb_gc_mark (res->parent);

	if (!NIL_P (res->callback))
		rb_gc_mark (res->callback);
}

static void c_free (RbResult *res)
{
	if (res->real && res->unref_on_free)
		xmmsc_result_unref (res->real);

	free (res);
}

VALUE TO_XMMS_CLIENT_RESULT (VALUE parent, xmmsc_result_t *res,
                             bool unref_on_free)
{
	VALUE self;
	RbResult *rbres = NULL;

	if (!res)
		return Qnil;

	self = Data_Make_Struct (cResult, RbResult, c_mark, c_free, rbres);

	rbres->real = res;
	rbres->parent = parent;
	rbres->callback = Qnil;
	rbres->unref_on_free = unref_on_free;

	rb_obj_call_init (self, 0, NULL);

	return self;
}

static void on_signal (xmmsc_result_t *res2, void *data)
{
	VALUE self = (VALUE) data;

	GET_OBJ (self, RbResult, res);

	rb_funcall (res->callback, rb_intern ("call"), 1,
	            TO_XMMS_CLIENT_RESULT (self, res2, res->unref_on_free));
}

static VALUE c_notifier_set (VALUE self)
{
	GET_OBJ (self, RbResult, res);

	if (!rb_block_given_p ())
		return Qnil;

	res->callback = rb_block_proc ();

	xmmsc_result_notifier_set (res->real, on_signal, (void *) self);

	return Qnil;
}

static VALUE c_wait (VALUE self)
{
	GET_OBJ (self, RbResult, res);

	xmmsc_result_wait (res->real);

	return self;
}

static VALUE c_restart (VALUE self)
{
	xmmsc_result_t *res2;

	GET_OBJ (self, RbResult, res);

	if ((res2 = xmmsc_result_restart (res->real)))
		xmmsc_result_unref (res2);

	return self;
}

static VALUE c_int_get (VALUE self)
{
	int id = 0;

	GET_OBJ (self, RbResult, res);

	if (!xmmsc_result_get_int (res->real, &id)) {
		rb_raise (rb_eRuntimeError, "xmmsc_result_get_int() failed");
		return Qnil;
	}

	return INT2FIX (id);
}

static VALUE c_uint_get (VALUE self)
{
	unsigned int id = 0;

	GET_OBJ (self, RbResult, res);

	if (!xmmsc_result_get_uint (res->real, &id)) {
		rb_raise (rb_eRuntimeError, "xmmsc_result_get_uint() failed");
		return Qnil;
	}

	return UINT2NUM (id);
}

static VALUE c_string_get (VALUE self)
{
	char *s = NULL;

	GET_OBJ (self, RbResult, res);

	if (!xmmsc_result_get_string (res->real, &s)) {
		rb_raise (rb_eRuntimeError, "xmmsc_result_get_string() failed");
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

	rb_hash_aset (*hash, rb_str_new2 (key), val);
}

static VALUE c_hashtable_get (VALUE self)
{
	VALUE rhash = rb_hash_new ();
	x_hash_t *hash = NULL;

	GET_OBJ (self, RbResult, res);

	if (!xmmsc_result_get_hashtable (res->real, &hash))
		rb_raise (rb_eRuntimeError,
		          "xmmsc_result_get_hashtable() failed");

	x_hash_foreach (hash, (XHFunc) xhash_to_rhash, &rhash);

	return rhash;
}

static VALUE c_intlist_get (VALUE self)
{
	VALUE a;
	x_list_t *list = NULL, *l;

	GET_OBJ (self, RbResult, res);

	if (!xmmsc_result_get_intlist (res->real, &list)) {
		rb_raise (rb_eRuntimeError, "xmmsc_result_get_int_list() failed");
		return Qnil;
	}

	a = rb_ary_new ();

	for (l = list; l; l = l->next)
		rb_ary_push (a, INT2FIX ((int) l->data));

	return a;
}

static VALUE c_uintlist_get (VALUE self)
{
	VALUE a;
	x_list_t *list = NULL, *l;

	GET_OBJ (self, RbResult, res);

	if (!xmmsc_result_get_uintlist (res->real, &list)) {
		rb_raise (rb_eRuntimeError, "xmmsc_result_get_uint_list() failed");
		return Qnil;
	}

	a = rb_ary_new ();

	for (l = list; l; l = l->next)
		rb_ary_push (a, UINT2NUM ((unsigned int) l->data));

	return a;
}

static VALUE c_stringlist_get (VALUE self)
{
	VALUE a;
	x_list_t *list = NULL, *l;

	GET_OBJ (self, RbResult, res);

	if (!xmmsc_result_get_stringlist (res->real, &list)) {
		rb_raise (rb_eRuntimeError, "xmmsc_result_get_string_list() failed");
		return Qnil;
	}

	a = rb_ary_new ();

	for (l = list; l; l = l->next)
		rb_ary_push (a, rb_str_new2 (l->data));

	return a;
}

static VALUE c_playlist_change_get (VALUE self)
{
	unsigned int type = 0, id = 0, arg = 0;

	GET_OBJ (self, RbResult, res);

	if (!(xmmsc_result_get_playlist_change (res->real, &type, &id,
	                                        &arg))) {
		rb_raise (rb_eRuntimeError,
		          "xmmsc_result_get_playlist_change() failed");
		return Qnil;
	}

	return rb_ary_new3 (3, UINT2NUM (type), UINT2NUM (id),
	                    UINT2NUM (arg));
}

void Init_Result (void)
{
	cResult = rb_define_class_under (mXmmsClient, "Result",
	                                 rb_cObject);

	/* ugh, we have to define the "new" method,
	 * so we can remove it again :(
	 */
	rb_define_singleton_method (cResult, "new", NULL, 0);
	rb_undef_method (rb_singleton_class (cResult), "new");

	rb_define_method (cResult, "notifier", c_notifier_set, 0);
	rb_define_method (cResult, "wait", c_wait, 0);
	rb_define_method (cResult, "restart", c_restart, 0);
	rb_define_method (cResult, "int", c_int_get, 0);
	rb_define_method (cResult, "uint", c_uint_get, 0);
	rb_define_method (cResult, "string", c_string_get, 0);
	rb_define_method (cResult, "hashtable", c_hashtable_get, 0);
	rb_define_method (cResult, "intlist", c_intlist_get, 0);
	rb_define_method (cResult, "uintlist", c_uintlist_get, 0);
	rb_define_method (cResult, "stringlist", c_stringlist_get, 0);
	rb_define_method (cResult, "playlist_change",
	                  c_playlist_change_get, 0);

	rb_define_const (cResult, "PLAYLIST_ADD",
	                 INT2FIX (XMMSC_PLAYLIST_ADD));
	rb_define_const (cResult, "PLAYLIST_CLEAR",
	                 INT2FIX (XMMSC_PLAYLIST_CLEAR));
	rb_define_const (cResult, "PLAYLIST_REMOVE",
	                 INT2FIX (XMMSC_PLAYLIST_REMOVE));
}
