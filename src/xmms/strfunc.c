/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#include "xmms/xmms_strfunc.h"
#include <glib.h>

/**
 * strnlen implementation
 * @returns the len of str or max_len if the string is longer
 */
gsize
xmms_strnlen (const gchar *str, gsize max_len)
{
	gsize ret = 0;
	while (max_len > 0) {
		if (str[ret] == '\0')
			break;
		ret ++;
		max_len --;
	}
	return ret;
}

