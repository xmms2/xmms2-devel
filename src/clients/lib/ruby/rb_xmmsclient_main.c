/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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
#include <xmms_configuration.h>

#include <ruby.h>

void Init_Client ();

/*
 * call-seq:
 *  Xmms.userconfdir -> String
 *
 * Returns the xmms2 configuration directory for the current user.
 */
static VALUE
m_userconfdir_get (VALUE self)
{
	const char *p;
	char path[PATH_MAX];

	p = xmmsc_userconfdir_get (path, PATH_MAX);

	return p ? rb_str_new2 (p) : Qnil;
}

/*
 * call-seq:
 *  Xmms.decode_url(url) -> String
 *
 * Decodes a url-encoded string _url_ and returns it in UNKNOWN ENCODING.
 * Use with caution.
 */
static VALUE
m_decode_url (VALUE self, VALUE str)
{
	const char *cstr, *tmp;
	VALUE url;

	cstr = StringValuePtr (str);

	tmp = xmmsc_result_decode_url (NULL, cstr);
	if (!tmp)
		return Qnil;

	url = rb_str_new2 (tmp);

	/* We have to free tmp here ourselves because we didn't pass a
	 * result to xmmsc_result_decode_url() above.
	 */
	free ((void *) tmp);

	return url;
}

void
Init_xmmsclient_ext (void)
{
	VALUE mXmms = rb_define_module ("Xmms");

	rb_define_module_function (mXmms, "userconfdir", m_userconfdir_get, 0);
	rb_define_module_function (mXmms, "decode_url", m_decode_url, 1);

	rb_define_const (mXmms, "VERSION", rb_str_new2 (XMMS_VERSION));

	Init_Client (mXmms); /* initializes Result and Playlist, too */
}
