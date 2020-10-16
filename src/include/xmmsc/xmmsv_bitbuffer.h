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


#ifndef __XMMSV_BITBUFFER_H__
#define __XMMSV_BITBUFFER_H__

#include <xmmsc/xmmsv_general.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup BitbufferType Bitbuffer
 * @ingroup ValueType
 * @{
 */

/* Bitbuffer */
xmmsv_t *xmmsv_bitbuffer_new_ro (const unsigned char *v, int len) XMMS_PUBLIC XMMS_DEPRECATED;
xmmsv_t *xmmsv_bitbuffer_new (void) XMMS_PUBLIC XMMS_DEPRECATED;
xmmsv_t *xmmsv_new_bitbuffer_ro (const unsigned char *v, int len) XMMS_PUBLIC;
xmmsv_t *xmmsv_new_bitbuffer (void) XMMS_PUBLIC;
int xmmsv_bitbuffer_get_bits (xmmsv_t *v, int bits, int64_t *res) XMMS_PUBLIC;
int xmmsv_bitbuffer_get_data (xmmsv_t *v, unsigned char *b, int len) XMMS_PUBLIC;
int xmmsv_bitbuffer_put_bits (xmmsv_t *v, int bits, int64_t d) XMMS_PUBLIC;
int xmmsv_bitbuffer_put_bits_at (xmmsv_t *v, int bits, int64_t d, int offset) XMMS_PUBLIC;
int xmmsv_bitbuffer_put_data (xmmsv_t *v, const unsigned char *b, int len) XMMS_PUBLIC;
int xmmsv_bitbuffer_align (xmmsv_t *v) XMMS_PUBLIC;
int xmmsv_bitbuffer_goto (xmmsv_t *v, int pos) XMMS_PUBLIC;
int xmmsv_bitbuffer_pos (xmmsv_t *v) XMMS_PUBLIC;
int xmmsv_bitbuffer_rewind (xmmsv_t *v) XMMS_PUBLIC;
int xmmsv_bitbuffer_end (xmmsv_t *v) XMMS_PUBLIC;
int xmmsv_bitbuffer_len (xmmsv_t *v) XMMS_PUBLIC;
const unsigned char *xmmsv_bitbuffer_buffer (xmmsv_t *v) XMMS_PUBLIC;
int xmmsv_get_bitbuffer (const xmmsv_t *val, const unsigned char **r, unsigned int *rlen) XMMS_PUBLIC;
int xmmsv_bitbuffer_serialize_value (xmmsv_t *bb, xmmsv_t *v);
int xmmsv_bitbuffer_deserialize_value (xmmsv_t *bb, xmmsv_t **val);

/** @} */

#ifdef __cplusplus
}
#endif

#endif
