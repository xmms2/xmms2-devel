/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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
#include "rb_collection.h"
#include "rb_result.h"

typedef struct {
	xmmsc_result_t *real;
	xmmsc_result_t *orig;
	VALUE xmms;
	VALUE callback;
	VALUE propdict;
} RbResult;

static VALUE cResult, cPropDict, cBroadcastResult, cSignalResult,
             eResultError, eValueError;

static void
c_mark (RbResult *res)
{
	rb_gc_mark (res->xmms);

	if (!NIL_P (res->callback))
		rb_gc_mark (res->callback);

	if (!NIL_P (res->propdict))
		rb_gc_mark (res->propdict);
}

static void
c_free (RbResult *res)
{
	xmmsc_result_unref (res->real);

	free (res);
}

VALUE
TO_XMMS_CLIENT_RESULT (VALUE xmms, xmmsc_result_t *res)
{
	VALUE self, klass;
	RbResult *rbres = NULL;

	if (!res)
		return Qnil;

	switch (xmmsc_result_get_class (res)) {
		case XMMSC_RESULT_CLASS_SIGNAL:
			klass = cSignalResult;
			break;
		case XMMSC_RESULT_CLASS_BROADCAST:
			klass = cBroadcastResult;
			break;
		default:
			klass = cResult;
			break;
	}

	self = Data_Make_Struct (klass, RbResult, c_mark, c_free, rbres);

	rbres->real = rbres->orig = res;
	rbres->xmms = xmms;
	rbres->callback = rbres->propdict = Qnil;

	rb_obj_call_init (self, 0, NULL);

	return self;
}

static void
on_signal (xmmsc_result_t *res2, void *data)
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
static VALUE
c_notifier_set (VALUE self)
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
static VALUE
c_wait (VALUE self)
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
static VALUE
c_sig_restart (VALUE self)
{
	xmmsc_result_t *res2;
	RbResult *res = NULL;
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbResult, res);

	if (!(res2 = xmmsc_result_restart (res->real)))
		rb_raise (eResultError, "cannot restart result");

	res->real = res2;

	Data_Get_Struct (res->xmms, RbXmmsClient, xmms);
	rb_ary_push (xmms->results, self);

	xmmsc_result_unref (res->real);

	return self;
}

static VALUE
c_disconnect (VALUE self)
{
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	xmmsc_result_disconnect (res->orig);

	return self;
}

static VALUE
int_get (RbResult *res)
{
	int32_t id = 0;

	if (!xmmsc_result_get_int (res->real, &id))
		rb_raise (eValueError, "cannot retrieve value");

	return INT2NUM (id);
}

static VALUE
uint_get (RbResult *res)
{
	uint32_t id = 0;

	if (!xmmsc_result_get_uint (res->real, &id))
		rb_raise (eValueError, "cannot retrieve value");

	return UINT2NUM (id);
}

static VALUE
string_get (RbResult *res)
{
	const char *s = NULL;

	if (!xmmsc_result_get_string (res->real, &s))
		rb_raise (eValueError, "cannot retrieve value");

	return rb_str_new2 (s ? s : "");
}

static VALUE
cast_result_value (xmmsc_result_value_type_t type, const void *value)
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

static void
dict_to_hash (const void *key, xmmsc_result_value_type_t type,
              const void *value, void *udata)
{
	VALUE *h = udata;

	rb_hash_aset (*h, ID2SYM (rb_intern (key)),
	              cast_result_value (type, value));
}

static VALUE
hashtable_get (RbResult *res)
{
	VALUE hash = rb_hash_new ();

	if (!xmmsc_result_dict_foreach (res->real,
	                                dict_to_hash, &hash))
		rb_raise (eValueError, "cannot retrieve value");

	return hash;
}

static VALUE
propdict_get (VALUE self, RbResult *res)
{
	if (NIL_P (res->propdict))
		res->propdict = rb_class_new_instance (1, &self, cPropDict);

	return res->propdict;
}

static VALUE
bin_get (VALUE self, RbResult *res)
{
	unsigned char *data = NULL;
	unsigned int len = 0;

	if (!xmmsc_result_get_bin (res->real, &data, &len))
		rb_raise (eValueError, "cannot retrieve value");

	return rb_str_new ((char *) data, len);
}

static VALUE
coll_get (VALUE self, RbResult *res)
{
	xmmsc_coll_t *coll = NULL;

	if (!xmmsc_result_get_collection (res->real, &coll))
		rb_raise (eValueError, "cannot retrieve value");

	return TO_XMMS_CLIENT_COLLECTION (coll);
}

static VALUE
value_get (VALUE self, RbResult *res)
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
			ret = propdict_get (self, res);
			break;
		case XMMS_OBJECT_CMD_ARG_BIN:
			ret = bin_get (self, res);
			break;
		case XMMS_OBJECT_CMD_ARG_COLL:
			ret = coll_get (self, res);
			break;
		/* don't check for XMMS_OBJECT_CMD_ARG_LIST here */
		default:
			ret = Qnil;
			break;
	}

	return ret;
}

static VALUE
list_get (VALUE self, RbResult *res)
{
	VALUE ret;

	ret = rb_ary_new ();

	while (xmmsc_result_list_valid (res->real)) {
		rb_ary_push (ret, value_get (self, res));

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
static VALUE
c_value_get (VALUE self)
{
	RbResult *res;

	Data_Get_Struct (self, RbResult, res);

	if (xmmsc_result_iserror (res->real))
		rb_raise (eValueError, "cannot retrieve value");

	if (xmmsc_result_is_list (res->real))
		return list_get (self, res);
	else
		return value_get (self, res);
}

static VALUE
c_is_error (VALUE self)
{
	RbResult *res;

	Data_Get_Struct (self, RbResult, res);

	return xmmsc_result_iserror (res->real) ? Qtrue : Qfalse;
}

static VALUE
c_get_error (VALUE self)
{
	RbResult *res;
	const char *error;

	Data_Get_Struct (self, RbResult, res);

	error = xmmsc_result_get_error (res->real);

	return rb_str_new2 (error ? error : "");
}

static VALUE
c_propdict_init (VALUE self, VALUE result)
{
	rb_iv_set (self, "result", result);

	return self;
}

#ifdef HAVE_PROTECT_INSPECT
static VALUE
propdict_inspect_cb (VALUE args, VALUE s)
{
	VALUE src, key, value;

	src = RARRAY (args)->ptr[0];
	key = RARRAY (args)->ptr[1];
	value = RARRAY (args)->ptr[2];

	if (RSTRING_LEN (s) > 1)
		rb_str_buf_cat2 (s, ", ");

	rb_str_buf_cat2 (s, "[");
	rb_str_buf_append (s, src);
	rb_str_buf_cat2 (s, "]");

	rb_str_buf_append (s, rb_inspect (key));
	rb_str_buf_cat2 (s, "=>");
	rb_str_buf_append (s, rb_inspect (value));

	return Qnil;
}

static VALUE
propdict_inspect (VALUE self)
{
	VALUE ret;

	ret = rb_str_new2 ("{");
	rb_iterate (rb_each, self, propdict_inspect_cb, ret);
	rb_str_buf_cat2 (ret, "}");

	OBJ_INFECT (ret, self);

	return ret;
}

static VALUE
c_propdict_inspect (VALUE self)
{
	return rb_protect_inspect (propdict_inspect, self, 0);
}
#endif /* HAVE_PROTECT_INSPECT */

static VALUE
c_propdict_aref (VALUE self, VALUE key)
{
	RbResult *res = NULL;
	xmmsc_result_value_type_t type;
	VALUE tmp;
	const char *ckey, *vstr;
	int32_t vint;
	uint32_t vuint;

	Check_Type (key, T_SYMBOL);

	tmp = rb_iv_get (self, "result");
	Data_Get_Struct (tmp, RbResult, res);

	ckey = rb_id2name (SYM2ID (key));

	type = xmmsc_result_get_dict_entry_type (res->real, ckey);

	switch (type) {
		case XMMSC_RESULT_VALUE_TYPE_INT32:
			xmmsc_result_get_dict_entry_int (res->real, ckey, &vint);
			tmp = INT2NUM (vint);
			break;
		case XMMSC_RESULT_VALUE_TYPE_UINT32:
			xmmsc_result_get_dict_entry_uint (res->real, ckey, &vuint);
			tmp = UINT2NUM (vuint);
			break;
		case XMMSC_RESULT_VALUE_TYPE_STRING:
			xmmsc_result_get_dict_entry_string (res->real, ckey, &vstr);
			tmp = rb_str_new2 (vstr ? vstr : "");
			break;
		default:
			tmp = Qnil;
			break;
	}

	return tmp;
}

static VALUE
c_propdict_has_key (VALUE self, VALUE key)
{
	RbResult *res = NULL;
	VALUE tmp;
	xmmsc_result_value_type_t type;
	const char *ckey;

	Check_Type (key, T_SYMBOL);

	tmp = rb_iv_get (self, "result");
	Data_Get_Struct (tmp, RbResult, res);

	ckey = rb_id2name (SYM2ID (key));

	type = xmmsc_result_get_dict_entry_type (res->real, ckey);

	return (type == XMMSC_RESULT_VALUE_TYPE_NONE) ? Qfalse : Qtrue;
}

static void
propdict_each (const void *key, xmmsc_result_value_type_t type,
               const void *value, const char *src, void *udata)
{
	switch (XPOINTER_TO_INT (udata)) {
		case EACH_PAIR:
			rb_yield_values (3, rb_str_new2 (src),
			                 ID2SYM (rb_intern (key)),
			                 cast_result_value (type, value));
			break;
		case EACH_KEY:
			rb_yield_values (2, rb_str_new2 (src),
			                 ID2SYM (rb_intern (key)));
			break;
		case EACH_VALUE:
			rb_yield_values (2, rb_str_new2 (src),
			                 cast_result_value (type, value));
			break;
	}
}

static VALUE
c_propdict_each (VALUE self)
{
	RbResult *res = NULL;
	VALUE tmp;

	tmp = rb_iv_get (self, "result");
	Data_Get_Struct (tmp, RbResult, res);

	xmmsc_result_propdict_foreach (res->real, propdict_each,
	                               XINT_TO_POINTER (EACH_PAIR));

	return self;
}

static VALUE
c_propdict_each_key (VALUE self)
{
	RbResult *res = NULL;
	VALUE tmp;

	tmp = rb_iv_get (self, "result");
	Data_Get_Struct (tmp, RbResult, res);

	xmmsc_result_propdict_foreach (res->real, propdict_each,
	                               XINT_TO_POINTER (EACH_KEY));

	return self;
}

static VALUE
c_propdict_each_value (VALUE self)
{
	RbResult *res = NULL;
	VALUE tmp;

	tmp = rb_iv_get (self, "result");
	Data_Get_Struct (tmp, RbResult, res);

	xmmsc_result_propdict_foreach (res->real, propdict_each,
	                               XINT_TO_POINTER (EACH_VALUE));

	return self;
}

static VALUE
c_source_preference_get (VALUE self)
{
	RbResult *res = NULL;
	VALUE ary = rb_ary_new ();
	const char **preferences = NULL;
	unsigned int i;

	Data_Get_Struct (self, RbResult, res);

	preferences = xmmsc_result_source_preference_get (res->real);

	for (i = 0; preferences[i]; i++) {
		if (!preferences[i]) {
			continue;
		}
		rb_ary_push (ary, rb_str_new2 (preferences[i]));
	}

	return ary;
}

void
Init_Result (VALUE mXmms)
{
	cResult = rb_define_class_under (mXmms, "Result", rb_cObject);

	/* ugh, we have to define the "new" method,
	 * so we can remove it again :(
	 */
	rb_define_singleton_method (cResult, "new", NULL, 0);
	rb_undef_method (rb_singleton_class (cResult), "new");

	rb_define_method (cResult, "notifier", c_notifier_set, 0);
	rb_define_method (cResult, "wait", c_wait, 0);
	rb_define_method (cResult, "value", c_value_get, 0);
	rb_define_method (cResult, "error?", c_is_error, 0);
	rb_define_method (cResult, "error", c_get_error, 0);

	rb_define_method (cResult, "source_preference", c_source_preference_get, 0);

	cBroadcastResult = rb_define_class_under (mXmms,
	                                          "BroadcastResult",
	                                          cResult);
	rb_define_method (cBroadcastResult, "disconnect", c_disconnect, 0);

	cSignalResult = rb_define_class_under (mXmms, "SignalResult",
	                                       cResult);
	rb_define_method (cSignalResult, "restart", c_sig_restart, 0);
	rb_define_method (cSignalResult, "disconnect", c_disconnect, 0);

	eResultError = rb_define_class_under (cResult, "ResultError",
	                                      rb_eStandardError);
	eValueError = rb_define_class_under (cResult, "ValueError",
	                                     eResultError);

	cPropDict = rb_define_class_under (mXmms, "PropDict", rb_cObject);

	rb_define_method (cPropDict, "initialize", c_propdict_init, 1);
#ifdef HAVE_PROTECT_INSPECT
	rb_define_method (cPropDict, "inspect", c_propdict_inspect, 0);
#endif /* HAVE_PROTECT_INSPECT */

	rb_define_method (cPropDict, "[]", c_propdict_aref, 1);
	rb_define_method (cPropDict, "has_key?", c_propdict_has_key, 1);
	rb_define_method (cPropDict, "each", c_propdict_each, 0);
	rb_define_method (cPropDict, "each_key", c_propdict_each_key, 0);
	rb_define_method (cPropDict, "each_value", c_propdict_each_value, 0);

	rb_define_alias (cPropDict, "include?", "has_key?");
	rb_define_alias (cPropDict, "key?", "has_key?");
	rb_define_alias (cPropDict, "member?", "has_key?");
	rb_define_alias (cPropDict, "each_pair", "each");

	rb_include_module (cPropDict, rb_mEnumerable);
}
