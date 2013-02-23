/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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




#ifndef __XMMS_MEDIAINFO_H__
#define __XMMS_MEDIAINFO_H__

#include "xmmspriv/xmms_medialib.h"

typedef struct xmms_mediainfo_reader_St xmms_mediainfo_reader_t;

xmms_mediainfo_reader_t * xmms_mediainfo_reader_start (xmms_medialib_t *medialib);
void xmms_mediainfo_reader_wakeup (xmms_mediainfo_reader_t *mr);

#endif /* __XMMS_MEDIAINFO_H__ */
