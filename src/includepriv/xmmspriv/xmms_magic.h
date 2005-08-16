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

#ifndef __XMMS_PRIV_MAGIC_H__
#define __XMMS_PRIV_MAGIC_H__

#include "xmms/xmms_transport.h"

typedef struct xmms_magic_checker_St {
	xmms_transport_t *transport;
	gchar *buf;
	guint alloc;
	guint read;
} xmms_magic_checker_t;

void xmms_magic_tree_free (GNode *tree);

GNode *xmms_magic_add (GNode *tree, const gchar *s, GNode *prev_node);
GNode *xmms_magic_match (xmms_magic_checker_t *c, const GList *magic);
guint xmms_magic_complexity (const GList *magic);

#endif
