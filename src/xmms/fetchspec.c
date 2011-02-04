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
get_cluster_column (xmms_fetch_info_t *info, xmmsv_t *val,
                    const char *str, s4_sourcepref_t *prefs)
{
	/* TODO: this is a hack, _row o_O */
	if (strcmp (str, "_row") == 0)
		return -1;
	return xmms_fetch_info_add_key (info, val, str, prefs);
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
				if (strcmp (str, "id") == 0) {
					ret->data.metadata.get[i] = METADATA_ID;
				} else if (strcmp (str, "key") == 0) {
					ret->data.metadata.get[i] = METADATA_KEY;
				} else if (strcmp (str, "value") == 0) {
					ret->data.metadata.get[i] = METADATA_VALUE;
				} else if (strcmp (str, "source") == 0) {
					ret->data.metadata.get[i] = METADATA_SOURCE;
				}
			}
		} else  if (xmmsv_get_string (val, &str)) {
			if (strcmp (str, "id") == 0) {
				ret->data.metadata.get[0] = METADATA_ID;
			} else if (strcmp (str, "key") == 0) {
				ret->data.metadata.get[0] = METADATA_KEY;
			} else if (strcmp (str, "value") == 0) {
				ret->data.metadata.get[0] = METADATA_VALUE;
			} else if (strcmp (str, "source") == 0) {
				ret->data.metadata.get[0] = METADATA_SOURCE;
			}
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
		}
	} else {
		ret->data.metadata.col_count = 1;
		ret->data.metadata.cols = g_new (int, ret->data.metadata.col_count);
		ret->data.metadata.cols[0] = xmms_fetch_info_add_key (info, fetch, NULL, sp);
	}
	if (!xmmsv_dict_entry_get_string (fetch, "aggregate", &str)) {
		/* Default to first as the aggregation function */
		str = "first";
	}
	if (strcmp (str, "first") == 0) {
		ret->data.metadata.aggr_func = AGGREGATE_FIRST;
	} else if (strcmp (str, "sum") == 0) {
		ret->data.metadata.aggr_func = AGGREGATE_SUM;
	} else if (strcmp (str, "max") == 0) {
		ret->data.metadata.aggr_func = AGGREGATE_MAX;
	} else if (strcmp (str, "min") == 0) {
		ret->data.metadata.aggr_func = AGGREGATE_MIN;
	} else if (strcmp (str, "list") == 0) {
		ret->data.metadata.aggr_func = AGGREGATE_LIST;
	} else if (strcmp (str, "random") == 0) {
		ret->data.metadata.aggr_func = AGGREGATE_RANDOM;
	} else if (strcmp (str, "avg") == 0) {
		ret->data.metadata.aggr_func = AGGREGATE_AVG;
	} else { /* Unknown aggregation function */
		xmms_error_set (err, XMMS_ERROR_INVAL, "Unknown aggregation function.");
	}

	s4_sourcepref_unref (sp);

	return ret;
}


static xmms_fetch_spec_t *
xmms_fetch_spec_new_cluster (xmmsv_t *fetch, xmms_fetch_info_t *info,
                             s4_sourcepref_t *prefs, xmms_error_t *err)
{
	xmmsv_t *cluster_by, *cluster_data;
	xmms_fetch_spec_t *spec;
	const gchar *str;
	gint i;

	if (!xmmsv_dict_get (fetch, "cluster-by", &cluster_by)) {
		const gchar *message = "Required field 'cluster-by' not set in cluster.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	if (!xmmsv_is_type (cluster_by, XMMSV_TYPE_LIST) &&
	    !xmmsv_is_type (cluster_by, XMMSV_TYPE_STRING)) {
		const gchar *message = "Field 'cluster-by' in cluster must be a list or string.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	if (!xmmsv_dict_get (fetch, "data", &cluster_data)) {
		const gchar *message = "Required field 'data' not set in cluster.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	if (!xmmsv_is_type (cluster_data, XMMSV_TYPE_DICT)) {
		const gchar *message = "Field 'data' in cluster must be a dict.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	spec = g_new0 (xmms_fetch_spec_t, 1);

	if (xmmsv_is_type (cluster_by, XMMSV_TYPE_LIST)) {
		spec->data.cluster.cluster_count = xmmsv_list_get_size (cluster_by);
		spec->data.cluster.cols = g_new0 (int, spec->data.cluster.cluster_count);
		for (i = 0; xmmsv_list_get_string (cluster_by, i, &str); i++) {
			spec->data.cluster.cols[i] = get_cluster_column (info, cluster_by, str, prefs);
		}
	} else {
		/* TODO: Should this really be allowed? Will be abstracted in bindings anyway */
		xmmsv_get_string (cluster_by, &str);
		spec->data.cluster.cluster_count = 1;
		spec->data.cluster.cols = g_new0 (int, 1);
		spec->data.cluster.cols[0] = get_cluster_column (info, cluster_by, str, prefs);
	}

	spec->data.cluster.data = xmms_fetch_spec_new (cluster_data, info, prefs, err);

	return spec;
}

static xmms_fetch_spec_t *
xmms_fetch_spec_new_cluster_list (xmmsv_t *fetch, xmms_fetch_info_t *info,
                                  s4_sourcepref_t *prefs, xmms_error_t *err)
{
	xmms_fetch_spec_t *ret;

	ret = xmms_fetch_spec_new_cluster (fetch, info, prefs, err);
	ret->type = FETCH_CLUSTER_LIST;

	return ret;
}


static xmms_fetch_spec_t *
xmms_fetch_spec_new_cluster_dict (xmmsv_t *fetch, xmms_fetch_info_t *info,
                                  s4_sourcepref_t *prefs, xmms_error_t *err)
{
	xmms_fetch_spec_t *ret;

	ret = xmms_fetch_spec_new_cluster (fetch, info, prefs, err);
	ret->type = FETCH_CLUSTER_DICT;

	return ret;
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
	spec->data.organize.keys = g_new0 (char *, spec->data.organize.count);
	spec->data.organize.data = g_new0 (xmms_fetch_spec_t, spec->data.organize.count);

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
		g_free (spec->data.cluster.cols);
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
