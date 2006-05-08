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

#ifndef __XMMS_PRIV_MAGIC_H__
#define __XMMS_PRIV_MAGIC_H__

#include "xmms/xmms_xformplugin.h"

typedef struct xmms_magic_checker_St {
	xmms_xform_t *xform;
	gchar *buf;
	guint alloc;
	guint read;
	guint offset;
} xmms_magic_checker_t;

void xmms_magic_tree_free (GNode *tree);

GNode *xmms_magic_match (xmms_magic_checker_t *c);
guint xmms_magic_complexity (const GList *magic);

#endif
