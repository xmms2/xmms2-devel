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

#ifndef __XMMSV_INTERNAL_H__
#define __XMMSV_INTERNAL_H__

#include "xmmsc/xmmsv.h"
#include "xmmsc/xmmsc_stdbool.h"

typedef struct xmmsv_dict_St xmmsv_dict_t;
typedef struct xmmsv_list_St xmmsv_list_t;

struct xmmsv_St {
	union {
		char *error;
		int32_t int32;
		char *string;
		xmmsv_coll_t *coll;

		struct {
			unsigned char *data;
			uint32_t len;
		} bin;

		xmmsv_list_t *list;
		xmmsv_dict_t *dict;


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

xmmsv_t *xmmsv_new (xmmsv_type_t type);

#endif
