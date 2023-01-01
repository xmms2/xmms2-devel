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


#ifndef __XMMSV_GENERAL_H__
#define __XMMSV_GENERAL_H__

#include <stdarg.h>
#include <stdint.h>

#include <xmmsc/xmmsc_compiler.h>
#include <xmmsc/xmmsc_stdint.h>
#include <xmmsc/xmmsc_idnumbers.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup ValueType The type xmmsv_t
 * @{
 */

typedef enum {
	XMMSV_TYPE_NONE,
	XMMSV_TYPE_ERROR,
	XMMSV_TYPE_INT64,
	XMMSV_TYPE_STRING,
	XMMSV_TYPE_COLL,
	XMMSV_TYPE_BIN,
	XMMSV_TYPE_LIST,
	XMMSV_TYPE_DICT,
	XMMSV_TYPE_BITBUFFER,
	XMMSV_TYPE_FLOAT,
	XMMSV_TYPE_END
} xmmsv_type_t;

#define XMMSV_TYPE_INT32 XMMSV_TYPE_INT64

typedef struct xmmsv_St xmmsv_t;

xmmsv_t *xmmsv_new_none (void) XMMS_PUBLIC;
xmmsv_t *xmmsv_new_error (const char *errstr) XMMS_PUBLIC; /* FIXME: err id? */
xmmsv_t *xmmsv_new_int (int64_t i) XMMS_PUBLIC;
xmmsv_t *xmmsv_new_float (float i) XMMS_PUBLIC;
xmmsv_t *xmmsv_new_string (const char *s) XMMS_PUBLIC;
xmmsv_t *xmmsv_new_bin (const unsigned char *data, unsigned int len) XMMS_PUBLIC;

xmmsv_t *xmmsv_copy (xmmsv_t *val) XMMS_PUBLIC;
xmmsv_t *xmmsv_coll_copy (xmmsv_t *orig_coll) XMMS_PUBLIC XMMS_DEPRECATED;

xmmsv_t *xmmsv_ref (xmmsv_t *val) XMMS_PUBLIC;
void xmmsv_unref (xmmsv_t *val) XMMS_PUBLIC;

xmmsv_type_t xmmsv_get_type (const xmmsv_t *val) XMMS_PUBLIC;
int xmmsv_is_type (const xmmsv_t *val, xmmsv_type_t t) XMMS_PUBLIC;

int xmmsv_get_error (const xmmsv_t *val, const char **r) XMMS_PUBLIC;
int xmmsv_get_int32 (const xmmsv_t *val, int32_t *r) XMMS_PUBLIC;
int xmmsv_get_int64 (const xmmsv_t *val, int64_t *r) XMMS_PUBLIC;
int xmmsv_get_float (const xmmsv_t *val, float *r) XMMS_PUBLIC;
int xmmsv_get_string (const xmmsv_t *val, const char **r) XMMS_PUBLIC;
int xmmsv_get_coll (const xmmsv_t *val, xmmsv_t **coll) XMMS_PUBLIC XMMS_DEPRECATED;
int xmmsv_get_bin (const xmmsv_t *val, const unsigned char **r, unsigned int *rlen) XMMS_PUBLIC;


/* legacy aliases */
int xmmsv_is_error (const xmmsv_t *val) XMMS_PUBLIC;

#if XMMSV_USE_INT64 == 1
#define xmmsv_get_int xmmsv_get_int64
#else
#define xmmsv_get_int xmmsv_get_int32
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif
