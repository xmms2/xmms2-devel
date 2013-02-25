/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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
 *  Linux statfs
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib.h>


#include <sys/vfs.h>

#include <xmms/xmms_log.h>
#include <xmmspriv/xmms_statfs.h>

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
	struct statfs st;

	if (statfs (path, &st) == -1) {
		xmms_log_error ("Failed to run statfs, will not guess.");
		return FALSE;
	}

	if (st.f_type == 0xFF534D42 || /* cifs */
	    st.f_type == 0x6969 || /* nfs */
	    st.f_type == 0x517B) { /* smb */
		return TRUE;
	}

	return FALSE;
}
