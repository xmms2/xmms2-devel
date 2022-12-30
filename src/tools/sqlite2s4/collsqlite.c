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


#include <xmmspriv/xmms_collection.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

struct db_info {
	sqlite3 *db;
	GHashTable **ht;
	xmmsv_t *coll;
};

typedef enum {
	XMMS_COLLECTION1_TYPE_REFERENCE,
	XMMS_COLLECTION1_TYPE_UNION,
	XMMS_COLLECTION1_TYPE_INTERSECTION,
	XMMS_COLLECTION1_TYPE_COMPLEMENT,
	XMMS_COLLECTION1_TYPE_HAS,
	XMMS_COLLECTION1_TYPE_EQUALS,
	XMMS_COLLECTION1_TYPE_MATCH,
	XMMS_COLLECTION1_TYPE_SMALLER,
	XMMS_COLLECTION1_TYPE_GREATER,
	XMMS_COLLECTION1_TYPE_IDLIST,
	XMMS_COLLECTION1_TYPE_QUEUE,
	XMMS_COLLECTION1_TYPE_PARTYSHUFFLE,
	XMMS_COLLECTION1_TYPE_LAST = XMMS_COLLECTION1_TYPE_PARTYSHUFFLE
} xmmsv_coll1_type_t;

void collection_restore (sqlite3 *db, GHashTable **ht);
static xmmsv_t *xmms_collection_dbread_operator (sqlite3 *db, gint id, xmmsv_coll_type_t type);

/* Creates a coll2 collection from a coll1 type */
static xmmsv_t *
create_coll (xmmsv_coll1_type_t type)
{
	xmmsv_t *ret;
	xmmsv_coll_type_t new_type = XMMS_COLLECTION1_TYPE_REFERENCE;
	const char *idlist_type = NULL;

	switch (type) {
	case XMMS_COLLECTION1_TYPE_REFERENCE:
		new_type = XMMS_COLLECTION_TYPE_REFERENCE;
		break;
	case XMMS_COLLECTION1_TYPE_UNION:
		new_type = XMMS_COLLECTION_TYPE_UNION;
		break;
	case XMMS_COLLECTION1_TYPE_INTERSECTION:
		new_type = XMMS_COLLECTION_TYPE_INTERSECTION;
		break;
	case XMMS_COLLECTION1_TYPE_COMPLEMENT:
		new_type = XMMS_COLLECTION_TYPE_COMPLEMENT;
		break;

	case XMMS_COLLECTION1_TYPE_HAS:
		new_type = XMMS_COLLECTION_TYPE_HAS;
		break;
	case XMMS_COLLECTION1_TYPE_EQUALS:
		new_type = XMMS_COLLECTION_TYPE_EQUALS;
		break;
	case XMMS_COLLECTION1_TYPE_MATCH:
		new_type = XMMS_COLLECTION_TYPE_MATCH;
		break;
	case XMMS_COLLECTION1_TYPE_SMALLER:
		new_type = XMMS_COLLECTION_TYPE_SMALLER;
		break;
	case XMMS_COLLECTION1_TYPE_GREATER:
		new_type = XMMS_COLLECTION_TYPE_GREATER;
		break;

	case XMMS_COLLECTION1_TYPE_IDLIST:
		idlist_type = "list";
		break;
	case XMMS_COLLECTION1_TYPE_QUEUE:
		idlist_type = "queue";
		break;
	case XMMS_COLLECTION1_TYPE_PARTYSHUFFLE:
		idlist_type = "pshuffle";
		break;
	}

	if (idlist_type != NULL) {
		ret = xmmsv_new_coll (XMMS_COLLECTION_TYPE_IDLIST);
		xmmsv_coll_attribute_set_string (ret, "type", idlist_type);
	} else {
		ret = xmmsv_new_coll (new_type);
	}

	return ret;
}

/* Augment the collection with new attributes needed for coll2 */
static xmmsv_t *
augment_coll (xmmsv_t *coll)
{
	xmmsv_t *ret = coll;
	const char *key;

	switch (xmmsv_coll_get_type (coll)) {
	case XMMS_COLLECTION_TYPE_HAS:
	case XMMS_COLLECTION_TYPE_MATCH:
	case XMMS_COLLECTION_TYPE_TOKEN:
	case XMMS_COLLECTION_TYPE_EQUALS:
	case XMMS_COLLECTION_TYPE_NOTEQUAL:
	case XMMS_COLLECTION_TYPE_SMALLER:
	case XMMS_COLLECTION_TYPE_SMALLEREQ:
	case XMMS_COLLECTION_TYPE_GREATER:
	case XMMS_COLLECTION_TYPE_GREATEREQ:
		if (xmmsv_coll_attribute_get_string (coll, "field", &key)
		    && strcmp (key, "id") == 0) {
			xmmsv_coll_attribute_set_string (coll, "type", "id");
		} else {
			xmmsv_coll_attribute_set_string (coll, "type", "value");
		}
		break;

	case XMMS_COLLECTION_TYPE_REFERENCE:
		if (xmmsv_coll_attribute_get_string (coll, "reference", &key)
		    && strcmp (key, "All Media") == 0) {
			ret = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
			xmmsv_unref (coll);
		}
		break;

	default:
		break;
	}

	return ret;
}

static int
restore_callback (void *userdata, int columns, char **col_strs, char **col_names)
{
	struct db_info *info = userdata;
	static gint previd = -1;
	gint id = 0, type = 0, nsid = 0, i;
	const gchar *label = NULL;
	static xmmsv_t *coll = NULL;

	for (i = 0; i < columns; i++) {
		if (!strcmp (col_names[i], "id")) {
			id = atoi (col_strs[i]);
		} else if (!strcmp (col_names[i], "type")) {
			type = atoi (col_strs[i]);
		} else if (!strcmp (col_names[i], "nsid")) {
			nsid = atoi (col_strs[i]);
		} else if (!strcmp (col_names[i], "label")) {
			label = col_strs[i];
		}
	}

	/* Do not duplicate operator if same id */
	if (previd < 0 || id != previd) {
		coll = xmms_collection_dbread_operator (info->db, id, type);
		previd = id;
	}
	else {
		xmmsv_ref (coll);  /* New label references the coll */
	}

	g_hash_table_replace (info->ht[nsid], g_strdup (label), coll);

	return 0;
}

void
collection_restore (sqlite3 *db, GHashTable **ht)
{
	const gchar *query;
	struct db_info info;

	info.db = db;
	info.ht = ht;

	/* Fetch all label-coll_operator for all namespaces, register in table */
	query = "SELECT op.id AS id, lbl.name AS label, "
	        "       lbl.namespace AS nsid, op.type AS type "
	        "FROM CollectionOperators AS op, CollectionLabels as lbl "
	        "WHERE op.id=lbl.collid "
	        "ORDER BY id";
	sqlite3_exec (db, query, restore_callback, &info, NULL);
}

static int
attribute_callback (void *userdata, int cols, char **col_strs, char **col_names)
{
	xmmsv_t *coll = userdata;
	const gchar *key = NULL, *value = NULL;
	int i;

	for (i = 0; i < cols; i++) {
		if (!strcmp (col_names[i], "key")) {
			key = col_strs[i];
		} else if (!strcmp (col_names[i], "value")) {
			value = col_strs[i];
		}
	}
	xmmsv_coll_attribute_set_string (coll, key, value);

	return 0;
}
static int
idlist_callback (void *userdata, int cols, char **col_strs, char **col_names)
{
	xmmsv_t *coll = userdata;
	int i;

	for (i = 0; i < cols; i++) {
		if (!strcmp (col_names[i], "mid")) {
			xmmsv_coll_idlist_append (coll, atoi (col_strs[i]));
		}
	}

	return 0;
}
static int
operator_callback (void *userdata, int cols, char **col_strs, char **col_names)
{
	struct db_info *info = userdata;
	xmmsv_t *coll = info->coll;
	xmmsv_t *op;
	int i;
	gint id = 0;
	gint type = 0;

	for (i = 0; i < cols; i++) {
		if (!strcmp (col_names[i], "id")) {
			id = atoi (col_strs[i]);
		} else if (!strcmp (col_names[i], "type")) {
			type = atoi (col_strs[i]);
		}
	}


	op = xmms_collection_dbread_operator (info->db, id, type);
	xmmsv_coll_add_operand (coll, op);
	xmmsv_unref (op);

	return 0;
}

static xmmsv_t *
xmms_collection_dbread_operator (sqlite3 *db, gint id, xmmsv_coll_type_t type)
{
	xmmsv_t *coll;
	gchar query[256];
	struct db_info info;

	coll = create_coll ((xmmsv_coll1_type_t) type);

	/* Retrieve the attributes */
	g_snprintf (query, sizeof (query),
	            "SELECT attr.key AS key, attr.value AS value "
	            "FROM CollectionOperators AS op, CollectionAttributes AS attr "
	            "WHERE op.id=%d AND attr.collid=op.id", id);

	sqlite3_exec (db, query, attribute_callback, coll, NULL);

	/* Retrieve the idlist */
	g_snprintf (query, sizeof (query),
	            "SELECT idl.mid AS mid "
	            "FROM CollectionOperators AS op, CollectionIdlists AS idl "
	            "WHERE op.id=%d AND idl.collid=op.id "
	            "ORDER BY idl.position", id);

	sqlite3_exec (db, query, idlist_callback, coll, NULL);

	/* Retrieve the operands */
	g_snprintf (query, sizeof (query),
	            "SELECT op.id AS id, op.type AS type "
	            "FROM CollectionOperators AS op, CollectionConnections AS conn "
	            "WHERE conn.to_id=%d AND conn.from_id=op.id", id);

	info.db = db;
	info.coll = coll;
	sqlite3_exec (db, query, operator_callback, &info, NULL);

	return augment_coll (coll);
}
