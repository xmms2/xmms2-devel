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

#ifndef __XMMS_PRIV_FETCH_SPEC_H__
#define __XMMS_PRIV_FETCH_SPEC_H__

#include <glib.h>
#include <xmmspriv/xmms_fetch_info.h>
#include <xmmsc/xmmsv_coll.h>
#include <xmmsc/xmmsv.h>
#include <xmms/xmms_error.h>

typedef enum {
	AGGREGATE_FIRST,
	AGGREGATE_SUM,
	AGGREGATE_MAX,
	AGGREGATE_MIN,
	AGGREGATE_SET,
	AGGREGATE_LIST,
	AGGREGATE_RANDOM,
	AGGREGATE_AVG,
	AGGREGATE_END
} aggregate_function_t;

typedef struct xmms_fetch_spec_St xmms_fetch_spec_t;

struct xmms_fetch_spec_St {
	enum {
		FETCH_CLUSTER_LIST,
		FETCH_CLUSTER_DICT,
		FETCH_ORGANIZE,
		FETCH_METADATA,
		FETCH_COUNT,
		FETCH_END
	} type;
	union {
		struct {
			enum {
				CLUSTER_BY_ID,
				CLUSTER_BY_POSITION,
				CLUSTER_BY_VALUE,
				CLUSTER_BY_END
			} type;
			int column;
			const gchar *fallback;
			xmms_fetch_spec_t *data;
		} cluster;
		struct {
			enum {
				METADATA_ID,
				METADATA_KEY,
				METADATA_VALUE,
				METADATA_SOURCE,
				METADATA_END
			} get[METADATA_END];
			int get_size;
			int col_count;
			int *cols;
			aggregate_function_t aggr_func;
		} metadata;
		struct {
			int count;
			const char **keys;
			xmms_fetch_spec_t **data;
		} organize;
	} data;
};


xmms_fetch_spec_t *xmms_fetch_spec_new (xmmsv_t *fetch, xmms_fetch_info_t *info, s4_sourcepref_t *prefs, xmms_error_t *err);
void xmms_fetch_spec_free (xmms_fetch_spec_t *spec);

#endif
