/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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




#ifndef __XMMS_ERROR_H__
#define __XMMS_ERROR_H__

#include <glib.h>
#include <xmmsc/xmmsc_errorcodes.h>
#include <xmmsc/xmmsc_compiler.h>

#define XMMS_ERROR_MESSAGE_MAXLEN 255

G_BEGIN_DECLS

typedef struct xmms_error_St {
	xmms_error_code_t code;
	gchar message[XMMS_ERROR_MESSAGE_MAXLEN + 1];
} xmms_error_t;

static inline void
xmms_error_set (xmms_error_t *err, xmms_error_code_t code, const gchar *message)
{
	g_return_if_fail (err);

	err->code = code;
	if (message) {
		g_strlcpy (err->message, message, XMMS_ERROR_MESSAGE_MAXLEN);
	} else {
		err->message[0] = 0;
	}
}

static inline void
xmms_error_reset (xmms_error_t *err)
{
	g_return_if_fail (err);

	err->code = XMMS_ERROR_NONE;
	err->message[0] = 0;
}

#define xmms_error_iserror(e) ((e)->code != XMMS_ERROR_NONE)
#define xmms_error_isok(e) ((e)->code == XMMS_ERROR_NONE)

#define xmms_error_type_get(e) ((e)->code)

const gchar *xmms_error_message_get (xmms_error_t *err) XMMS_PUBLIC;

G_END_DECLS

#endif
