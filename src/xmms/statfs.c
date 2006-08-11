/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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


/** @file
 *  "Portable" statfs
 *  Since this file is filled with ugly ifdefs
 *  and other things that we don't like in xmms2
 *  we leave it just with one function.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib.h>

#include "xmms/xmms_defs.h"

#if defined(STATFS_LINUX)
#include <sys/vfs.h>
#elif defined(STATFS_BSD)
#include <sys/param.h>
#include <sys/mount.h>
#endif

#include "xmms/xmms_log.h"
#include "xmmspriv/xmms_statfs.h"

/**
 * This function uses the statfs() call to
 * check if the path is on a remote filesystem
 * or not.
 *
 * @returns TRUE if path is on a remote filesystem
 */
gboolean
xmms_statfs_is_remote (const gchar *path)
{
#if defined(STATFS_LINUX) || defined(STATFS_BSD)
	struct statfs st;

	if (statfs (path, &st) == -1) {
		xmms_log_error ("Failed to run statfs, will not guess.");
		return FALSE;
	}
#endif

#if defined(STATFS_LINUX)
	if (st.f_type == 0xFF534D42 || /* cifs */
	    st.f_type == 0x6969 || /* nfs */
	    st.f_type == 0x517B) { /* smb */
		return TRUE;
	}
#elif defined(STATFS_BSD)
	if ((g_strcasecmp (st.f_fstypename, "nfs") == 0) ||
	    (g_strcasecmp (st.f_fstypename, "smb") == 0)) {
		return TRUE;
	}
#endif

	return FALSE;
}

