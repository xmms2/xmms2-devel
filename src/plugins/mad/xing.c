/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundstr�m, Anders Gustafsson
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
 * Xing-header parser.
 */

#include "xmms/xmms.h"
#include "xmms/util.h"

#include "xing.h"

struct xmms_xing_St {
	gint flags;
	guint frames;
	guint bytes;
	guint toc[XMMS_XING_TOC_SIZE];
};

gboolean
xmms_xing_has_flag (xmms_xing_t *xing, xmms_xing_flags_t flag)
{
	return xing->flags & flag;
}

guint
xmms_xing_get_frames (xmms_xing_t *xing)
{
	return xing->frames;
}

guint
xmms_xing_get_bytes (xmms_xing_t *xing)
{
	return xing->bytes;
}

guint
xmms_xing_get_toc (xmms_xing_t *xing, gint index)
{
	g_return_val_if_fail (0 <= index && index < 100, -1);

	return xing->toc[index];
}

xmms_xing_t *
xmms_xing_parse (struct mad_bitptr ptr)
{
	xmms_xing_t *xing;
	guint32 xing_magic;


	xing_magic = mad_bit_read (&ptr, 4*8);

	if (xing_magic != 0x58696e67) { /* "Xing" */
		return NULL;
	}

	xing = g_new0 (xmms_xing_t, 1);

	g_return_val_if_fail (xing, NULL);

	xing->flags = mad_bit_read (&ptr, 32);

	if (xmms_xing_has_flag (xing, XMMS_XING_FRAMES))
		xing->frames = mad_bit_read (&ptr, 32);

	if (xmms_xing_has_flag (xing, XMMS_XING_BYTES))
		xing->bytes = mad_bit_read (&ptr, 32);

	if (xmms_xing_has_flag (xing, XMMS_XING_TOC)) {
		gint i;
		for (i = 0; i < 100; i++)
			xing->toc[i] = mad_bit_read (&ptr, 8);
	}

	if (xmms_xing_has_flag (xing, XMMS_XING_SCALE)) {
		/* just move the pointer forward */
		mad_bit_read (&ptr, 32);
	}

	return xing;
}

