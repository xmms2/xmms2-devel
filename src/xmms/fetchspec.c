/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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
#include <xmmspriv/xmms_fetch_spec.h>
#include <xmmspriv/xmms_fetch_info.h>
#include <xmms/xmms_log.h>
#include <string.h>
#include <stdlib.h>

static gboolean
metadata_value_from_string (const gchar *name, guint32 *value)
{
	if (name == NULL) {
		return FALSE;
	} else if (strcmp (name, "id") == 0) {
		*value = METADATA_ID;
	} else if (strcmp (name, "field") == 0) {
		*value = METADATA_KEY;
	} else if (strcmp (name, "value") == 0) {
		*value = METADATA_VALUE;
	} else if (strcmp (name, "source") == 0) {
		*value = METADATA_SOURCE;
	} else {
		return FALSE;
	}

	return TRUE;
}

static gboolean
aggregate_value_from_string (const gchar *name, guint32 *value)
{
	if (name == NULL) {
		return FALSE;
	} else if (strcmp (name, "first") == 0) {
		*value = AGGREGATE_FIRST;
	} else if (strcmp (name, "sum") == 0) {
		*value = AGGREGATE_SUM;
	} else if (strcmp (name, "max") == 0) {
		*value = AGGREGATE_MAX;
	} else if (strcmp (name, "min") == 0) {
		*value = AGGREGATE_MIN;
	} else if (strcmp (name, "list") == 0) {
		*value = AGGREGATE_LIST;
	} else if (strcmp (name, "set") == 0) {
		*value = AGGREGATE_SET;
	} else if (strcmp (name, "random") == 0) {
		*value = AGGREGATE_RANDOM;
	} else if (strcmp (name, "avg") == 0) {
		*value = AGGREGATE_AVG;
	} else {
		return FALSE;
	}

	return TRUE;
}

/**
 * Sanitize the 'get' property of a 'metadata' fetch specification.
 */
static xmmsv_t *
normalize_metadata_get (xmmsv_t *fetch, xmms_error_t *err)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *get, *list;
	guint32 values;

	if (!xmmsv_dict_get (fetch, "get", &get) ||
	    xmmsv_get_type (get) != XMMSV_TYPE_LIST ||
	    xmmsv_list_get_size (get) < 1) {
		const gchar *message = "'get' must be a non-empty list of strings.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	list = xmmsv_new_list ();
	values = 0;

	/* Scan for duplicates or invalid values */
	xmmsv_get_list_iter (get, &it);
	while (xmmsv_list_iter_valid (it)) {
		const gchar *value = NULL;
		guint32 get_as_int, mask;

		xmmsv_list_iter_entry_string (it, &value);

		if (!metadata_value_from_string (value, &get_as_int)) {
			const gchar *message = "'get' entries must be 'id', 'field', 'value' or 'source'.";
			xmms_error_set (err, XMMS_ERROR_INVAL, message);
			xmmsv_unref (list);
			return NULL;
		}

		mask = 1 << (get_as_int + 1);
		if (values & mask) {
			const gchar *message = "'get' entries must be unique.";
			xmms_error_set (err, XMMS_ERROR_INVAL, message);
			xmmsv_unref (list);
			return NULL;
		}

		values |= mask;

		xmmsv_list_append_int (list, get_as_int);
		xmmsv_list_iter_next (it);
	}

	return list;
}

static xmmsv_t *
normalize_metadata_fields (xmmsv_t *fetch, xmms_error_t *err)
{
	gpointer SENTINEL = GINT_TO_POINTER (0x31337);
	GHashTable *table;

	xmmsv_list_iter_t *it;
	xmmsv_t *fields;

	if (!xmmsv_dict_get (fetch, "fields", &fields)) {
		/* No fields means that we should fetch all fields */
		return NULL;
	}

	if (xmmsv_get_type (fields) != XMMSV_TYPE_LIST) {
		const gchar *message = "'fields' must be a list of strings.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	if (xmmsv_list_get_size (fields) < 1) {
		/* No fields means that we should fetch all fields */
		return NULL;
	}

	table = g_hash_table_new (g_str_hash, g_str_equal);

	xmmsv_get_list_iter (fields, &it);
	while (xmmsv_list_iter_valid (it)) {
		const gchar *value = NULL;

		if (!xmmsv_list_iter_entry_string (it, &value)) {
			const gchar *message = "'fields' entries must be of string type.";
			xmms_error_set (err, XMMS_ERROR_INVAL, message);
			g_hash_table_unref (table);
			return NULL;
		}

		if (g_hash_table_lookup (table, (gpointer) value) == SENTINEL) {
			const gchar *message = "'fields' entries must be unique.";
			xmms_error_set (err, XMMS_ERROR_INVAL, message);
			g_hash_table_unref (table);
			return NULL;
		}

		g_hash_table_insert (table, (gpointer) value, SENTINEL);

		xmmsv_list_iter_next (it);
	}

	g_hash_table_unref (table);

	return fields;
}


static s4_sourcepref_t *
normalize_source_preferences (xmmsv_t *fetch, s4_sourcepref_t *prefs, xmms_error_t *err)
{
	s4_sourcepref_t *sp;
	xmmsv_list_iter_t *it;
	const char **strv;
	const gchar *str;
	xmmsv_t *list;
	gint length, idx;

	if (!xmmsv_dict_get (fetch, "source-preference", &list)) {
		return s4_sourcepref_ref (prefs);
	}

	if (xmmsv_get_type (list) != XMMSV_TYPE_LIST) {
		const gchar *message = "'source-preference' must be a list of strings.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	length = xmmsv_list_get_size (list);
	if (length == 0) {
		return s4_sourcepref_ref (prefs);
	}

	strv = g_new0 (const char *, length + 1);

	idx = 0;

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_valid (it)) {
		if (!xmmsv_list_iter_entry_string (it, &str)) {
			const gchar *message = "'source-preference' must be a list of strings.";
			xmms_error_set (err, XMMS_ERROR_INVAL, message);
			g_free (strv);
			return NULL;
		}

		strv[idx++] = str;

		xmmsv_list_iter_next (it);
	}

	sp = s4_sourcepref_create (strv);
	g_free (strv);

	return sp;
}

static gint
normalize_aggregate_function (xmmsv_t *fetch, xmms_error_t *err)
{
	const gchar *name;
	guint32 aggregate;

	if (xmmsv_dict_entry_get_type (fetch, "aggregate") == XMMSV_TYPE_NONE) {
		xmmsv_dict_set_string (fetch, "aggregate", "first");
	}

	/* Default to first as the aggregation function */
	if (!xmmsv_dict_entry_get_string (fetch, "aggregate", &name)) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "'aggregate' must be a string.");
		return -1;
	}

	if (!aggregate_value_from_string (name, &aggregate)) {
		const gchar *message = "'aggregate' must be 'first', 'sum', 'max', 'min', 'list', 'set', 'random', or 'avg'";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return -1;
	}

	return aggregate;
}


static xmms_fetch_spec_t *
xmms_fetch_spec_new_metadata (xmmsv_t *fetch, xmms_fetch_info_t *info,
                              s4_sourcepref_t *prefs, xmms_error_t *err)
{
	xmms_fetch_spec_t *ret = NULL;
	s4_sourcepref_t *sp;
	const gchar *key;
	xmmsv_t *gets, *fields;
	gint i, size, aggregate, get;

	aggregate = normalize_aggregate_function (fetch, err);
	if (xmms_error_iserror (err)) {
		return NULL;
	}

	fields = normalize_metadata_fields (fetch, err);
	if (xmms_error_iserror (err)) {
		return NULL;
	}

	gets = normalize_metadata_get (fetch, err);
	if (xmms_error_iserror (err)) {
		return NULL;
	}

	sp = normalize_source_preferences (fetch, prefs, err);
	if (xmms_error_iserror (err)) {
		xmmsv_unref (gets);
		return NULL;
	}

	ret = g_new0 (xmms_fetch_spec_t, 1);
	ret->type = FETCH_METADATA;
	ret->data.metadata.aggr_func = aggregate;

	for (i = 0; i < 4 && xmmsv_list_get_int (gets, i, &get); i++) {
		ret->data.metadata.get[i] = get;
	}
	ret->data.metadata.get_size = i;

	if (fields != NULL) {
		size = xmmsv_list_get_size (fields);
		ret->data.metadata.col_count = size;
		ret->data.metadata.cols = g_new (gint32, size);
		for (i = 0; xmmsv_list_get_string (fields, i, &key); i++) {
			ret->data.metadata.cols[i] = xmms_fetch_info_add_key (info, fetch, key, sp);
		}
	} else {
		/* No fields requested, fetching all available */
		ret->data.metadata.col_count = 1;
		ret->data.metadata.cols = g_new0 (gint32, 1);
		ret->data.metadata.cols[0] = xmms_fetch_info_add_key (info, fetch, NULL, sp);
	}

	s4_sourcepref_unref (sp);
	xmmsv_unref (gets);

	return ret;
}

static gboolean
cluster_by_from_string (const gchar *name, gint *value)
{
	if (name == NULL) {
		return FALSE;
	} else if (strcmp (name, "id") == 0) {
		*value = CLUSTER_BY_ID;
	} else if (strcmp (name, "position") == 0) {
		*value = CLUSTER_BY_POSITION;
	} else if (strcmp (name, "value") == 0) {
		*value = CLUSTER_BY_VALUE;
	} else {
		return FALSE;
	}

	return TRUE;
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
	s4_sourcepref_t *sp;
	const gchar *value = NULL;
	const gchar *field = NULL;
	const gchar *fallback = NULL;
	gint cluster_type;

	if (!xmmsv_dict_get (fetch, "cluster-by", &cluster_by)) {
		cluster_by = xmmsv_new_string ("value");
		xmmsv_dict_set (fetch, "cluster-by", cluster_by);
		xmmsv_unref (cluster_by);
	}

	if (!xmmsv_dict_entry_get_string (fetch, "cluster-by", &value)) {
		const gchar *message = "'cluster-by' must be a string.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	xmmsv_get_string (cluster_by, &value);

	if (!cluster_by_from_string (value, &cluster_type)) {
		const gchar *message = "'cluster-by' must be 'id', 'position', or 'value'.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	if (cluster_type == CLUSTER_BY_VALUE) {
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

	if (xmmsv_dict_entry_get_type (fetch, "cluster-fallback") == XMMSV_TYPE_NONE) {
		fallback = NULL;
	} else if (!xmmsv_dict_entry_get_string (fetch, "cluster-fallback", &fallback)) {
		const gchar *message = "Optional field 'default' must be a string.";
		xmms_error_set (err, XMMS_ERROR_INVAL, message);
		return NULL;
	}

	sp = normalize_source_preferences (fetch, prefs, err);
	if (xmms_error_iserror (err)) {
		return NULL;
	}

	data = xmms_fetch_spec_new (cluster_data, info, sp, err);
	if (xmms_error_iserror (err)) {
		s4_sourcepref_unref (sp);
		return NULL;
	}

	spec = g_new0 (xmms_fetch_spec_t, 1);
	spec->data.cluster.data = data;
	spec->data.cluster.type = cluster_type;
	spec->data.cluster.fallback = fallback;

	switch (spec->data.cluster.type) {
		case CLUSTER_BY_ID:
			/* Media library id is always found at the first column */
			spec->data.cluster.column = 0;
			break;
		case CLUSTER_BY_VALUE:
			xmmsv_dict_get (fetch, "cluster-field", &cluster_field);
			spec->data.cluster.column = xmms_fetch_info_add_key (info, cluster_field,
			                                                     field, sp);
			break;
		case CLUSTER_BY_POSITION:
			/* do nothing */
			break;
	}

	s4_sourcepref_unref (sp);

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
	s4_sourcepref_t *sp;
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

	sp = normalize_source_preferences (fetch, prefs, err);
	if (xmms_error_iserror (err)) {
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
		xmms_fetch_spec_t *orgee;
		const gchar *str;
		xmmsv_t *entry;

		xmmsv_dict_iter_pair (it, &str, &entry);

		orgee = xmms_fetch_spec_new (entry, info, sp, err);
		if (xmms_error_iserror (err)) {
			xmms_fetch_spec_free (spec);
			spec = NULL;
			break;
		}

		spec->data.organize.keys[org_idx] = str;
		spec->data.organize.data[org_idx] = orgee;

		org_idx++;
		xmmsv_dict_iter_next (it);
	}
	xmmsv_dict_iter_explicit_destroy (it);

	s4_sourcepref_unref (sp);

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

	if (xmmsv_dict_entry_get_type (fetch, "type") == XMMSV_TYPE_NONE) {
		xmmsv_dict_set_string (fetch, "type", "metadata");
	}

	if (!xmmsv_dict_entry_get_string (fetch, "type", &type)) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "A fetch specification must have a type.");
		return NULL;
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
