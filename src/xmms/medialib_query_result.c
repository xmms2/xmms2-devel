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

#include "xmmspriv/xmms_fetch_info.h"
#include "xmmspriv/xmms_fetch_spec.h"
#include "s4.h"

#include <glib.h>
#include <glib/gstdio.h>

xmmsv_t *xmms_medialib_query_to_xmmsv (s4_resultset_t *set, xmms_fetch_spec_t *spec);

typedef struct {
	gint64 sum;
	gint n;
} avg_data_t;

typedef struct {
	xmmsv_t *data;
	gint n;
} random_data_t;

typedef struct {
	GHashTable *ht;
	xmmsv_t *list;
} set_data_t;

static gboolean
aggregate_first (xmmsv_t **current, gint int_value, const gchar *str_value)
{
	if (*current != NULL) {
		return FALSE;
	}

	if (str_value != NULL) {
		*current = xmmsv_new_string (str_value);
	} else {
		*current = xmmsv_new_int (int_value);
	}

	return TRUE;
}

static gboolean
aggregate_list (xmmsv_t **current, gint int_value, const gchar *str_value)
{
	gboolean created = FALSE;

	if (*current == NULL) {
		*current = xmmsv_new_list ();
		created = TRUE;
	}

	if (str_value != NULL) {
		xmmsv_list_append_string (*current, str_value);
	} else {
		xmmsv_list_append_int (*current, int_value);
	}

	return created;
}

static gboolean
aggregate_set (xmmsv_t **current, gint int_value, const gchar *str_value)
{
	gboolean created = FALSE;
	set_data_t *data;
	xmmsv_t *value;
	gpointer key;
	guint length;

	if (*current == NULL) {
		set_data_t init = {
			.ht = g_hash_table_new (NULL, NULL),
			.list = xmmsv_new_list ()
		};
		*current = xmmsv_new_bin ((guchar *) &init, sizeof (set_data_t));
		created = TRUE;
	}

	xmmsv_get_bin (*current, (const guchar **) &data, &length);

	if (str_value != NULL) {
		value = xmmsv_new_string (str_value);
		key = (gpointer) str_value;
	} else {
		value = xmmsv_new_int (int_value);
		key = GINT_TO_POINTER (int_value);
	}

	if (g_hash_table_lookup (data->ht, key) == NULL) {
		g_hash_table_insert (data->ht, key, value);
		xmmsv_list_append (data->list, value);
	}

	xmmsv_unref (value);

	return created;
}

static gboolean
aggregate_sum (xmmsv_t **current, gint int_value, const gchar *str_value)
{
	gint old_value = 0;

	if (str_value != NULL) {
		/* 'sum' only applies to numbers */
		return FALSE;
	}

	if (*current != NULL) {
		xmmsv_get_int (*current, &old_value);
		xmmsv_unref (*current);
	}

	*current = xmmsv_new_int (old_value + int_value);

	return TRUE;
}

static gboolean
aggregate_min (xmmsv_t **current, gint int_value, const gchar *str_value)
{
	gint old_value;

	if (str_value != NULL) {
		/* 'min' only applies to numbers */
		return FALSE;
	}

	if (*current == NULL) {
		*current = xmmsv_new_int (int_value);
		return TRUE;
	}

	xmmsv_get_int (*current, &old_value);

	if (old_value > int_value) {
		xmmsv_unref (*current);
		*current = xmmsv_new_int (int_value);
		return TRUE;
	}

	return FALSE;
}

static gboolean
aggregate_max (xmmsv_t **current, gint int_value, const gchar *str_value)
{
	gint old_value;

	if (str_value != NULL) {
		/* 'max' only applies to numbers */
		return FALSE;
	}

	if (*current == NULL) {
		*current = xmmsv_new_int (int_value);
		return TRUE;
	}

	xmmsv_get_int (*current, &old_value);

	if (old_value > int_value) {
		xmmsv_unref (*current);
		*current = xmmsv_new_int (int_value);
		return TRUE;
	}

	return FALSE;
}

static gboolean
aggregate_random (xmmsv_t **current, gint int_value, const gchar *str_value)
{
	gboolean created = FALSE;
	random_data_t *data;
	guint length;

	if (*current == NULL) {
		random_data_t init = { 0 };
		*current = xmmsv_new_bin ((guchar *) &init, sizeof (random_data_t));
		created = TRUE;
	}

	xmmsv_get_bin (*current, (const guchar **) &data, &length);

	data->n++;

	if (g_random_int_range (0, data->n) == 0) {
		xmmsv_unref (data->data);
		if (str_value != NULL) {
			data->data = xmmsv_new_string (str_value);
		} else {
			data->data = xmmsv_new_int (int_value);
		}
	}

	return created;
}

static gboolean
aggregate_average (xmmsv_t **current, gint int_value, const gchar *str_value)
{
	gboolean created = FALSE;
	avg_data_t *data;
	guint length;

	if (*current == NULL) {
		avg_data_t init = { 0 };
		*current = xmmsv_new_bin ((guchar *) &init, sizeof (avg_data_t));
		created = TRUE;
	}

	xmmsv_get_bin (*current, (const guchar **) &data, &length);

	if (str_value == NULL) {
		data->n++;
		data->sum += int_value;
	}

	return created;
}

/* Converts an S4 result (a column) into an xmmsv values */
static void *
result_to_xmmsv (xmmsv_t *ret, gint32 id, const s4_result_t *res,
                 xmms_fetch_spec_t *spec)
{
	const s4_val_t *val;
	xmmsv_t *dict, *current;
	const gchar *str_value, *key = NULL;
	gint32 i, int_value;
	gboolean changed;

	/* Loop through all the values the column has */
	while (res != NULL) {
		dict = ret;
		current = ret;

		/* Loop through the list of what to get ("key", "source", ..) */
		for (i = 0; i < spec->data.metadata.get_size; i++) {
			str_value = NULL;

			/* Fill str_value with the correct value if it is a string
			 * or int_value if it is an integer
			 */
			switch (spec->data.metadata.get[i]) {
				case METADATA_KEY:
					str_value = s4_result_get_key (res);
					break;
				case METADATA_SOURCE:
					str_value = s4_result_get_src (res);
					if (str_value == NULL)
						str_value = "server";
					break;
				case METADATA_ID:
					int_value = id;
					break;
				case METADATA_VALUE:
					val = s4_result_get_val (res);

					if (!s4_val_get_int (val, &int_value)) {
						s4_val_get_str (val, &str_value);
					}
					break;
			}

			/* If this is not the last property to get we use this property
			 * as a key in a dict
			 */
			if (i < (spec->data.metadata.get_size - 1)) {
				/* Convert integers to strings */
				if (str_value == NULL) {
					/* Big enough to hold 2^32 with minus sign */
					gchar buf[12];
					g_sprintf (buf, "%i", int_value);
					key = buf;
				} else {
					key = str_value;
				}

				/* Make sure the root dict exists */
				if (dict == NULL) {
					ret = dict = xmmsv_new_dict ();
				}

				/* If this dict contains dicts we have to create a new
				 * dict if one does not exists for the key yet
				 */
				if (!xmmsv_dict_get (dict, key, &current))
					current = NULL;

				if (i < (spec->data.metadata.get_size - 2)) {
					if (current == NULL) {
						current = xmmsv_new_dict ();
						xmmsv_dict_set (dict, key, current);
						xmmsv_unref (current);
					}
					dict = current;
				}
			}
		}

		changed = 0;

		switch (spec->data.metadata.aggr_func) {
			case AGGREGATE_FIRST:
				changed = aggregate_first (&current, int_value, str_value);
				break;
			case AGGREGATE_LIST:
				changed = aggregate_list (&current, int_value, str_value);
				break;
			case AGGREGATE_SET:
				changed = aggregate_set (&current, int_value, str_value);
				break;
			case AGGREGATE_SUM:
				changed = aggregate_sum (&current, int_value, str_value);
				break;
			case AGGREGATE_MIN:
				changed = aggregate_min (&current, int_value, str_value);
				break;
			case AGGREGATE_MAX:
				changed = aggregate_max (&current, int_value, str_value);
				break;
			case AGGREGATE_RANDOM:
				changed = aggregate_random (&current, int_value, str_value);
				break;
			case AGGREGATE_AVG:
				changed = aggregate_average (&current, int_value, str_value);
				break;
		}

		/* Update the previous dict (if there is one) */
		if (i > 1 && changed) {
			xmmsv_dict_set (dict, key, current);
			xmmsv_unref (current);
		} else if (changed) {
			ret = current;
		}

		res = s4_result_next (res);
	}

	return ret;
}

/* Converts the temporary value returned by result_to_xmmsv into the real value */
static xmmsv_t *
aggregate_data (xmmsv_t *value, aggregate_function_t aggr_func)
{
	const random_data_t *random_data;
	const avg_data_t *avg_data;
	const set_data_t *set_data;
	gconstpointer data;
	xmmsv_t *ret;
	guint len;

	ret = NULL;
	data = NULL;

	if (value != NULL && xmmsv_is_type (value, XMMSV_TYPE_BIN))
		xmmsv_get_bin (value, (const guchar **) &data, &len);

	switch (aggr_func) {
		case AGGREGATE_FIRST:
		case AGGREGATE_MIN:
		case AGGREGATE_MAX:
		case AGGREGATE_SUM:
		case AGGREGATE_LIST:
			ret = xmmsv_ref (value);
			break;
		case AGGREGATE_RANDOM:
			random_data = data;
			if (random_data != NULL) {
				ret = random_data->data;
			}
			break;
		case AGGREGATE_SET:
			set_data = data;
			g_hash_table_destroy (set_data->ht);
			ret = set_data->list;
			break;
		case AGGREGATE_AVG:
			avg_data = data;
			if (avg_data != NULL) {
				ret = xmmsv_new_int (avg_data->n ? avg_data->sum / avg_data->n : 0);
			}
			break;
	}

	xmmsv_unref (value);

	return ret;
}

/* Applies an aggregation function to the leafs in an xmmsv dict tree */
static xmmsv_t *
aggregate_result (xmmsv_t *val, gint depth, aggregate_function_t aggr_func)
{
	xmmsv_dict_iter_t *it;

	if (val == NULL) {
		return NULL;
	}

	if (depth == 0) {
		return aggregate_data (val, aggr_func);
	}

	/* If it's a dict we call this function recursively on all its values */
	xmmsv_get_dict_iter (val, &it);

	while (xmmsv_dict_iter_valid (it)) {
		xmmsv_t *entry;

		xmmsv_dict_iter_pair (it, NULL, &entry);

		entry = aggregate_result (entry, depth - 1, aggr_func);
		xmmsv_dict_iter_set (it, entry);
		xmmsv_unref (entry);

		xmmsv_dict_iter_next (it);
	}

	return val;
}

/* Converts an S4 resultset to an xmmsv using the fetch specification */
static xmmsv_t *
metadata_to_xmmsv (s4_resultset_t *set, xmms_fetch_spec_t *spec)
{
	const s4_resultrow_t *row;
	xmmsv_t *ret = NULL;
	gint i;

	/* Loop over the rows in the resultset */
	for (i = 0; s4_resultset_get_row (set, i, &row); i++) {
		gint32 id, j;

		s4_val_get_int (s4_result_get_val (s4_resultset_get_result (set, i, 0)), &id);
		for (j = 0; j < spec->data.metadata.col_count; j++) {
			const s4_result_t *res;

			if (s4_resultrow_get_col (row, spec->data.metadata.cols[j], &res)) {
				ret = result_to_xmmsv (ret, id, res, spec);
			}
		}
	}

	return aggregate_result (ret, spec->data.metadata.get_size - 1,
	                         spec->data.metadata.aggr_func);
}


/* Divides an S4 set into a list of smaller sets with
 * the same values for the cluster attributes
 */
static void
cluster_set (s4_resultset_t *set, xmms_fetch_spec_t *spec,
             GHashTable *table, GList **list)
{
	const s4_resultrow_t *row;
	gint position;

	/* Run through all the rows in the result set.
	 * Uses a hash table to find the correct cluster to put the row in
	 */
	for (position = 0; s4_resultset_get_row (set, position, &row); position++) {
		s4_resultset_t *cluster;
		const s4_result_t *res;
		const gchar *value = "(No value)"; /* Used to represent NULL */
		gchar buf[12];

		if (spec->data.cluster.type == CLUSTER_BY_POSITION) {
			g_snprintf (buf, sizeof (buf), "%i", position);
			value = buf;
		} else if (s4_resultrow_get_col (row, spec->data.cluster.column, &res)) {
			const s4_val_t *val = s4_result_get_val (res);
			if (!s4_val_get_str (val, &value)) {
				gint32 ival;
				s4_val_get_int (val, &ival);
				g_snprintf (buf, sizeof (buf), "%i", ival);
				value = buf;
			}
		}

		cluster = g_hash_table_lookup (table, value);
		if (cluster == NULL) {
			cluster = s4_resultset_create (s4_resultset_get_colcount (set));
			g_hash_table_insert (table, g_strdup (value), cluster);
			*list = g_list_prepend (*list, cluster);
		}
		s4_resultset_add_row (cluster, row);
	}
}

static GList *
cluster_list (s4_resultset_t *set, xmms_fetch_spec_t *spec)
{
	GHashTable *table;
	GList *list = NULL;

	table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	cluster_set (set, spec, table, &list);
	g_hash_table_destroy (table);

	return g_list_reverse (list);
}

static GHashTable *
cluster_dict (s4_resultset_t *set, xmms_fetch_spec_t *spec)
{
	GHashTable *table;
	GList *list = NULL;

	table = g_hash_table_new_full (g_str_hash, g_str_equal,
	                               g_free, (GDestroyNotify) s4_resultset_free);

	cluster_set (set, spec, table, &list);
	g_list_free (list);

	return table;
}

static xmmsv_t *
convert_ghashtable_to_xmmsv (GHashTable *table, xmms_fetch_spec_t *spec)
{
	GHashTableIter iter;
	s4_resultset_t *value;
	const gchar *key;
	xmmsv_t *ret;

	g_hash_table_iter_init (&iter, table);

	ret = xmmsv_new_dict ();

	while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &value)) {
		xmmsv_t *converted;

		if (value == NULL) {
			continue;
		}

		converted = xmms_medialib_query_to_xmmsv (value, spec);
		xmmsv_dict_set (ret, key, converted);
		xmmsv_unref (converted);
	}

	if (xmmsv_dict_get_size (ret) == 0) {
		xmmsv_unref (ret);
		ret = NULL;
	}

	return ret;
}

/* Converts an S4 resultset into an xmmsv_t, based on the fetch specification */
xmmsv_t *
xmms_medialib_query_to_xmmsv (s4_resultset_t *set, xmms_fetch_spec_t *spec)
{
	GHashTable *set_table;
	GList *sets;
	xmmsv_t *val, *ret = NULL;
	gint i;

	switch (spec->type) {
		case FETCH_COUNT:
			ret = xmmsv_new_int (s4_resultset_get_rowcount (set));
			break;
		case FETCH_METADATA:
			ret = metadata_to_xmmsv (set, spec);
			break;
		case FETCH_ORGANIZE:
			ret = xmmsv_new_dict ();

			for (i = 0; i < spec->data.organize.count; i++) {
				val = xmms_medialib_query_to_xmmsv (set, spec->data.organize.data[i]);
				if (val != NULL) {
					xmmsv_dict_set (ret, spec->data.organize.keys[i], val);
					xmmsv_unref (val);
				}
			}
			break;
		case FETCH_CLUSTER_LIST:
			sets = cluster_list (set, spec);
			ret = xmmsv_new_list ();
			for (; sets != NULL; sets = g_list_delete_link (sets, sets)) {
				set = sets->data;

				val = xmms_medialib_query_to_xmmsv (set, spec->data.cluster.data);
				if (val != NULL) {
					xmmsv_list_append (ret, val);
					xmmsv_unref (val);
				}
				s4_resultset_free (set);
			}
			break;
		case FETCH_CLUSTER_DICT:
			set_table = cluster_dict (set, spec);
			ret = convert_ghashtable_to_xmmsv (set_table, spec->data.cluster.data);

			g_hash_table_destroy (set_table);
			break;
	}

	return ret;
}
