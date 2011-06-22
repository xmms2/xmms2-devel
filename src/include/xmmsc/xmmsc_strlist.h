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

#ifndef __XMMS_STRLIST_H__
#define __XMMS_STRLIST_H__

#include <stdarg.h>

char **xmms_valist_to_strlist (const char *first, va_list ap);
char **xmms_vargs_to_strlist (const char *first, ...);
int xmms_strlist_len (char **data);
void xmms_strlist_destroy (char **data);
char **xmms_strlist_prepend_copy (char **data, char *newstr);
char **xmms_strlist_copy (char **strlist);

#endif /* __XMMS_STRLIST_H__ */
