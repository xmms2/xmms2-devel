/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2005-2006 Daniel Svensson, <daniel@nittionio.nu> 
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

#define APE_HEADER_SIZE 32

gint xmms_ape_get_size (gchar *buff, gint len);
gchar *xmms_ape_get_text (gchar *key, gchar *buff, gint len);
gboolean xmms_ape_tag_is_valid (gchar *buff, gint len);


#endif
