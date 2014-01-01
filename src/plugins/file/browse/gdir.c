/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
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

#include <xmms/xmms_xformplugin.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "browse.h"

gboolean
xmms_file_browse (xmms_xform_t *xform,
                  const gchar *url,
                  xmms_error_t *error)
{
	GDir *dir;
	GError *err = NULL;
	const gchar *d, *tmp;
	struct stat st;

	tmp = url + 7;

	dir = g_dir_open (tmp, 0, &err);
	if (!dir) {
		xmms_error_set (error, XMMS_ERROR_NOENT, err->message);
		return FALSE;
	}

	while ((d = g_dir_read_name (dir))) {
		guint32 flags = 0;
		const char *entry;
		int ret;

		gchar *t = g_build_filename (tmp, d, NULL);

		ret = stat (t, &st);
		g_free (t);

		entry = d;

		if (ret) {
			continue;
		}

		if (S_ISDIR (st.st_mode)) {
			flags |= XMMS_XFORM_BROWSE_FLAG_DIR;
		}

		xmms_xform_browse_add_entry (xform, entry, flags);

		if (!S_ISDIR (st.st_mode)) {
			xmms_xform_browse_add_entry_property_int (xform, "size",
			                                          st.st_size);
		}
	}

	g_dir_close (dir);

	return TRUE;
}
