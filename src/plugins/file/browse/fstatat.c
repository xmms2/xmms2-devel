/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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

#define _ATFILE_SOURCE

#include <xmms/xmms_xformplugin.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#include "browse.h"

gboolean
xmms_file_browse (xmms_xform_t *xform,
                  const gchar *url,
                  xmms_error_t *error)
{
	DIR *dir;
	struct dirent *d;
	int dir_fd;
	const gchar *tmp;
	struct stat st;

	tmp = url + 7;

	dir = opendir (tmp);
	if (!dir) {
		xmms_error_set (error, XMMS_ERROR_NOENT, strerror (errno));
		return FALSE;
	}

	dir_fd = dirfd (dir);

	while ((d = readdir (dir))) {
		guint32 flags = 0;
		const char *entry;
		int ret;

		entry = d->d_name;

		if (!strcmp (d->d_name, ".") || !strcmp (d->d_name, ".."))
			continue;

		ret = fstatat (dir_fd, d->d_name, &st, 0);
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

	closedir (dir);

	return TRUE;
}
