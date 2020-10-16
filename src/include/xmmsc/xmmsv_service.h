/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

#ifndef __XMMSV_SERVICE_H__
#define __XMMSV_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

xmmsv_t *xmmsv_sc_argument_new (const char *name, const char *docstring, xmmsv_type_t type, xmmsv_t *default_value) XMMS_PUBLIC;

const char * xmmsv_sc_argument_get_name (xmmsv_t *arg) XMMS_PUBLIC;
const char * xmmsv_sc_argument_get_docstring (xmmsv_t *arg) XMMS_PUBLIC;
int64_t xmmsv_sc_argument_get_type (xmmsv_t *arg) XMMS_PUBLIC;
xmmsv_t * xmmsv_sc_argument_get_default_value (xmmsv_t *arg) XMMS_PUBLIC;

#ifdef __cplusplus
}
#endif

#endif
