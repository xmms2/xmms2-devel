/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

/* Have thread safe localtime when POSIX localtime_r isn't available */

#include <xmmspriv/xmms_localtime.h>

G_LOCK_DEFINE_STATIC (mutex);

gboolean
xmms_localtime (const time_t *tt, struct tm *res)
{
	struct tm *ret = NULL;

	G_LOCK (mutex);
	if ((ret = localtime (tt))) {
		memcpy (res, ret, sizeof (struct tm));
		ret = res;
	}
	G_UNLOCK (mutex);

	if (ret) {
		return TRUE;
	} else {
		return FALSE;
	}
}
