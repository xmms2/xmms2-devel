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

#ifdef HAVE_ECORE
#include <Ecore.h>
#endif

#include <glib.h>

#include <xmms/xmmsclient.h>

#ifdef HAVE_ECORE
#include <xmms/xmmsclient-ecore.h>
#endif

#include <xmms/xmmsclient-glib.h>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <ruby.h>

#include "rb_xmmsclient_main.h"
#include "rb_result.h"

#define METHOD_ADD_HANDLER(name) \
	static VALUE c_##name (VALUE self) \
	{ \
		xmmsc_result_t *res; \
		VALUE o; \
\
		GET_OBJ (self, RbXmmsClient, xmms); \
\
		res = xmmsc_##name (xmms->real); \
\
		o = TO_XMMS_CLIENT_RESULT (self, res); \
		rb_ary_push (xmms->results, o); \
\
		return o; \
	}

#define METHOD_ADD(mod, name, argc) \
	rb_define_method ((mod), #name, c_##name, (argc));

typedef struct {
	xmmsc_connection_t *real;
	VALUE results;
} RbXmmsClient;

static void c_mark (RbXmmsClient *xmms)
{
	rb_gc_mark (xmms->results);
}

static void c_free (RbXmmsClient *xmms)
{
	if (xmms->real)
		xmmsc_deinit (xmms->real);

#ifdef HAVE_ECORE
	ecore_shutdown ();
#endif

	free (xmms);
}

static VALUE c_new (VALUE klass)
{
	VALUE self;
	RbXmmsClient *xmms;

#ifdef HAVE_ECORE
	ecore_init ();
#endif

	self = Data_Make_Struct (klass, RbXmmsClient,
	                         c_mark, c_free, xmms);
	xmms->results = rb_ary_new ();

	rb_obj_call_init (self, 0, NULL);

	return self;
}

static const char *get_login ()
{
	int uid = getuid ();
	struct passwd *pw;
	static char ret[64] = {0};

	if (ret[0])
		return ret;

	setpwent ();

	while ((pw = getpwent ()))
		if (pw->pw_uid == uid)
			snprintf (ret, sizeof (ret), "%s", pw->pw_name);

	endpwent ();

	return ret;
}

static VALUE c_connect (int argc, VALUE *argv, VALUE self)
{
	VALUE vpath;
	char *dbus_path, path[PATH_MAX + 1];

	GET_OBJ (self, RbXmmsClient, xmms);

	if (argc > 0) {
		rb_scan_args (argc, argv, "1", &vpath);
		Check_Type (vpath, T_STRING);
		dbus_path = StringValuePtr (vpath);
	} else if (!(dbus_path = getenv ("DBUS_PATH")))
		snprintf (path, sizeof (path),
		          "unix:path=/tmp/xmms-dbus-%s",
		         get_login ());

	if (dbus_path)
		snprintf (path, sizeof (path), "%s", dbus_path);

	if (!(xmms->real = xmmsc_init ()))
		return Qfalse;

	return xmmsc_connect (xmms->real, path) ? Qtrue : Qfalse;
}

static VALUE c_disconnect (VALUE self)
{
	GET_OBJ (self, RbXmmsClient, xmms);

	xmmsc_deinit (xmms->real);
	xmms->real = NULL;

	return Qtrue;
}

#ifdef HAVE_ECORE
static VALUE c_setup_with_ecore (VALUE self)
{
	GET_OBJ (self, RbXmmsClient, xmms);

	xmmsc_setup_with_ecore (xmms->real);

	return Qnil;
}
#endif

static VALUE c_setup_with_gmain (VALUE self)
{
	GET_OBJ (self, RbXmmsClient, xmms);

	xmmsc_setup_with_gmain (xmms->real, g_main_context_default ());

	return Qnil;
}

METHOD_ADD_HANDLER(quit);

METHOD_ADD_HANDLER(playback_start);
METHOD_ADD_HANDLER(playback_pause);
METHOD_ADD_HANDLER(playback_stop);
METHOD_ADD_HANDLER(playback_next);
METHOD_ADD_HANDLER(playback_status);
METHOD_ADD_HANDLER(playback_playtime);
METHOD_ADD_HANDLER(playback_statistics);
METHOD_ADD_HANDLER(playback_current_id);

static VALUE c_playback_seek_ms (VALUE self, VALUE ms)
{
	VALUE o;
	xmmsc_result_t *res;

	GET_OBJ (self, RbXmmsClient, xmms);

	Check_Type (ms, T_FIXNUM);

	res = xmmsc_playback_seek_ms (xmms->real, NUM2UINT (ms));

	o = TO_XMMS_CLIENT_RESULT (self, res);
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

	o = TO_XMMS_CLIENT_RESULT (self, res);
	rb_ary_push (xmms->results, o);

	return o;
}

METHOD_ADD_HANDLER(playlist_changed);
METHOD_ADD_HANDLER(playlist_entry_changed);
METHOD_ADD_HANDLER(playlist_list);

static VALUE c_playlist_set_next (VALUE self, VALUE type, VALUE moment)
{
	VALUE o;
	xmmsc_result_t *res;

	GET_OBJ (self, RbXmmsClient, xmms);

	Check_Type (type, T_FIXNUM);
	Check_Type (moment, T_FIXNUM);

	res = xmmsc_playlist_set_next (xmms->real, FIX2INT (type),
	                               FIX2INT (moment));

	o = TO_XMMS_CLIENT_RESULT (self, res);
	rb_ary_push (xmms->results, o);

	return o;
}

static VALUE c_playlist_get_mediainfo (VALUE self, VALUE id)
{
	VALUE o;
	xmmsc_result_t *res;

	GET_OBJ (self, RbXmmsClient, xmms);

	Check_Type (id, T_FIXNUM);

	res = xmmsc_playlist_get_mediainfo (xmms->real, FIX2INT (id));

	o = TO_XMMS_CLIENT_RESULT (self, res);
	rb_ary_push (xmms->results, o);

	return o;
}

void Init_XmmsClient (void)
{
	VALUE c;

	rb_require ("ecore");

	c = rb_define_class_under (mXmmsClient, "XmmsClient", rb_cObject);

	rb_define_singleton_method (c, "new", c_new, 0);
	rb_define_method (c, "connect", c_connect, -1);
	rb_define_method (c, "disconnect", c_disconnect, 0);

#ifdef HAVE_ECORE
	rb_define_method (c, "setup_with_ecore", c_setup_with_ecore, 0);
#endif

	rb_define_method (c, "setup_with_gmain", c_setup_with_gmain, 0);

	METHOD_ADD (c, quit, 0);
	METHOD_ADD (c, playback_start, 0);
	METHOD_ADD (c, playback_pause, 0);
	METHOD_ADD (c, playback_stop, 0);
	METHOD_ADD (c, playback_next, 0);
	METHOD_ADD (c, playback_status, 0);
	METHOD_ADD (c, playback_playtime, 0);
	METHOD_ADD (c, playback_statistics, 0);
	METHOD_ADD (c, playback_current_id, 0);
	METHOD_ADD (c, playback_seek_ms, 1);
	METHOD_ADD (c, playback_seek_samples, 1);

	METHOD_ADD (c, playlist_changed, 0);
	METHOD_ADD (c, playlist_entry_changed, 0);
	METHOD_ADD (c, playlist_list, 0);
	METHOD_ADD (c, playlist_set_next, 2);
	METHOD_ADD (c, playlist_get_mediainfo, 1);

	rb_define_const (c, "PLAY",
	                 INT2FIX (XMMSC_PLAYBACK_PLAY));
	rb_define_const (c, "STOP",
	                 INT2FIX (XMMSC_PLAYBACK_STOP));
	rb_define_const (c, "PAUSE",
	                 INT2FIX (XMMSC_PLAYBACK_PAUSE));
}
