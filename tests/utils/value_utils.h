/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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
#ifndef __VALUE_UTILS_H__
#define __VALUE_UTILS_H__

#include <xmmsc/xmmsv.h>

int xmmsv_compare (xmmsv_t *a, xmmsv_t *b);
int xmmsv_compare_unordered (xmmsv_t *a, xmmsv_t *b);
void xmmsv_dump_indented (xmmsv_t *value, int indent);
void xmmsv_dump (xmmsv_t *value);

#endif
