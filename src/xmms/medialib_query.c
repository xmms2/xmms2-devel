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

#include <xmms_configuration.h>
#include <xmmspriv/xmms_medialib.h>
#include <xmmspriv/xmms_xform.h>
#include <xmmspriv/xmms_utils.h>
#include <xmms/xmms_error.h>
#include <xmms/xmms_config.h>
#include <xmms/xmms_object.h>
#include <xmms/xmms_ipc.h>
#include <xmms/xmms_log.h>


#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <time.h>

#include <xmmspriv/xmms_fetch_info.h>
#include <xmmspriv/xmms_fetch_spec.h>
#include "s4.h"

static s4_condition_t *collection_to_condition (xmms_medialib_session_t *s, xmmsv_t *coll, xmms_fetch_info_t *fetch, xmmsv_t *order);

typedef enum xmms_sort_type_St {
	SORT_TYPE_COLUMN,
	SORT_TYPE_RANDOM,
	SORT_TYPE_LIST
} xmms_sort_type_t;

/* A filter matching everything */
static gint
universe_filter (void)
{
	return 0;
}

/* A filter for idlists. Checks if the value given (id number)
 * is in the hash table
 */
static gint
idlist_filter (const s4_val_t *value, s4_condition_t *cond)
{
	GHashTable *id_table;
	gint32 ival;

	if (!s4_val_get_int (value, &ival)) {
		return 1;
	}

	id_table = s4_cond_get_funcdata (cond);

	return g_hash_table_lookup (id_table, GINT_TO_POINTER (ival)) == NULL;
}

/**
 * Creates a new resultset where the order is the same as in the idlist
 *
 * @param set The resultset to sort. It will be freed by this function
 * @param id_pos The position of the "id" column
 * @param idlist The idlist to order by
 * @return A new set with the same order as the idlist
 */
static s4_resultset_t *
xmms_medialib_result_sort_idlist (s4_resultset_t *set, xmmsv_t *idlist)
{
	const s4_resultrow_t *row;
	const s4_result_t *result;
	GHashTable *row_table;
	s4_resultset_t *ret;
	gint32 ival, i;

	row_table = g_hash_table_new (NULL, NULL);

	for (i = 0; s4_resultset_get_row (set, i, &row); i++) {
		if (s4_resultrow_get_col (row, 0, &result)
		    && s4_val_get_int (s4_result_get_val (result), &ival)) {
			g_hash_table_insert (row_table, GINT_TO_POINTER (ival), (void *) row);
		}
	}

	ret = s4_resultset_create (s4_resultset_get_colcount (set));

	for (i = 0; xmmsv_list_get_int (idlist, i, &ival); i++) {
		row = g_hash_table_lookup (row_table, GINT_TO_POINTER (ival));
		if (row != NULL) {
			s4_resultset_add_row (ret, row);
		}
	}

	g_hash_table_destroy (row_table);
	s4_resultset_free (set);

	return ret;
}

/**
 * Sorts a resultset
 *
 * @param set The resultset to sort
 * @param fetch The fetch-list used when set was created
 * @param order A list with orderings. An ordering can be a string
 * telling which column to sort by (prefixed by '-' to sort ascending)
 * or a list of integers (an idlist).
 * @return The set (or a new set) with the correct ordering
 */
static s4_resultset_t *
xmms_medialib_result_sort (s4_resultset_t *set, xmms_fetch_info_t *fetch_info, xmmsv_t *order)
{
	gint i, stop, type;
	s4_order_t *s4_order;
	xmmsv_t *val;

	/* Find the first idlist-order operand */
	for (i = 0; xmmsv_list_get (order, i, &val); i++) {
		xmmsv_dict_entry_get_int (val, "type", &type);
		if (type == SORT_TYPE_LIST) {
			xmmsv_t *idlist;
			xmmsv_dict_get (val, "list", &idlist);
			set = xmms_medialib_result_sort_idlist (set, idlist);
			break;
		}
	}

	/* We willorder by the operands before the idlist */
	stop = i;

	s4_order = s4_order_create ();

	for (i = 0; i < stop && xmmsv_list_get (order, i, &val); i++) {
		xmmsv_dict_entry_get_int (val, "type", &type);

		if (type == SORT_TYPE_COLUMN) {
			gint id, j, direction, collation;
			s4_order_entry_t *entry;
			xmmsv_t *ids;

			if (!xmmsv_dict_entry_get_int (val, "direction", &direction))
				direction = S4_ORDER_ASCENDING;
			if (!xmmsv_dict_entry_get_int (val, "collation", &collation))
				collation = S4_CMP_COLLATE;

			xmmsv_dict_get (val, "field", &ids);

			entry = s4_order_add_column (s4_order, collation, direction);
			for (j = 0; xmmsv_list_get_int (ids, j, &id); j++) {
				s4_order_entry_add_choice (entry, id);
			}
		} else if (type == SORT_TYPE_RANDOM) {
			gint seed;
			if (!xmmsv_dict_entry_get_int (val, "seed", &seed))
				seed = g_random_int_range (G_MININT32, G_MAXINT32);
			s4_order_add_random (s4_order, seed);
			break;
		}
	}

	s4_resultset_sort (set, s4_order);
	s4_order_free (s4_order);

	return set;
}

/* Check if a collection is the universe
 * TODO: Move it to the xmmstypes lib?
 */
static gboolean
is_universe (xmmsv_t *coll)
{
	const gchar *target_name;
	gboolean ret = FALSE;

	switch (xmmsv_coll_get_type (coll)) {
		case XMMS_COLLECTION_TYPE_UNIVERSE:
			ret = TRUE;
			break;
		case XMMS_COLLECTION_TYPE_REFERENCE:
			if (xmmsv_coll_attribute_get_string (coll, "reference", &target_name)
			    && strcmp (target_name, "All Media") == 0)
				ret = TRUE;
			break;
		default:
			break;
	}

	return ret;
}

/* Returns non-zero if the collection has an ordering, 0 otherwise */
static gboolean
has_order (xmmsv_t *coll)
{
	xmmsv_t *operands, *operand;
	gint i;

	operands = xmmsv_coll_operands_get (coll);

	switch (xmmsv_coll_get_type (coll)) {
		/* Filter keeps the ordering of the operand */
		case XMMS_COLLECTION_TYPE_HAS:
		case XMMS_COLLECTION_TYPE_MATCH:
		case XMMS_COLLECTION_TYPE_TOKEN:
		case XMMS_COLLECTION_TYPE_EQUALS:
		case XMMS_COLLECTION_TYPE_NOTEQUAL:
		case XMMS_COLLECTION_TYPE_SMALLER:
		case XMMS_COLLECTION_TYPE_SMALLEREQ:
		case XMMS_COLLECTION_TYPE_GREATER:
		case XMMS_COLLECTION_TYPE_GREATEREQ:
			/* Intersection is orderded if the first operand is ordeed */
		case XMMS_COLLECTION_TYPE_INTERSECTION:
			xmmsv_list_get (operands, 0, &operand);
			return has_order (operand);

			/* Union is ordered if all operands are ordered (concat) */
		case XMMS_COLLECTION_TYPE_UNION:
			for (i = 0; xmmsv_list_get (operands, i, &operand); i++) {
				if (!has_order (operand))
					return FALSE;
			}

			/* These are always ordered */
		case XMMS_COLLECTION_TYPE_IDLIST:
		case XMMS_COLLECTION_TYPE_ORDER:
		case XMMS_COLLECTION_TYPE_LIMIT:
			return TRUE;

		case XMMS_COLLECTION_TYPE_REFERENCE:
			if (!is_universe (coll)) {
				xmmsv_list_get (operands, 0, &operand);
				return has_order (operand);
			}
		case XMMS_COLLECTION_TYPE_COMPLEMENT:
		case XMMS_COLLECTION_TYPE_UNIVERSE:
		case XMMS_COLLECTION_TYPE_MEDIASET:
			break;
	}

	return FALSE;
}


static s4_condition_t *
create_idlist_filter (xmms_medialib_session_t *session, GHashTable *id_table)
{
	s4_sourcepref_t *sourcepref;
	s4_condition_t *condition;

	sourcepref = xmms_medialib_session_get_source_preferences (session);

	condition = s4_cond_new_custom_filter (idlist_filter, id_table,
	                                       (free_func_t) g_hash_table_destroy,
	                                       "song_id", sourcepref, 0, 0,
	                                       S4_COND_PARENT);

	s4_sourcepref_unref (sourcepref);

	return condition;
}

static s4_condition_t *
complement_condition (xmms_medialib_session_t *session, xmmsv_t *coll,
                      xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	xmmsv_t *operands, *operand;
	s4_condition_t *cond;

	cond = s4_cond_new_combiner (S4_COMBINE_NOT);

	operands = xmmsv_coll_operands_get (coll);
	if (xmmsv_list_get (operands, 0, &operand)) {
		s4_condition_t *operand_cond;
		operand_cond = collection_to_condition (session, operand, fetch, order);
		s4_cond_add_operand (cond, operand_cond);
		s4_cond_unref (operand_cond);
	}

	return cond;
}

static s4_filter_type_t
filter_type_from_collection (xmmsv_t *coll)
{
	xmmsv_coll_type_t type = xmmsv_coll_get_type (coll);

	switch (type) {
		case XMMS_COLLECTION_TYPE_HAS:
			return S4_FILTER_EXISTS;
		case XMMS_COLLECTION_TYPE_MATCH:
			return S4_FILTER_MATCH;
		case XMMS_COLLECTION_TYPE_TOKEN:
			return S4_FILTER_TOKEN;
		case XMMS_COLLECTION_TYPE_EQUALS:
			return S4_FILTER_EQUAL;
		case XMMS_COLLECTION_TYPE_NOTEQUAL:
			return S4_FILTER_NOTEQUAL;
		case XMMS_COLLECTION_TYPE_SMALLER:
			return S4_FILTER_SMALLER;
		case XMMS_COLLECTION_TYPE_SMALLEREQ:
			return S4_FILTER_SMALLEREQ;
		case XMMS_COLLECTION_TYPE_GREATER:
			return S4_FILTER_GREATER;
		case XMMS_COLLECTION_TYPE_GREATEREQ:
			return S4_FILTER_GREATEREQ;
		default:
			g_assert_not_reached ();
	}
}

static void
get_filter_type_and_compare_mode (xmmsv_t *coll,
                                  s4_filter_type_t *type,
                                  s4_cmp_mode_t *cmp_mode)
{
	const gchar *value;

	*type = filter_type_from_collection (coll);

	if (!xmmsv_coll_attribute_get_string (coll, "collation", &value)) {
		/* For <, <=, >= and > we default to natcoll,
		 * so that strings will order correctly
		 */
		switch (*type) {
			case S4_FILTER_SMALLER:
			case S4_FILTER_GREATER:
			case S4_FILTER_SMALLEREQ:
			case S4_FILTER_GREATEREQ:
				*cmp_mode = S4_CMP_COLLATE;
				break;
			default:
				*cmp_mode = S4_CMP_CASELESS;
		}
	} else if (strcmp (value, "NOCASE") == 0) {
		*cmp_mode = S4_CMP_CASELESS;
	} else if (strcmp (value, "BINARY") == 0) {
		*cmp_mode = S4_CMP_BINARY;
	} else if (strcmp (value, "NATCOLL") == 0) {
		*cmp_mode = S4_CMP_COLLATE;
	} else {
		/* Programming error, too weak validation. */
		g_assert_not_reached ();
	}
}

static s4_condition_t *
filter_condition (xmms_medialib_session_t *session,
                  xmmsv_t *coll, xmms_fetch_info_t *fetch,
                  xmmsv_t *order)
{
	s4_sourcepref_t *sp;
	s4_filter_type_t type;
	s4_cmp_mode_t cmp_mode;
	gint32 ival, flags = 0;
	const gchar *filter_type, *key, *val;
	xmmsv_t *operands, *operand;
	s4_condition_t *cond;
	s4_val_t *value = NULL;

	if (!xmmsv_coll_attribute_get_string (coll, "type", &filter_type) || strcmp (filter_type, "value") == 0) {
		/* If 'field' is not set, match against every key */
		if (!xmmsv_coll_attribute_get_string (coll, "field", &key)) {
			key = NULL;
		}
	} else {
		key = (gchar *) "song_id";
		flags = S4_COND_PARENT;
	}

	if (xmmsv_coll_attribute_get_string (coll, "value", &val)) {
		gchar *endptr;

		ival = strtol (val, &endptr, 10);
		if (endptr > val && *endptr == '\0') {
			value = s4_val_new_int (ival);
		} else {
			value = s4_val_new_string (val);
		}
	}

	if (xmmsv_coll_attribute_get_string (coll, "source-preference", &val)) {
		gchar **prefs;
		prefs = g_strsplit (val, ":", -1);
		sp = s4_sourcepref_create ((const gchar **) prefs);
		g_strfreev (prefs);
	} else {
		sp = xmms_medialib_session_get_source_preferences (session);
	}

	get_filter_type_and_compare_mode (coll, &type, &cmp_mode);

	cond = s4_cond_new_filter (type, key, value, sp, cmp_mode, flags);

	s4_val_free (value);
	s4_sourcepref_unref (sp);

	operands = xmmsv_coll_operands_get (coll);
	xmmsv_list_get (operands, 0, &operand);

	if (!is_universe (operand)) {
		s4_condition_t *op_cond = cond;
		cond = s4_cond_new_combiner (S4_COMBINE_AND);
		s4_cond_add_operand (cond, op_cond);
		s4_cond_unref (op_cond);
		op_cond = collection_to_condition (session, operand, fetch, order);
		s4_cond_add_operand (cond, op_cond);
		s4_cond_unref (op_cond);
	}

	return cond;
}

static s4_condition_t *
idlist_condition (xmms_medialib_session_t *session,
                  xmmsv_t *coll, xmms_fetch_info_t *fetch,
                  xmmsv_t *order)
{
	GHashTable *id_table;
	gint32 i, ival;
	xmmsv_t *child_order, *idlist;

	idlist = xmmsv_coll_idlist_get (coll);

	child_order = xmmsv_build_dict (XMMSV_DICT_ENTRY_INT ("type", SORT_TYPE_LIST),
	                                XMMSV_DICT_ENTRY ("list", xmmsv_ref (idlist)),
	                                XMMSV_DICT_END);

	xmmsv_list_append (order, child_order);
	xmmsv_unref (child_order);

	id_table = g_hash_table_new (NULL, NULL);

	for (i = 0; xmmsv_coll_idlist_get_index (coll, i, &ival); i++) {
		g_hash_table_insert (id_table, GINT_TO_POINTER (ival), GINT_TO_POINTER (1));
	}

	return create_idlist_filter (session, id_table);
}

static s4_condition_t *
intersection_condition (xmms_medialib_session_t *session, xmmsv_t *coll,
                        xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	s4_condition_t *cond;
	xmmsv_t *operands, *operand;
	gint i;

	operands = xmmsv_coll_operands_get (coll);
	cond = s4_cond_new_combiner (S4_COMBINE_AND);

	for (i = 0; xmmsv_list_get (operands, i, &operand); i++) {
		s4_condition_t *op_cond;
		if (i == 0) {
			/* We keep the ordering of the first operand */
			op_cond = collection_to_condition (session, operand, fetch, order);
		} else {
			op_cond = collection_to_condition (session, operand, fetch, NULL);
		}
		s4_cond_add_operand (cond, op_cond);
		s4_cond_unref (op_cond);
	}

	return cond;
}

/**
 * Calculate a hash code for the combination of multiple column values.
 */
static gint
limit_condition_calculate_hash (const s4_resultrow_t *row, gint *idx)
{
	gint hash = 1;
	gint i;

	for (i = 0; idx[i] != '\0'; i++) {
		const s4_result_t *result;
		const s4_val_t *value;
		const gchar *string;
		gint32 number;

		if (!s4_resultrow_get_col (row, idx[i], &result))
			continue;

		value = s4_result_get_val (result);
		if (s4_val_get_str (value, &string)) {
			hash = hash * 31 + g_str_hash (string);
		} else if (s4_val_get_int (value, &number)) {
			hash = hash * 31 + (number ^ (number >> 16));
		}
	};

	return hash;
}

static gboolean
limit_condition_fields (xmms_medialib_session_t *session, const gchar *value,
                        xmms_fetch_info_t *fetch, gint **result)
{
	s4_sourcepref_t *sourcepref;
	gchar **fields;
	gint i, length;
	gint *indices;

	fields = g_strsplit (value, ",", 0);
	length = g_strv_length (fields);

	sourcepref = xmms_medialib_session_get_source_preferences (session);

	indices = g_new0 (gint, MAX (0, length + 1));
	for (i = 0; fields[i]; i++) {
		indices[i] = xmms_fetch_info_add_key (fetch, NULL, fields[i], sourcepref);
	}
	*result = indices;

	s4_sourcepref_unref (sourcepref);

	g_strfreev (fields);

	return TRUE;
}

/**
 * Limit search result based on row index.
 * Based on start, length, skip the first few items, and only take
 * as many as length specifies.
 */
static void
limit_condition_by_position (s4_resultset_t *set, xmmsv_t *id_list, GHashTable *id_table,
                             gint start, guint length)
{
	guint stop = start + length;
	gint i;

	for (i = start; i < stop; i++) {
		const s4_resultrow_t *row;
		const s4_result_t *result;
		const s4_val_t *value;
		gint mid;

		if (!s4_resultset_get_row (set, i, &row))
			break;

		if (!s4_resultrow_get_col (row, 0, &result))
			continue;

		value = s4_result_get_val (result);
		if (!s4_val_get_int (value, &mid))
			continue;

		xmmsv_list_append_int (id_list, mid);

		g_hash_table_insert (id_table,
		                     GINT_TO_POINTER (mid),
		                     GINT_TO_POINTER (TRUE));
	}
}

/**
 * Limit search result based on the combined key of multiple fields.
 * This is useful when grouping is applied on the result and you want
 * for example to query the second and third albums by an artist.
 */
static void
limit_condition_by_value (s4_resultset_t *set, xmmsv_t *id_list, GHashTable *id_table,
                          gint start, guint length, gint *indices)
{
	GHashTable *group_table, *skip_table;
	const s4_resultrow_t *row;
	gint i;

	skip_table = g_hash_table_new (g_direct_hash, g_direct_equal);
	group_table = g_hash_table_new (g_direct_hash, g_direct_equal);

	for (i = 0; s4_resultset_get_row (set, i, &row); i++) {
		const s4_result_t *result;
		const s4_val_t *value;
		gint hash, mid;

		hash = limit_condition_calculate_hash (row, indices);

		if (g_hash_table_size (skip_table) < start) {
			g_hash_table_insert (skip_table,
			                     GINT_TO_POINTER (hash),
			                     GINT_TO_POINTER (TRUE));
			continue;
		}

		if (g_hash_table_lookup (group_table, GINT_TO_POINTER (hash)) == NULL) {
			if (g_hash_table_lookup (skip_table, GINT_TO_POINTER (hash)) != NULL)
				continue;

			if (g_hash_table_size (group_table) >= length)
				continue;

			g_hash_table_insert (group_table,
			                     GINT_TO_POINTER (hash),
			                     GINT_TO_POINTER (TRUE));
		}

		if (!s4_resultrow_get_col (row, 0, &result))
			continue;

		value = s4_result_get_val (result);
		if (!s4_val_get_int (value, &mid))
			continue;

		xmmsv_list_append_int (id_list, mid);

		g_hash_table_insert (id_table,
		                     GINT_TO_POINTER (mid),
		                     GINT_TO_POINTER (TRUE));
	}

	g_hash_table_unref (group_table);
	g_hash_table_unref (skip_table);
}

static s4_condition_t *
limit_condition (xmms_medialib_session_t *session, xmmsv_t *coll,
                 xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	s4_resultset_t *set;
	xmmsv_t *operands, *operand, *id_list, *child_order;
	GHashTable *id_table;
	const gchar *type, *fields;
	gint start, length;
	gint *indices;

	if (!xmms_collection_get_int_attr (coll, "start", &start))
		start = 0;

	if (!xmms_collection_get_int_attr (coll, "length", &length))
		length = G_MAXINT32;

	if (!xmmsv_coll_attribute_get_string (coll, "type", &type))
		type = "position";

	xmmsv_coll_attribute_get_string (coll, "fields", &fields);

	operands = xmmsv_coll_operands_get (coll);
	xmmsv_list_get (operands, 0, &operand);

	id_list = xmmsv_new_list ();
	id_table = g_hash_table_new (g_direct_hash, g_direct_equal);

	set = xmms_medialib_query_recurs (session, operand, fetch);

	if (strcmp ("value", type) == 0 && limit_condition_fields (session, fields, fetch, &indices)) {
		limit_condition_by_value (set, id_list, id_table, start, length, indices);
		g_free (indices);
	} else if (strcmp ("id", type) == 0 && limit_condition_fields (session, "id", fetch, &indices)) {
		limit_condition_by_value (set, id_list, id_table, start, length, indices);
		g_free (indices);
	} else {
		limit_condition_by_position (set, id_list, id_table, start, length);
	}

	s4_resultset_free (set);

	/* Need ordering for correct windowing */
	child_order = xmmsv_build_dict (XMMSV_DICT_ENTRY_INT ("type", SORT_TYPE_LIST),
	                                XMMSV_DICT_ENTRY ("list", id_list),
	                                XMMSV_DICT_END);

	xmmsv_list_append (order, child_order);
	xmmsv_unref (child_order);

	return create_idlist_filter (session, id_table);
}

static s4_condition_t *
mediaset_condition (xmms_medialib_session_t *session, xmmsv_t *coll,
                    xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	xmmsv_t *operands, *operand;

	operands = xmmsv_coll_operands_get (coll);
	xmmsv_list_get (operands, 0, &operand);

	return collection_to_condition (session, operand, fetch, NULL);
}

static void
order_condition_by_id (xmmsv_t *entry,
                       xmms_fetch_info_t *fetch,
                       s4_sourcepref_t *sourcepref)
{
	xmmsv_t *value;
	gint field;

	field = xmms_fetch_info_add_key (fetch, NULL, "id", sourcepref);

	value = xmmsv_build_list (XMMSV_LIST_ENTRY_INT (field),
	                          XMMSV_LIST_END);

	xmmsv_dict_set_int (entry, "type", SORT_TYPE_COLUMN);
	xmmsv_dict_set (entry, "field", value);

	xmmsv_unref (value);
}

static void
order_condition_by_value (xmmsv_t *entry,
                          xmms_fetch_info_t *fetch,
                          s4_sourcepref_t *sourcepref,
                          xmmsv_t *coll)
{
	xmmsv_t *attrs, *field, *ids;
	xmmsv_list_iter_t *it;
	const gchar *value;

	attrs = xmmsv_coll_attributes_get (coll);
	xmmsv_dict_get (attrs, "field", &field);

	if (xmmsv_is_type (field, XMMSV_TYPE_STRING)) {
		xmmsv_t *list = xmmsv_new_list ();
		xmmsv_list_append (list, field);
		xmmsv_dict_set (attrs, "field", list);
		xmmsv_unref (list);
		field = list;
	}

	ids = xmmsv_new_list ();

	xmmsv_get_list_iter (field, &it);
	while (xmmsv_list_iter_entry_string (it, &value)) {
		gint id = xmms_fetch_info_add_key (fetch, NULL, value, sourcepref);
		xmmsv_list_append_int (ids, id);
		xmmsv_list_iter_next (it);
	}

	xmmsv_dict_set_int (entry, "type", SORT_TYPE_COLUMN);
	xmmsv_dict_set (entry, "field", ids);

	xmmsv_unref (ids);
}

/**
 * Add a dict to the sort list:
 * { "type": (ID|VALUE|RANDOM|LIST), "field": ..., "direction": ... }
 */
static s4_condition_t *
order_condition (xmms_medialib_session_t *session, xmmsv_t *coll,
                 xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	s4_sourcepref_t *sourcepref;
	xmmsv_t *operands, *operand, *entry;
	const gchar *key;

	entry = xmmsv_new_dict ();

	if (!xmmsv_coll_attribute_get_string (coll, "type", &key)) {
		key = (gchar *) "value";
	}

	sourcepref = xmms_medialib_session_get_source_preferences (session);

	if (strcmp (key, "random") == 0) {
		gint seed;
		xmmsv_dict_set_int (entry, "type", SORT_TYPE_RANDOM);
		if (xmms_collection_get_int_attr (coll, "seed", &seed))
			xmmsv_dict_set_int (entry, "seed", seed);
	} else if (strcmp (key, "id") == 0) {
		order_condition_by_id (entry, fetch, sourcepref);
	} else {
		order_condition_by_value (entry, fetch, sourcepref, coll);
	}

	s4_sourcepref_unref (sourcepref);

	if (!xmmsv_coll_attribute_get_string (coll, "direction", &key)) {
		xmmsv_dict_set_int (entry, "direction", S4_ORDER_ASCENDING);
	} else if (strcmp (key, "ASC") == 0) {
		xmmsv_dict_set_int (entry, "direction", S4_ORDER_ASCENDING);
	} else {
		xmmsv_dict_set_int (entry, "direction", S4_ORDER_DESCENDING);
	}

	xmmsv_list_append (order, entry);
	xmmsv_unref (entry);

	operands = xmmsv_coll_operands_get (coll);
	xmmsv_list_get (operands, 0, &operand);

	return collection_to_condition (session, operand, fetch, order);
}

static s4_condition_t *
union_ordered_condition (xmms_medialib_session_t *session, xmmsv_t *coll,
                         xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *operands, *operand, *id_list, *entry;
	GHashTable *id_table;

	id_list = xmmsv_new_list ();
	id_table = g_hash_table_new (NULL, NULL);
	operands = xmmsv_coll_operands_get (coll);

	xmmsv_get_list_iter (operands, &it);
	while (xmmsv_list_iter_entry (it, &operand)) {
		const s4_resultrow_t *row;
		s4_resultset_t *set;
		gint j;

		/* Query the operand */
		set = xmms_medialib_query_recurs (session, operand, fetch);

		/* Append the IDs to the id_list */
		for (j = 0; s4_resultset_get_row (set, j, &row); j++) {
			const s4_result_t *result;
			gint32 value;

			if (!s4_resultrow_get_col (row, 0, &result))
				continue;

			if (!s4_val_get_int (s4_result_get_val (result), &value))
				continue;

			xmmsv_list_append_int (id_list, value);

			g_hash_table_insert (id_table,
			                     GINT_TO_POINTER (value),
			                     GINT_TO_POINTER (1));
		}

		s4_resultset_free (set);

		xmmsv_list_iter_next (it);
	}

	entry = xmmsv_build_dict (XMMSV_DICT_ENTRY_INT ("type", SORT_TYPE_LIST),
	                          XMMSV_DICT_ENTRY ("list", id_list),
	                          XMMSV_DICT_END);

	xmmsv_list_append (order, entry);
	xmmsv_unref (entry);

	return create_idlist_filter (session, id_table);
}

static s4_condition_t *
union_unordered_condition (xmms_medialib_session_t *session, xmmsv_t *coll,
                           xmms_fetch_info_t *fetch)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *operands, *operand;
	s4_condition_t *cond;

	cond = s4_cond_new_combiner (S4_COMBINE_OR);
	operands = xmmsv_coll_operands_get (coll);

	xmmsv_get_list_iter (operands, &it);
	while (xmmsv_list_iter_entry (it, &operand)) {
		s4_condition_t *op_cond;

		op_cond = collection_to_condition (session, operand, fetch, NULL);
		s4_cond_add_operand (cond, op_cond);
		s4_cond_unref (op_cond);

		xmmsv_list_iter_next (it);
	}

	return cond;
}

static s4_condition_t *
universe_condition (xmms_medialib_session_t *session, xmmsv_t *coll,
                    xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	return s4_cond_new_custom_filter ((filter_function_t) universe_filter, NULL, NULL,
	                                  "song_id", NULL, S4_CMP_BINARY, 0, S4_COND_PARENT);
}

static s4_condition_t *
reference_condition (xmms_medialib_session_t *session, xmmsv_t *coll,
                     xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	xmmsv_t *operands, *reference;

	if (is_universe (coll)) {
		return universe_condition (session, coll, fetch, order);
	}

	operands = xmmsv_coll_operands_get (coll);
	if (!xmmsv_list_get (operands, 0, &reference)) {
		xmms_log_error ("Collection references not properly bound, bye bye");
		g_assert_not_reached ();
	}

	return collection_to_condition  (session, reference, fetch, order);
}

/**
 * Convert an xmms2 collection to an S4 condition.
 *
 * @param coll The collection to convert
 * @param fetch Information on what S4 fetches
 * @param order xmmsv_t list that will be filled in with order information
 * as the function recurses.
 * @return A new S4 condition. Must be freed with s4_cond_free
 */
static s4_condition_t *
collection_to_condition (xmms_medialib_session_t *session, xmmsv_t *coll,
                         xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	switch (xmmsv_coll_get_type (coll)) {
		case XMMS_COLLECTION_TYPE_COMPLEMENT:
			return complement_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_HAS:
		case XMMS_COLLECTION_TYPE_MATCH:
		case XMMS_COLLECTION_TYPE_TOKEN:
		case XMMS_COLLECTION_TYPE_EQUALS:
		case XMMS_COLLECTION_TYPE_NOTEQUAL:
		case XMMS_COLLECTION_TYPE_SMALLER:
		case XMMS_COLLECTION_TYPE_SMALLEREQ:
		case XMMS_COLLECTION_TYPE_GREATER:
		case XMMS_COLLECTION_TYPE_GREATEREQ:
			return filter_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_IDLIST:
			return idlist_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_INTERSECTION:
			return intersection_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_LIMIT:
			return limit_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_MEDIASET:
			return mediaset_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_ORDER:
			return order_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_REFERENCE:
			return reference_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_UNION:
			if (has_order (coll)) {
				return union_ordered_condition (session, coll, fetch, order);
			}
			return union_unordered_condition (session, coll, fetch);
		case XMMS_COLLECTION_TYPE_UNIVERSE:
			return universe_condition (session, coll, fetch, order);
		default:
			/* ??? */
			return NULL;
	}
}

/**
 * Internal function that does the actual querying.
 *
 * @param coll The collection to use when querying
 * @param fetch Information on what is being fetched
 * @return An S4 resultset correspoding to the entires in the
 * medialib matching the collection.
 * Must be free with s4_resultset_free
 */
s4_resultset_t *
xmms_medialib_query_recurs (xmms_medialib_session_t *session,
                            xmmsv_t *coll, xmms_fetch_info_t *fetch)
{
	s4_condition_t *cond;
	s4_resultset_t *ret;
	xmmsv_t *order;

	order = xmmsv_new_list ();

	cond = collection_to_condition (session, coll, fetch, order);
	ret = xmms_medialib_session_query (session, fetch->fs, cond);
	s4_cond_free (cond);

	ret = xmms_medialib_result_sort (ret, fetch, order);

	xmmsv_unref (order);

	return ret;
}
