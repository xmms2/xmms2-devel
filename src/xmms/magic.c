/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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




#include <glib.h>
#include <string.h>
#include "xmms/util.h"

typedef struct xmms_mimemap_St {
	gchar *ext;
	gchar *mime;
} xmms_mimemap_t;

const xmms_mimemap_t mimemap[] = {
	{ "mp3", "audio/mpeg" },
	{ "ogg", "application/ogg" },
	{ "wav", "audio/wave" },
	{ "sid", "audio/prs.sid" },
	{ "tar", "application/x-tar" },
	{ "m3u", "audio/mpegurl" },
	{ NULL, NULL }
};

const gchar *
xmms_magic_mime_from_file (const gchar *file)
{
	gchar *p;
	gint i = 0;

	g_return_val_if_fail (file, NULL);

	p = strrchr (file, '.');
	g_return_val_if_fail (p, NULL);

	*p++;

	while (mimemap[i].ext) {
		if (g_strcasecmp (mimemap[i].ext, p) == 0) {
			XMMS_DBG ("Mimetype acording to ext: %s", mimemap[i].mime);
			return mimemap[i].mime;
		}
		i++;
	}

	return NULL;

}

