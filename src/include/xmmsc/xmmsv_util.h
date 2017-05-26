/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#include <xmmsc/xmmsv_general.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup UtilValueType Utilities
 * @ingroup ValueType
 * @{
 */

#define xmmsv_check_type(type) ((type) > XMMSV_TYPE_NONE && (type) < XMMSV_TYPE_END)

char *xmmsv_encode_url (const char *url) XMMS_PUBLIC;
char *xmmsv_encode_url_full (const char *url, xmmsv_t *args) XMMS_PUBLIC;
xmmsv_t *xmmsv_decode_url (const xmmsv_t *url) XMMS_PUBLIC;

int xmmsv_utf8_validate (const char *str) XMMS_PUBLIC;

xmmsv_t *xmmsv_propdict_to_dict (xmmsv_t *propdict, const char **src_prefs) XMMS_PUBLIC;

int xmmsv_dict_format (char *target, int len, const char *fmt, xmmsv_t *val) XMMS_PUBLIC;

xmmsv_t *xmmsv_serialize (xmmsv_t *v) XMMS_PUBLIC;
xmmsv_t *xmmsv_deserialize (xmmsv_t *v) XMMS_PUBLIC;

/** @} */

#ifdef __cplusplus
}
#endif

#endif
