/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
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


#ifndef __XMMS_ERROR_XMMS_H__
#define __XMMS_ERROR_XMMS_H__

typedef enum {
	XMMS_ERROR_NONE=0,
	XMMS_ERROR_GENERIC,
	XMMS_ERROR_OOM,
	XMMS_ERROR_PERMISSION,
	XMMS_ERROR_NOENT,
	XMMS_ERROR_INVAL,
	XMMS_ERROR_NO_SAUSAGE,
	XMMS_ERROR_COUNT /* must be last */
} xmms_error_code_t;

#endif
