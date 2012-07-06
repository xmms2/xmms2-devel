/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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
	VALUE xmms;
} RbResult;

/* An xmmsv_t of type XMMSV_TYPE_DICT */
typedef struct {
	xmmsv_t *real;
	VALUE parent;
} RbDict;

static VALUE extract_value (VALUE parent, xmmsv_t *val);

static VALUE cResult, cDict, cRawDict,
             cBroadcastResult, cSignalResult,
             eResultError, eValueError;

static void
c_mark (RbResult *res)
{
	rb_gc_mark (res->xmms);
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

	rbres->real = res;
	rbres->xmms = xmms;

	rb_obj_call_init (self, 0, NULL);

	return self;
}

static void
c_dict_mark (RbDict *dict)
{
	rb_gc_mark (dict->parent);
}

static void
c_dict_free (RbDict *dict)
{
	xmmsv_unref (dict->real);

	free (dict);
}

static int
on_signal (xmmsv_t *val, void *data)
{
	VALUE rbval, ret, callback = (VALUE) data;

	rbval = extract_value (Qnil, val);

	ret = rb_funcall (callback, rb_intern ("call"), 1, rbval);

	if (ret == Qnil || ret == Qfalse)
		return 0;
	else if (ret == Qtrue)
		return 1;
	else
		return NUM2INT (ret);
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
	VALUE callback;
	RbResult *res = NULL;
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbResult, res);

	if (!rb_block_given_p ())
		return Qnil;

	callback = rb_block_proc ();

	Data_Get_Struct (res->xmms, RbXmmsClient, xmms);
	rb_ary_push (xmms->result_callbacks, callback);

	xmmsc_result_notifier_set (res->real, on_signal, (void *) callback);

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

static VALUE
c_disconnect (VALUE self)
{
	RbResult *res = NULL;

	Data_Get_Struct (self, RbResult, res);

	xmmsc_result_disconnect (res->real);

	return self;
}

static VALUE
c_value_get (VALUE self)
{
	RbResult *res = NULL;
	xmmsv_t *val;

	Data_Get_Struct (self, RbResult, res);

	val = xmmsc_result_get_value (res->real);

	return extract_value (self, val);
}

static VALUE
int_get (xmmsv_t *val)
{
	int32_t id = 0;

	if (!xmmsv_get_int (val, &id))
		rb_raise (eValueError, "cannot retrieve value");

	return INT2NUM (id);
}

static VALUE
string_get (xmmsv_t *val)
{
	const char *s = NULL;

	if (!xmmsv_get_string (val, &s))
		rb_raise (eValueError, "cannot retrieve value");

	return rb_str_new2 (s ? s : "");
}

static VALUE
bin_get (xmmsv_t *val)
{
	const unsigned char *data = NULL;
	unsigned int len = 0;

	if (!xmmsv_get_bin (val, &data, &len))
		rb_raise (eValueError, "cannot retrieve value");

	return rb_str_new ((char *) data, len);
}

static VALUE
coll_get (xmmsv_t *val)
{
	xmmsc_coll_t *coll = NULL;

	if (!xmmsv_get_coll (val, &coll))
		rb_raise (eValueError, "cannot retrieve value");

	return TO_XMMS_CLIENT_COLLECTION (coll);
}

static void
list_to_array (xmmsv_t *value, void *user_data)
{
	VALUE *args = user_data;

	rb_ary_push (args[0], extract_value (args[1], value));
}

static VALUE
list_get (VALUE parent, xmmsv_t *val)
{
	VALUE args[2];

	args[0] = rb_ary_new ();
	args[1] = parent;

	xmmsv_list_foreach (val, list_to_array, args);

	return args[0];
}

static VALUE
extract_value (VALUE parent, xmmsv_t *val)
{
	switch (xmmsv_get_type (val)) {
		case XMMSV_TYPE_INT32:
			return int_get (val);
		case XMMSV_TYPE_STRING:
			return string_get (val);
		case XMMSV_TYPE_BIN:
			return bin_get (val);
		case XMMSV_TYPE_COLL:
			return coll_get (val);
		case XMMSV_TYPE_LIST:
			return list_get (parent, val);
		case XMMSV_TYPE_DICT:
			// will be handled below
			break;
		default:
			return Qnil;
	}

	VALUE value;
	RbDict *dict = NULL;

	value = Data_Make_Struct (cRawDict, RbDict,
			c_dict_mark, c_dict_free,
			dict);

	dict->real = xmmsv_ref (val);
	dict->parent = parent;

	rb_obj_call_init (value, 0, NULL);

	return value;
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
	xmmsv_t *val;
	const char *error;
	int ret;

	Data_Get_Struct (self, RbResult, res);

	val = xmmsc_result_get_value (res->real);

	ret = xmmsv_get_error (val, &error);

	return rb_str_new2 (ret ? error : "");
}

#ifdef HAVE_RB_PROTECT_INSPECT
static VALUE
dict_inspect_cb (VALUE args, VALUE s)
{
	VALUE key, value;

	key = RARRAY (args)->ptr[0];
	value = RARRAY (args)->ptr[1];

	if (RSTRING_LEN (s) > 1)
		rb_str_buf_cat2 (s, ", ");

	rb_str_buf_append (s, rb_inspect (key));
	rb_str_buf_cat2 (s, "=>");
	rb_str_buf_append (s, rb_inspect (value));

	return Qnil;
}

static VALUE
dict_inspect (VALUE self)
{
	VALUE ret;

	ret = rb_str_new2 ("{");
	rb_iterate (rb_each, self, dict_inspect_cb, ret);
	rb_str_buf_cat2 (ret, "}");

	OBJ_INFECT (ret, self);

	return ret;
}

static VALUE
c_dict_inspect (VALUE self)
{
	return rb_protect_inspect (dict_inspect, self, 0);
}
#endif /* HAVE_RB_PROTECT_INSPECT */

static VALUE
c_dict_size (VALUE self)
{
	RbDict *dict = NULL;
	int size;

	Data_Get_Struct (self, RbDict, dict);

	size = xmmsv_dict_get_size (dict->real);

	return INT2NUM (size);
}

static VALUE
c_dict_empty (VALUE self)
{
	RbDict *dict = NULL;
	int size;

	Data_Get_Struct (self, RbDict, dict);

	size = xmmsv_dict_get_size (dict->real);

	return size == 0 ? Qtrue : Qfalse;
}

static VALUE
c_dict_aref (VALUE self, VALUE key)
{
	RbDict *dict = NULL;
	xmmsv_dict_iter_t *it;
	xmmsv_t *value;
	const char *ckey;
	int s;

	Check_Type (key, T_SYMBOL);

	Data_Get_Struct (self, RbDict, dict);

	ckey = rb_id2name (SYM2ID (key));

	xmmsv_get_dict_iter (dict->real, &it);

	s = xmmsv_dict_iter_find (it, ckey);
	if (!s)
		return Qnil;

	xmmsv_dict_iter_pair (it, NULL, &value);

	return extract_value (self, value);
}

static VALUE
c_dict_has_key (VALUE self, VALUE key)
{
	RbDict *dict = NULL;
	xmmsv_dict_iter_t *it;
	const char *ckey;

	Check_Type (key, T_SYMBOL);

	Data_Get_Struct (self, RbDict, dict);

	ckey = rb_id2name (SYM2ID (key));

	xmmsv_get_dict_iter (dict->real, &it);

	return xmmsv_dict_iter_find (it, ckey) ? Qtrue : Qfalse;
}

static void
dict_each_pair (const char *key, xmmsv_t *value, void *udata)
{
	VALUE *parent = udata;

	rb_yield_values (2,
	                 ID2SYM (rb_intern (key)),
	                 extract_value (*parent, value));
}

static void
dict_each_key (const char *key, xmmsv_t *value, void *udata)
{
	rb_yield (ID2SYM (rb_intern (key)));
}

static void
dict_each_value (const char *key, xmmsv_t *value, void *udata)
{
	VALUE *parent = udata;

	rb_yield (extract_value (*parent, value));
}

static VALUE
c_dict_each (VALUE self)
{
	RbDict *dict = NULL;

	Data_Get_Struct (self, RbDict, dict);

	xmmsv_dict_foreach (dict->real, dict_each_pair, &self);

	return self;
}

static VALUE
c_dict_each_key (VALUE self)
{
	RbDict *dict = NULL;

	Data_Get_Struct (self, RbDict, dict);

	xmmsv_dict_foreach (dict->real, dict_each_key, NULL);

	return self;
}

static VALUE
c_dict_each_value (VALUE self)
{
	RbDict *dict = NULL;

	Data_Get_Struct (self, RbDict, dict);

	xmmsv_dict_foreach (dict->real, dict_each_value, &self);

	return self;
}

/*
 * call-seq:
 *  rawdict.to_propdict( src_prefs ) -> propdict
 *
 * Transforms a RawDict (key-source-value) to a regular
 * key-value dict.
 * The optional src_prefs argument restricts which sources
 * are considered. The value may be a string or an array
 * of strings, which may contain wildcards.
 * Example: rawdict.to_propdict( ['server','plugin/*'] )
 */
static VALUE
c_raw_dict_to_propdict (int argc, VALUE *argv, VALUE self)
{
	VALUE value, sources = Qnil;
	RbDict *dict = NULL, *dict2 = NULL;
	xmmsv_t *inner_dict;
	const char **csources = NULL;

	Data_Get_Struct (self, RbDict, dict);

	rb_scan_args (argc, argv, "01", &sources);

	if (!NIL_P (sources))
		csources = parse_string_array (sources);
	inner_dict = xmmsv_propdict_to_dict (dict->real, csources);
	if (csources)
		free (csources);

	value = Data_Make_Struct (cDict, RbDict,
	                          c_dict_mark, c_dict_free,
	                          dict2);

	// don't add a second reference here
	dict2->real = inner_dict;
	dict2->parent = dict->parent;

	rb_obj_call_init (value, 0, NULL);

	return value;
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

	cBroadcastResult = rb_define_class_under (mXmms,
	                                          "BroadcastResult",
	                                          cResult);
	rb_define_method (cBroadcastResult, "disconnect", c_disconnect, 0);

	cSignalResult = rb_define_class_under (mXmms, "SignalResult",
	                                       cResult);
	rb_define_method (cSignalResult, "disconnect", c_disconnect, 0);

	eResultError = rb_define_class_under (cResult, "ResultError",
	                                      rb_eStandardError);
	eValueError = rb_define_class_under (cResult, "ValueError",
	                                     eResultError);

	cDict = rb_define_class_under (mXmms, "Dict", rb_cObject);

#ifdef HAVE_RB_PROTECT_INSPECT
	rb_define_method (cDict, "inspect", c_dict_inspect, 0);
#endif /* HAVE_RB_PROTECT_INSPECT */

	rb_define_method (cDict, "size", c_dict_size, 0);
	rb_define_method (cDict, "empty?", c_dict_empty, 0);
	rb_define_method (cDict, "[]", c_dict_aref, 1);
	rb_define_method (cDict, "has_key?", c_dict_has_key, 1);
	rb_define_method (cDict, "each", c_dict_each, 0);
	rb_define_method (cDict, "each_key", c_dict_each_key, 0);
	rb_define_method (cDict, "each_value", c_dict_each_value, 0);

	rb_define_alias (cDict, "length", "size");
	rb_define_alias (cDict, "include?", "has_key?");
	rb_define_alias (cDict, "key?", "has_key?");
	rb_define_alias (cDict, "member?", "has_key?");
	rb_define_alias (cDict, "each_pair", "each");

	rb_include_module (cDict, rb_mEnumerable);

	cRawDict = rb_define_class_under (mXmms, "RawDict", cDict);

	rb_define_method (cRawDict, "to_propdict", c_raw_dict_to_propdict, -1);
}
