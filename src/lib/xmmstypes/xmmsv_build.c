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

#include <stdio.h>

#include "xmmspriv/xmmsv.h"
#include "xmmsc/xmmsc_util.h"


/**
 * Helper function to build a list #xmmsv_t containing the
 * strings from the input array.
 *
 * @param array An array of C strings. Must be NULL-terminated if num
 *              is -1.
 * @param num The optional number of elements to read from the array. Set to
 *            -1 if the array is NULL-terminated.
 * @return An #xmmsv_t containing the list of strings. Must be
 *         unreffed manually when done.
 */
xmmsv_t *
xmmsv_make_stringlist (char *array[], int num)
{
	xmmsv_t *list, *elem;
	int i;

	list = xmmsv_new_list ();
	if (array) {
		for (i = 0; (num >= 0 && i < num) || array[i]; i++) {
			elem = xmmsv_new_string (array[i]);
			xmmsv_list_append (list, elem);
			xmmsv_unref (elem);
		}
	}

	return list;
}

xmmsv_t *
xmmsv_build_dict_va (const char *firstkey, va_list ap)
{
	const char *key;
	xmmsv_t *val, *res;

	res = xmmsv_new_dict ();
	if (!res)
		return NULL;

	key = firstkey;
	do {
		val = va_arg (ap, xmmsv_t *);

		if (!xmmsv_dict_set (res, key, val)) {
			xmmsv_unref (res);
			res = NULL;
			break;
		}
		xmmsv_unref (val);
		key = va_arg (ap, const char *);
	} while (key);

	return res;
}

xmmsv_t *
xmmsv_build_dict (const char *firstkey, ...)
{
	va_list ap;
	xmmsv_t *res;

	va_start (ap, firstkey);
	res = xmmsv_build_dict_va (firstkey, ap);
	va_end (ap);

	return res;
}

xmmsv_t *
xmmsv_build_list_va (xmmsv_t *first_entry, va_list ap)
{
	xmmsv_t *val, *res;

	res = xmmsv_new_list ();
	if (!res)
		return NULL;

	val = first_entry;

	while (val) {
		if (!xmmsv_list_append (res, val)) {
			xmmsv_unref (res);
			res = NULL;
			break;
		}

		xmmsv_unref (val);

		val = va_arg (ap, xmmsv_t *);
	}

	return res;
}

xmmsv_t *
xmmsv_build_list (xmmsv_t *first_entry, ...)
{
	va_list ap;
	xmmsv_t *res;

	va_start (ap, first_entry);
	res = xmmsv_build_list_va (first_entry, ap);
	va_end (ap);

	return res;
}

/**
 * Creates an organize fetch specification that may be passed to xmmsc_coll_query.
 * It takes a dict with key-value pairs where the values are fetch specifications.
 *
 * @return An organize fetch specification
 */
xmmsv_t *
xmmsv_build_organize (xmmsv_t *data)
{
	xmmsv_t *res;

	x_return_val_if_fail (data != NULL, NULL);
	res = xmmsv_new_dict ();

	if (res != NULL) {
		xmmsv_dict_set_string (res, "type", "organize");
		xmmsv_dict_set (res, "data", data);
		xmmsv_unref (data);
	}

	return res;
}

/**
 * Creates a metadata fetch specification.
 *
 * @param fields A list of fields to fetch, or NULL to fetch everything
 * @param get A list of what to get ("id", "key", "value", "source")
 * @param aggregate The aggregation function to use
 * @param sourcepref A list of sources, first one has the highest priority
 * @return A metadata fetch specification
 */
xmmsv_t *xmmsv_build_metadata (xmmsv_t *fields, xmmsv_t *get, const char *aggregate, xmmsv_t *sourcepref)
{
	xmmsv_t *res = xmmsv_new_dict ();
	if (res == NULL)
		return NULL;

	xmmsv_dict_set_string (res, "type", "metadata");

	if (fields != NULL) {
		if (xmmsv_get_type (fields) == XMMSV_TYPE_STRING) {
			xmmsv_t *list = xmmsv_new_list ();
			xmmsv_list_append (list, fields);
			xmmsv_unref (fields);
			fields = list;
		}
		xmmsv_dict_set (res, "fields", fields);
		xmmsv_unref (fields);
	}
	if (get != NULL) {
		if (xmmsv_get_type (get) == XMMSV_TYPE_STRING) {
			xmmsv_t *list = xmmsv_new_list ();
			xmmsv_list_append (list, get);
			xmmsv_unref (get);
			get = list;
		}
		xmmsv_dict_set (res, "get", get);
		xmmsv_unref (get);
	}
	if (sourcepref != NULL) {
		xmmsv_dict_set (res, "source-preference", sourcepref);
		xmmsv_unref (sourcepref);
	}
	if (aggregate != NULL) {
		xmmsv_dict_set_string (res, "aggregate", aggregate);
	}

	return res;
}

/**
 * Creates a cluster-list fetch specification.
 *
 * @param cluster_by A list of attributes to cluster by
 * @param cluster_data The fetch specifcation to use when filling the list
 * @return A cluster-list fetch specification
 */
xmmsv_t *xmmsv_build_cluster_list (xmmsv_t *cluster_by, xmmsv_t *cluster_field, xmmsv_t *cluster_data)
{
	xmmsv_t *res = xmmsv_new_dict ();
	if (res == NULL)
		return NULL;

	xmmsv_dict_set_string (res, "type", "cluster-list");

	if (cluster_by != NULL) {
		xmmsv_dict_set (res, "cluster-by", cluster_by);
		xmmsv_unref (cluster_by);
	}

	if (cluster_field != NULL) {
		xmmsv_dict_set (res, "cluster-field", cluster_field);
		xmmsv_unref (cluster_field);
	}

	if (cluster_data != NULL) {
		xmmsv_dict_set (res, "data", cluster_data);
		xmmsv_unref (cluster_data);
	}

	return res;
}

/**
 * Creates a cluster-dict fetch specification.
 *
 * @param cluster_by A list of attributes to cluster by
 * @param cluster_data The fetch specifcation to use when filling the list
 * @return A cluster-list fetch specification
 */
xmmsv_t *xmmsv_build_cluster_dict (xmmsv_t *cluster_by, xmmsv_t *cluster_field, xmmsv_t *cluster_data)
{
	xmmsv_t *res = xmmsv_new_dict ();
	if (res == NULL)
		return NULL;

	xmmsv_dict_set_string (res, "type", "cluster-dict");

	if (cluster_by != NULL) {
		xmmsv_dict_set (res, "cluster-by", cluster_by);
		xmmsv_unref (cluster_by);
	}


	if (cluster_field != NULL) {
		xmmsv_dict_set (res, "cluster-field", cluster_field);
		xmmsv_unref (cluster_field);
	}

	if (cluster_data != NULL) {
		xmmsv_dict_set (res, "data", cluster_data);
		xmmsv_unref (cluster_data);
	}

	return res;
}

/**
 * Creates a count fetch specification
 *
 * @return A new count fetch specification
 */
xmmsv_t *xmmsv_build_count ()
{
	xmmsv_t *res = xmmsv_new_dict ();

	xmmsv_dict_set_string (res, "type", "count");
	return res;
}
