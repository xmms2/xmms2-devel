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


#ifndef __XMMSV_UTIL_H__
#define __XMMSV_UTIL_H__

#include "xmmsc/xmmsv_general.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup UtilValueType Utilities
 * @ingroup ValueType
 * @{
 */

#define xmmsv_check_type(type) ((type) > XMMSV_TYPE_NONE && (type) < XMMSV_TYPE_END)

xmmsv_t *xmmsv_decode_url (const xmmsv_t *url);

int xmmsv_utf8_validate (const char *str);

xmmsv_t *xmmsv_make_stringlist (char *array[], int num);

xmmsv_t *xmmsv_propdict_to_dict (xmmsv_t *propdict, const char **src_prefs);

int xmmsv_dict_format (char *target, int len, const char *fmt, xmmsv_t *val);

xmmsv_t *xmmsv_serialize (xmmsv_t *v);
xmmsv_t *xmmsv_deserialize (xmmsv_t *v);

/** @} */

#ifdef __cplusplus
}
#endif

#endif
