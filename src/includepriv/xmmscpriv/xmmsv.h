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

#ifndef __XMMSV_INTERNAL_H__
#define __XMMSV_INTERNAL_H__

#include <stdint.h>

#include <xmmsc/xmmsv.h>
#include <xmmsc/xmmsc_stdbool.h>

typedef struct xmmsv_dict_internal_St xmmsv_dict_internal_t;
typedef struct xmmsv_list_internal_St xmmsv_list_internal_t;
typedef struct xmmsv_coll_internal_St xmmsv_coll_internal_t;

struct xmmsv_St {
	union {
		char *error;
		int64_t int64;
		float flt32;
		char *string;
		xmmsv_coll_internal_t *coll;
		xmmsv_list_internal_t *list;
		xmmsv_dict_internal_t *dict;

		struct {
			unsigned char *data;
			uint32_t len;
		} bin;

		struct {
			bool ro;
			unsigned char *buf;
			int alloclen; /* in bits */
			int len; /* in bits */
			int pos; /* in bits */
		} bit;
	} value;
	xmmsv_type_t type;

	int ref;  /* refcounting */
};

xmmsv_t *_xmmsv_new (xmmsv_type_t type);

void _xmmsv_list_free (xmmsv_list_internal_t *dict);
void _xmmsv_dict_free (xmmsv_dict_internal_t *dict);
void _xmmsv_coll_free (xmmsv_coll_internal_t *coll);

#endif
