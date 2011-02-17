/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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

#include <sys/prctl.h>
#include <string.h>

#include "xmmspriv/xmms_thread_name.h"
#include "xmms/xmms_log.h"


void
xmms_set_thread_name (const gchar *name)
{
	g_return_if_fail (name != NULL && strlen (name) <= 15);

	if (prctl (PR_SET_NAME, (unsigned long) name, 0, 0, 0) != 0) {
		xmms_log_error ("Could not set thread name: %s", name);
	}
}
