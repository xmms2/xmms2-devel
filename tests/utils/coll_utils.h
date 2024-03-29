/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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
#ifndef __COLL_UTILS_H__
#define __COLL_UTILS_H__

#include <xmmsc/xmmsv_coll.h>

xmmsv_t *xmmsv_coll_from_string (const char *data);
xmmsv_t *xmmsv_coll_from_dict (xmmsv_t *data);
int xmmsv_coll_compare (xmmsv_t *a, xmmsv_t *b);
void xmmsv_coll_dump_indented (xmmsv_t *coll, int indent);
void xmmsv_coll_dump (xmmsv_t *coll);

#endif
