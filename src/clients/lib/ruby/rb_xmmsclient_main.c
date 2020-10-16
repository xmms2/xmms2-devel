/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

void Init_Client (VALUE);

void Init_xmmsclient_ext (void);

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
	char path[XMMS_PATH_MAX];

	p = xmmsc_userconfdir_get (path, XMMS_PATH_MAX);

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
	const unsigned char *burl;
	unsigned int blen;
	xmmsv_t *strv, *decoded;
	VALUE url = Qnil;

	strv = xmmsv_new_string (StringValuePtr (str));

	decoded = xmmsv_decode_url (strv);

	if (!decoded)
		goto out;

	if (!xmmsv_get_bin (decoded, &burl, &blen))
		goto out;

	url = rb_str_new ((char *) burl, blen);

out:
	if (decoded)
		xmmsv_unref (decoded);

	xmmsv_unref (strv);

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
