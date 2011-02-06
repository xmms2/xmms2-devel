/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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
#include "xmmspriv/xmms_fetch_spec.h"
#include "xmmspriv/xmms_fetch_info.h"
#include "xmms/xmms_log.h"
#include <string.h>

static int
metadata_value_from_string (const gchar *name)
{
	if (strcmp (name, "id") == 0) {
		return METADATA_ID;
	} else if (strcmp (name, "key") == 0) {
		return METADATA_KEY;
	} else if (strcmp (name, "value") == 0) {
		return METADATA_VALUE;
	} else if (strcmp (name, "source") == 0) {
		return METADATA_SOURCE;
	}

	/* TODO: implement error handling */

	return -1;
}

static int
aggregate_value_from_string (const gchar *name)
{
	if (strcmp (name, "first") == 0) {
		return AGGREGATE_FIRST;
	} else if (strcmp (name, "sum") == 0) {
		return AGGREGATE_SUM;
	} else if (strcmp (name, "max") == 0) {
		return AGGREGATE_MAX;
	} else if (strcmp (name, "min") == 0) {
		return AGGREGATE_MIN;
	} else if (strcmp (name, "list") == 0) {
		return AGGREGATE_LIST;
	} else if (strcmp (name, "set") == 0) {
		return AGGREGATE_SET;
	} else if (strcmp (name, "random") == 0) {
		return AGGREGATE_RANDOM;
	} else if (strcmp (name, "avg") == 0) {
		return AGGREGATE_AVG;
	}

	/* TODO: implement error handling */

	return -1;
}

static xmms_fetch_spec_t *
xmms_fetch_spec_new_metadata (xmmsv_t *fetch, xmms_fetch_info_t *info,
                              s4_sourcepref_t *prefs, xmms_error_t *err)
{
	xmms_fetch_spec_t *ret;
	s4_sourcepref_t *sp;
	const gchar *str;
	xmmsv_t *val;
	gint i, id_only = 0;

	ret = g_new0 (xmms_fetch_spec_t, 1);
	ret->type = FETCH_METADATA;

	if (xmmsv_dict_get (fetch, "get", &val)) {
		if (xmmsv_is_type (val, XMMSV_TYPE_LIST)) {
			for (i = 0; i < 4 && xmmsv_list_get_string (val, i, &str); i++) {
				ret->data.metadata.get[i] = metadata_value_from_string (str);
			}
		} else  if (xmmsv_get_string (val, &str)) {
			ret->data.metadata.get[0] = metadata_value_from_string (str);
			i = 1;
		}

		ret->data.metadata.get_size = i;
		if (i == 1 && ret->data.metadata.get[0] == METADATA_ID)
			id_only = 1;
	} else {
		ret->data.metadata.get_size = 1;
		ret->data.metadata.get[0] = METADATA_VALUE;
	}

	if (xmmsv_dict_get (fetch, "source-preference", &val)) {
		const char **strs = g_new (const char *, xmmsv_list_get_size (val) + 1);

		for (i = 0; xmmsv_list_get_string (val, i, &str); i++) {
			strs[i] = str;
		}
		strs[i] = NULL;
		sp = s4_sourcepref_create (strs);
		g_free (strs);
	} else {
		sp = s4_sourcepref_ref (prefs);
	}

	if (id_only) {
		ret->data.metadata.col_count = 1;
		ret->data.metadata.cols = g_new (int, ret->data.metadata.col_count);
		ret->data.metadata.cols[0] = 0;
	} else if (xmmsv_dict_get (fetch, "keys", &val)) {
		if (xmmsv_is_type (val, XMMSV_TYPE_LIST)) {
			ret->data.metadata.col_count = xmmsv_list_get_size (val);
			ret->data.metadata.cols = g_new (int, ret->data.metadata.col_count);
			for (i = 0; xmmsv_list_get_string (val, i, &str); i++) {
				ret->data.metadata.cols[i] = xmms_fetch_info_add_key (info, fetch, str, sp);
			}
		} else if (xmmsv_get_string (val, &str)) {
			ret->data.metadata.col_count = 1;
			ret->data.metadata.cols = g_new (int, 1);
			ret->data.metadata.cols[0] = xmms_fetch_info_add_key (info, fetch, str, sp);
		} else {
			/* TODO: whaaaaat? */
		}
	} else {
		/* TODO: What does this do? */
		ret->data.metadata.col_count = 1;
		ret->data.metadata.cols = g_new (int, ret->data.metadata.col_count);
		ret->data.metadata.cols[0] = xmms_fetch_info_add_key (info, fetch, NULL, sp);
	}


	if (!xmmsv_dict_entry_get_string (fetch, "aggregate", &str)) {
		/* Default to first as the aggregation function */
		str = "first";
	}

	ret->data.metadata.aggr_func = aggregate_value_from_string (str);

	s4_sourcepref_unref (sp);

	return ret;
}

/**
 * Decodes a cluster fetch specification from a dictionary.
 * The 'cluster-by' must be one of 'id', 'position' or 'value'. If set
 * to 'value', then an additional 'cluster-field' will be used to specify
 * which meta data attribute to cluster on.
 */
static xmms_fetch_spec_t *
xmms_fetch_spec_new_cluster (xmmsv_t *fetch, xmms_fetch_info_t *info,
                             s4_sourcepref_t *prefs, xmms_error_t *err)
{
	xmmsv_t *cluster_by, *cluster_field, *cluster_data;
	xmms_fetch_spec_t *data, *spec = NULL;
	const gchar *type = NULL;
	const gchar *field = NULL;

	if (!xmmsv_dict_get (fetch, "cluster-by", &cluster_by)) {
		cluster_by = xmmsv_new_string ("value");
		xmmsv_dict_set (fetch, "cluster-by", cluster_by);
	}

	if (!xmmsv_dict_entry_get_string (fetch, "cluster-by", &type)) {
		const gchar *message = "'cluster-by' must be a string.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	if (xmmsv_get_string (cluster_by, &type) && strcmp (type, "value") == 0) {
		if (!xmmsv_dict_entry_get_string (fetch, "cluster-field", &field)) {
			const gchar *message = "'cluster-field' must  if 'cluster-by' is 'value'.";
			xmms_error_set (err, XMMS_ERROR_INVAL, message);
			return NULL;
		}
	}

	if (!xmmsv_dict_get (fetch, "data", &cluster_data)) {
		const gchar *message = "Required field 'data' not set in cluster.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	data = xmms_fetch_spec_new (cluster_data, info, prefs, err);
	if (xmms_error_iserror (err)) {
		return NULL;
	}

	spec = g_new0 (xmms_fetch_spec_t, 1);
	spec->data.cluster.data = data;

	if (strcmp (type, "position") == 0) {
		spec->data.cluster.type = CLUSTER_BY_POSITION;
	} else if (strcmp (type, "id") == 0) {
		/* Media library id is always found at the first column */
		spec->data.cluster.column = 0;
		spec->data.cluster.type = CLUSTER_BY_ID;
	} else {
		xmmsv_dict_get (fetch, "cluster-field", &cluster_field);
		spec->data.cluster.column = xmms_fetch_info_add_key (info, cluster_field, field, prefs);
		spec->data.cluster.type = CLUSTER_BY_VALUE;
	}

	return spec;
}

static xmms_fetch_spec_t *
xmms_fetch_spec_new_cluster_list (xmmsv_t *fetch, xmms_fetch_info_t *info,
                                  s4_sourcepref_t *prefs, xmms_error_t *err)
{
	xmms_fetch_spec_t *spec;

	spec = xmms_fetch_spec_new_cluster (fetch, info, prefs, err);
	if (spec != NULL)
		spec->type = FETCH_CLUSTER_LIST;

	return spec;
}


static xmms_fetch_spec_t *
xmms_fetch_spec_new_cluster_dict (xmmsv_t *fetch, xmms_fetch_info_t *info,
                                  s4_sourcepref_t *prefs, xmms_error_t *err)
{
	xmms_fetch_spec_t *spec;

	spec = xmms_fetch_spec_new_cluster (fetch, info, prefs, err);
	if (spec != NULL)
		spec->type = FETCH_CLUSTER_DICT;

	return spec;
}


static xmms_fetch_spec_t *
xmms_fetch_spec_new_organize (xmmsv_t *fetch, xmms_fetch_info_t *info,
                              s4_sourcepref_t *prefs, xmms_error_t *err)
{
	xmms_fetch_spec_t *spec;
	xmmsv_dict_iter_t *it;
	xmmsv_t *org_data;
	gint org_idx;

	if (!xmmsv_dict_get (fetch, "data", &org_data)) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "Required field 'data' not set in organize.");
		return NULL;
	}

	if (xmmsv_get_type (org_data) != XMMSV_TYPE_DICT) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "Field 'data' in organize must be a dict.");
		return NULL;
	}

	spec = g_new0 (xmms_fetch_spec_t, 1);
	spec->type = FETCH_ORGANIZE;

	spec->data.organize.count = xmmsv_dict_get_size (org_data);
	spec->data.organize.keys = g_new0 (const char *, spec->data.organize.count);
	spec->data.organize.data = g_new0 (xmms_fetch_spec_t *, spec->data.organize.count);

	org_idx = 0;
	xmmsv_get_dict_iter (org_data, &it);
	while (xmmsv_dict_iter_valid (it)) {
		const gchar *str;
		xmmsv_t *entry;

		xmmsv_dict_iter_pair (it, &str, &entry);

		/* TODO: error handling when xmms_fetch_spec_new returns NULL */
		spec->data.organize.keys[org_idx] = str;
		spec->data.organize.data[org_idx] = xmms_fetch_spec_new (entry, info,
		                                                         prefs, err);

		org_idx++;
		xmmsv_dict_iter_next (it);
	}
	xmmsv_dict_iter_explicit_destroy (it);

	return spec;
}

static xmms_fetch_spec_t *
xmms_fetch_spec_new_count (xmmsv_t *fetch, xmms_fetch_info_t *info,
                           s4_sourcepref_t *prefs, xmms_error_t *err)
{
	xmms_fetch_spec_t *ret;

	ret = g_new0 (xmms_fetch_spec_t, 1);
	ret->type = FETCH_COUNT;

	return ret;
}



/**
 * Converts a fetch specification in xmmsv_t form into a
 * fetch_spec_t structure
 */
xmms_fetch_spec_t *
xmms_fetch_spec_new (xmmsv_t *fetch, xmms_fetch_info_t *info,
                     s4_sourcepref_t *prefs, xmms_error_t *err)
{
	const char *type;

	if (xmmsv_get_type (fetch) != XMMSV_TYPE_DICT) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "A fetch specification must be a dict.");
		return NULL;
	}

	if (!xmmsv_dict_entry_get_string (fetch, "type", &type)) {
		type = "metadata";
	}

	if (strcmp (type, "metadata") == 0) {
		return xmms_fetch_spec_new_metadata (fetch, info, prefs, err);
	} else if (strcmp (type, "cluster-list") == 0) {
		return xmms_fetch_spec_new_cluster_list (fetch, info, prefs, err);
	} else if (strcmp (type, "cluster-dict") == 0) {
		return xmms_fetch_spec_new_cluster_dict (fetch, info, prefs, err);
	} else if (strcmp (type, "organize") == 0) {
		return xmms_fetch_spec_new_organize (fetch, info, prefs, err);
	} else if (strcmp (type, "count") == 0) {
		return xmms_fetch_spec_new_count (fetch, info, prefs, err);
	}

	xmms_error_set (err, XMMS_ERROR_INVAL, "Unknown fetch type.");

	return NULL;
}


void
xmms_fetch_spec_free (xmms_fetch_spec_t *spec)
{
	int i;
	if (spec == NULL)
		return;

	switch (spec->type) {
	case FETCH_METADATA:
		g_free (spec->data.metadata.cols);
		break;
	case FETCH_CLUSTER_DICT:
	case FETCH_CLUSTER_LIST:
		xmms_fetch_spec_free (spec->data.cluster.data);
		break;
	case FETCH_ORGANIZE:
		for (i = 0; i < spec->data.organize.count; i++) {
			xmms_fetch_spec_free (spec->data.organize.data[i]);
		}

		g_free (spec->data.organize.keys);
		g_free (spec->data.organize.data);
		break;
	case FETCH_COUNT: /* Nothing to free */
		break;
	}

	g_free (spec);
}
