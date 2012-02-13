/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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

#ifndef __APE_H__
#define __APE_H__

#include "xmms/xmms_log.h"
#include "xmms/xmms_xformplugin.h"

typedef struct xmms_apetag_St xmms_apetag_t;

xmms_apetag_t *xmms_apetag_init (xmms_xform_t *xform);

gboolean xmms_apetag_read (xmms_apetag_t *tag);
void xmms_apetag_destroy (xmms_apetag_t *tag);

const gchar *xmms_apetag_lookup_str (xmms_apetag_t *tag, const gchar *key);
gint xmms_apetag_lookup_int (xmms_apetag_t *tag, const gchar *key);


#endif
