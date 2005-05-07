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
#include <xmms/output.h>

#include <xmms/xmmsclient-glib.h>

#include <ruby.h>
#include <stdbool.h>

#include "rb_xmmsclient_main.h"
#include "rb_xmmsclient.h"
#include "rb_result.h"

#define METHOD_ADD_HANDLER(name, unref) \
	static VALUE c_##name (VALUE self) \
	{ \
		xmmsc_result_t *res; \
		VALUE o; \
\
		GET_OBJ (self, RbXmmsClient, xmms); \
\
		res = xmmsc_##name (xmms->real); \
\
		o = TO_XMMS_CLIENT_RESULT (res, true, unref); \
		rb_ary_push (xmms->results, o); \
\
		return o; \
	}

#define METHOD_ADD(mod, name, argc) \
	rb_define_method ((mod), #name, c_##name, (argc));

VALUE eXmmsClientError;

static void c_mark (RbXmmsClient *xmms)
{
	rb_gc_mark (xmms->results);
}

static void c_free (RbXmmsClient *xmms)
{
	xmmsc_unref (xmms->real);

	free (xmms);
}

static VALUE c_alloc (VALUE klass)
{
	RbXmmsClient *xmms;

	return Data_Make_Struct (klass, RbXmmsClient,
	                         c_mark, c_free, xmms);
}

static VALUE c_init (VALUE self, VALUE name)
{
	GET_OBJ (self, RbXmmsClient, xmms);

	if (!(xmms->real = xmmsc_init (StringValuePtr (name)))) {
		rb_raise (rb_eNoMemError, "failed to allocate memory");
		return Qnil;
	}

	xmms->results = rb_ary_new ();

	return self;
}

static VALUE c_connect (int argc, VALUE *argv, VALUE self)
{
	VALUE path;
	char *p = NULL;

	GET_OBJ (self, RbXmmsClient, xmms);

	rb_scan_args (argc, argv, "01", &path);

	if (!NIL_P (path))
		p = StringValuePtr (path);

	if (!xmmsc_connect (xmms->real, p))
		rb_raise (eXmmsClientError, "cannot connect to daemon");

	return self;
}

METHOD_ADD_HANDLER(quit, true);

METHOD_ADD_HANDLER(playback_start, true);
METHOD_ADD_HANDLER(playback_pause, true);
METHOD_ADD_HANDLER(playback_stop, true);
METHOD_ADD_HANDLER(playback_tickle, true);
METHOD_ADD_HANDLER(playback_status, false);
METHOD_ADD_HANDLER(broadcast_playback_status, false);
METHOD_ADD_HANDLER(playback_playtime, true);
METHOD_ADD_HANDLER(signal_playback_playtime, true);
METHOD_ADD_HANDLER(playback_current_id, true);
METHOD_ADD_HANDLER(broadcast_playback_current_id, false);

METHOD_ADD_HANDLER(broadcast_configval_changed, false);

static VALUE c_playback_seek_ms (VALUE self, VALUE ms)
{
	VALUE o;
	xmmsc_result_t *res;

	GET_OBJ (self, RbXmmsClient, xmms);

	Check_Type (ms, T_FIXNUM);

	res = xmmsc_playback_seek_ms (xmms->real, NUM2UINT (ms));

	o = TO_XMMS_CLIENT_RESULT (res, true, true);
	rb_ary_push (xmms->results, o);

	return o;
}

static VALUE c_playback_seek_samples (VALUE self, VALUE samples)
{
	VALUE o;
	xmmsc_result_t *res;

	GET_OBJ (self, RbXmmsClient, xmms);

	Check_Type (samples, T_FIXNUM);

	res = xmmsc_playback_seek_samples (xmms->real, NUM2UINT (samples));

	o = TO_XMMS_CLIENT_RESULT (res, true, true);
	rb_ary_push (xmms->results, o);

	return o;
}

METHOD_ADD_HANDLER(broadcast_playlist_changed, false);
METHOD_ADD_HANDLER(playlist_current_pos, true);
METHOD_ADD_HANDLER(broadcast_playlist_current_pos, false);
METHOD_ADD_HANDLER(broadcast_medialib_entry_changed, false);
METHOD_ADD_HANDLER(playlist_list, true);

static VALUE c_playlist_set_next (VALUE self, VALUE pos)
{
	VALUE o;
	xmmsc_result_t *res;

	GET_OBJ (self, RbXmmsClient, xmms);

	Check_Type (pos, T_FIXNUM);

	res = xmmsc_playlist_set_next (xmms->real, FIX2INT (pos));

	o = TO_XMMS_CLIENT_RESULT (res, true, true);
	rb_ary_push (xmms->results, o);

	return o;
}

static VALUE c_playlist_set_next_rel (VALUE self, VALUE pos)
{
	VALUE o;
	xmmsc_result_t *res;

	GET_OBJ (self, RbXmmsClient, xmms);

	Check_Type (pos, T_FIXNUM);

	res = xmmsc_playlist_set_next_rel (xmms->real, FIX2INT (pos));

	o = TO_XMMS_CLIENT_RESULT (res, true, true);
	rb_ary_push (xmms->results, o);

	return o;
}

static VALUE c_playlist_add (VALUE self, VALUE path)
{
	VALUE o;
	xmmsc_result_t *res;

	StringValue (path);

	GET_OBJ (self, RbXmmsClient, xmms);

	res = xmmsc_playlist_add (xmms->real, StringValuePtr (path));

	o = TO_XMMS_CLIENT_RESULT (res, true, true);
	rb_ary_push (xmms->results, o);

	return o;
}

static VALUE c_medialib_get_info (VALUE self, VALUE id)
{
	VALUE o;
	xmmsc_result_t *res;

	GET_OBJ (self, RbXmmsClient, xmms);

	Check_Type (id, T_FIXNUM);

	res = xmmsc_medialib_get_info (xmms->real, FIX2INT (id));

	o = TO_XMMS_CLIENT_RESULT (res, true, true);
	rb_ary_push (xmms->results, o);

	return o;
}

static VALUE c_configval_get (VALUE self, VALUE key)
{
	VALUE o;
	xmmsc_result_t *res;

	GET_OBJ (self, RbXmmsClient, xmms);

	Check_Type (key, T_STRING);

	res = xmmsc_configval_get (xmms->real, StringValuePtr (key));

	o = TO_XMMS_CLIENT_RESULT (res, true, true);
	rb_ary_push (xmms->results, o);

	return o;
}

static VALUE c_configval_set (VALUE self, VALUE key, VALUE val)
{
	VALUE o;
	xmmsc_result_t *res;

	GET_OBJ (self, RbXmmsClient, xmms);

	Check_Type (key, T_STRING);
	Check_Type (val, T_STRING);

	res = xmmsc_configval_set (xmms->real, StringValuePtr (key),
	                           StringValuePtr (val));

	o = TO_XMMS_CLIENT_RESULT (res, true, true);
	rb_ary_push (xmms->results, o);

	return o;
}

void Init_XmmsClient (VALUE m)
{
	VALUE c;

	c = rb_define_class_under (m, "XmmsClient", rb_cObject);

	rb_define_alloc_func (c, c_alloc);
	rb_define_method (c, "initialize", c_init, 1);
	rb_define_method (c, "connect", c_connect, -1);

	METHOD_ADD (c, quit, 0);
	METHOD_ADD (c, playback_start, 0);
	METHOD_ADD (c, playback_pause, 0);
	METHOD_ADD (c, playback_stop, 0);
	METHOD_ADD (c, playback_tickle, 0);
	METHOD_ADD (c, broadcast_playback_status, 0);
	METHOD_ADD (c, playback_status, 0);
	METHOD_ADD (c, playback_playtime, 0);
	METHOD_ADD (c, signal_playback_playtime, 0);
	METHOD_ADD (c, playback_current_id, 0);
	METHOD_ADD (c, broadcast_playback_current_id, 0);
	METHOD_ADD (c, playback_seek_ms, 1);
	METHOD_ADD (c, playback_seek_samples, 1);

	METHOD_ADD (c, broadcast_playlist_changed, 0);
	METHOD_ADD (c, playlist_current_pos, 0);
	METHOD_ADD (c, broadcast_playlist_current_pos, 0);
	METHOD_ADD (c, broadcast_medialib_entry_changed, 0);
	METHOD_ADD (c, playlist_list, 0);
	METHOD_ADD (c, playlist_set_next, 1);
	METHOD_ADD (c, playlist_set_next_rel, 1);
	METHOD_ADD (c, playlist_add, 1);
	METHOD_ADD (c, medialib_get_info, 1);

	METHOD_ADD (c, configval_get, 1);
	METHOD_ADD (c, configval_set, 2);
	METHOD_ADD (c, broadcast_configval_changed, 0);

	rb_define_const (c, "PLAY",
	                 INT2FIX (XMMS_OUTPUT_STATUS_PLAY));
	rb_define_const (c, "STOP",
	                 INT2FIX (XMMS_OUTPUT_STATUS_STOP));
	rb_define_const (c, "PAUSE",
	                 INT2FIX (XMMS_OUTPUT_STATUS_PAUSE));

	eXmmsClientError = rb_define_class_under (m, "XmmsClientError",
	                                          rb_eStandardError);
}
