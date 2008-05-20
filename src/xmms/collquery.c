/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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


/** @file
 *  Functions to build an SQL query from a collection.
 */

#include <string.h>
#include <glib.h>

#include "xmmspriv/xmms_collquery.h"
#include "xmms/xmms_log.h"


/* Query structures */

typedef struct {
	guint limit_start;
	guint limit_len;
	GList *order;
	GList *fetch;
	GList *group;
} coll_query_params_t;

typedef enum {
	XMMS_QUERY_ALIAS_ID,
	XMMS_QUERY_ALIAS_PROP,
} coll_query_alias_type_t;

typedef struct {
	coll_query_alias_type_t type;
	guint id;
	gboolean optional;
} coll_query_alias_t;

typedef struct {
	GHashTable *aliases;
	guint alias_count;
	gchar *alias_base;
	GString *conditions;
	coll_query_params_t *params;
} coll_query_t;



static coll_query_t* init_query (coll_query_params_t *params);
static void add_fetch_group_aliases (coll_query_t *query, coll_query_params_t *params);
static void destroy_query (coll_query_t* query);
static GString* xmms_collection_gen_query (coll_query_t *query);
static void xmms_collection_append_to_query (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, coll_query_t *query);

static void query_append_uint (coll_query_t *query, guint i);
static void query_append_string (coll_query_t *query, const gchar *s);
static void query_append_protect_string (coll_query_t *query, gchar *s);
static void query_append_operand (coll_query_t *query, xmms_coll_dag_t *dag, xmmsc_coll_t *coll);
static void query_append_intersect_operand (coll_query_t *query, xmms_coll_dag_t *dag, xmmsc_coll_t *coll);
static void query_append_filter (coll_query_t *query, xmmsc_coll_type_t type, gchar *key, gchar *value, gboolean case_sens);
static void query_string_append_joins (gpointer key, gpointer val, gpointer udata);
static void query_string_append_alias_list (coll_query_t *query, GString *qstring, GList *fields);
static void query_string_append_fetch (coll_query_t *query, GString *qstring);
static void query_string_append_alias (GString *qstring, coll_query_alias_t *alias);

static gchar *canonical_field_name (gchar *field);
static gboolean operator_is_allmedia (xmmsc_coll_t *op);
static coll_query_alias_t *query_make_alias (coll_query_t *query, const gchar *field, gboolean optional);
static coll_query_alias_t *query_get_alias (coll_query_t *query, const gchar *field);



/** @defgroup CollectionQuery CollectionQuery
  * @ingroup XMMSServer
  * @brief This module generates queries from collections.
  *
  * @{
  */

/* Generate a query string from a collection and query parameters. */
GString*
xmms_collection_get_query (xmms_coll_dag_t *dag, xmmsc_coll_t *coll,
                           guint limit_start, guint limit_len,
                           GList *order, GList *fetch, GList *group)
{
	GString *qstring;
	coll_query_t *query;
	coll_query_params_t params = { limit_start, limit_len, order, fetch, group };

	query = init_query (&params);
	xmms_collection_append_to_query (dag, coll, query);
	add_fetch_group_aliases (query, &params);

	qstring = xmms_collection_gen_query (query);

	destroy_query (query);

	return qstring;
}


/* Initialize a query structure */
static coll_query_t*
init_query (coll_query_params_t *params)
{
	coll_query_t *query;

	query = g_new (coll_query_t, 1);
	if (query == NULL) {
		return NULL;
	}

	query->aliases = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                        g_free, g_free);

	query->alias_count = 1;
	query->alias_base = NULL;
	query->conditions = g_string_new (NULL);
	query->params = params;

	return query;
}

static void
add_fetch_group_aliases (coll_query_t *query, coll_query_params_t *params)
{
	GList *n;

	/* Prepare aliases for the group/fetch fields */
	for (n = query->params->group; n; n = n->next) {
		query_make_alias (query, n->data, TRUE);
	}
	for (n = query->params->fetch; n; n = n->next) {
		query_make_alias (query, n->data, TRUE);
	}
}

/* Free a coll_query_t object */
static void
destroy_query (coll_query_t* query)
{
	g_hash_table_destroy (query->aliases);
	g_string_free (query->conditions, TRUE);
	g_free (query);
}


/* Generate a query string from a query structure. */
static GString*
xmms_collection_gen_query (coll_query_t *query)
{
	GString *qstring;

	/* If no alias base yet (m0), select the default base property */
	if (query->alias_base == NULL) {
		query_make_alias (query, XMMS_COLLQUERY_DEFAULT_BASE, FALSE);
	}

	/* Append select and joins */
	qstring = g_string_new ("SELECT DISTINCT ");
	query_string_append_fetch (query, qstring);
	g_string_append (qstring, " FROM Media as m0");
	g_hash_table_foreach (query->aliases, query_string_append_joins, qstring);

	/* Append conditions */
	g_string_append_printf (qstring, " WHERE m0.key='%s'", query->alias_base);
	if (query->conditions->len > 0) {
		g_string_append_printf (qstring, " AND %s", query->conditions->str);
	}

	/* Append grouping */
	if (query->params->group != NULL) {
		g_string_append (qstring, " GROUP BY ");
		query_string_append_alias_list (query, qstring, query->params->group);
	}

	/* Append ordering */
	/* FIXME: Ordering is Teh Broken (source?) */
	if (query->params->order != NULL) {
		g_string_append (qstring, " ORDER BY ");
		query_string_append_alias_list (query, qstring, query->params->order);
	}

	/* Append limit */
	if (query->params->limit_len != 0) {
		if (query->params->limit_start ) {
			g_string_append_printf (qstring, " LIMIT %u,%u",
			                        query->params->limit_start,
			                        query->params->limit_len);
		} else {
			g_string_append_printf (qstring, " LIMIT %u",
			                        query->params->limit_len);
		}
	}

	return qstring;
}

/* Recursively append conditions corresponding to the given collection to the query. */
static void
xmms_collection_append_to_query (xmms_coll_dag_t *dag, xmmsc_coll_t *coll,
                                 coll_query_t *query)
{
	gint i;
	xmmsc_coll_t *op;
	guint *idlist;
	gchar *attr1, *attr2, *attr3;
	gboolean case_sens;

	xmmsc_coll_type_t type = xmmsc_coll_get_type (coll);
	switch (type) {
	case XMMS_COLLECTION_TYPE_REFERENCE:
		if (!operator_is_allmedia (coll)) {
			query_append_operand (query, dag, coll);
		} else {
			/* FIXME: Hackish solution to append a ref to All Media */
			query_append_string (query, "1");
		}
		break;

	case XMMS_COLLECTION_TYPE_UNION:
	case XMMS_COLLECTION_TYPE_INTERSECTION:
		i = 0;
		query_append_string (query, "(");

		xmmsc_coll_operand_list_save (coll);
		xmmsc_coll_operand_list_first (coll);
		while (xmmsc_coll_operand_list_entry (coll, &op)) {
			if (i != 0) {
				if (type == XMMS_COLLECTION_TYPE_UNION)
					query_append_string (query, " OR ");
				else
					query_append_string (query, " AND ");
			} else {
				i = 1;
			}
			xmms_collection_append_to_query (dag, op, query);
			xmmsc_coll_operand_list_next (coll);
		}
		xmmsc_coll_operand_list_restore (coll);

		query_append_string (query, ")");
		break;

	case XMMS_COLLECTION_TYPE_COMPLEMENT:
		query_append_string (query, "NOT ");
		query_append_operand (query, dag, coll);
		break;

	case XMMS_COLLECTION_TYPE_HAS:
	case XMMS_COLLECTION_TYPE_EQUALS:
	case XMMS_COLLECTION_TYPE_MATCH:
	case XMMS_COLLECTION_TYPE_SMALLER:
	case XMMS_COLLECTION_TYPE_GREATER:
		xmmsc_coll_attribute_get (coll, "field", &attr1);
		xmmsc_coll_attribute_get (coll, "value", &attr2);
		xmmsc_coll_attribute_get (coll, "case-sensitive", &attr3);
		case_sens = (attr3 != NULL && strcmp (attr3, "true") == 0);

		query_append_string (query, "(");
		query_append_filter (query, type, attr1, attr2, case_sens);

		query_append_intersect_operand (query, dag, coll);
		query_append_string (query, ")");
		break;

	case XMMS_COLLECTION_TYPE_IDLIST:
	case XMMS_COLLECTION_TYPE_QUEUE:
	case XMMS_COLLECTION_TYPE_PARTYSHUFFLE:
		idlist = xmmsc_coll_get_idlist (coll);
		query_append_string (query, "m0.id IN (");
		for (i = 0; idlist[i] != 0; ++i) {
			if (i != 0) {
				query_append_string (query, ",");
			}
			query_append_uint (query, idlist[i]);
		}
		query_append_string (query, ")");
		break;

	/* invalid type */
	default:
		XMMS_DBG ("Cannot append invalid collection operator!");
		g_assert_not_reached ();
		break;
	}

}


/** Register a (unique) field alias in the query structure and return
 * the corresponding alias pointer.
 *
 * @param query  The query object to insert the alias in.
 * @param field  The name of the property that will correspond to the alias.
 * @param optional  Whether the property can be optional (i.e. LEFT JOIN)
 * @return  The alias pointer.
 */
static coll_query_alias_t *
query_make_alias (coll_query_t *query, const gchar *field, gboolean optional)
{
	coll_query_alias_t *alias;
	alias = g_hash_table_lookup (query->aliases, field);

	/* Insert in the hashtable */
	if (alias == NULL) {
		gchar *fieldkey = g_strdup (field);

		alias = g_new (coll_query_alias_t, 1);
		alias->optional = optional;
		alias->id = 0;

		if (strcmp (field, "id") == 0) {
			alias->type = XMMS_QUERY_ALIAS_ID;
		} else {
			alias->type = XMMS_QUERY_ALIAS_PROP;

			/* Found a base */
			if (query->alias_base == NULL &&
			    (!optional || strcmp (field, XMMS_COLLQUERY_DEFAULT_BASE) == 0)) {
				alias->id = 0;
				query->alias_base = fieldkey;
			} else {
				alias->id = query->alias_count;
				query->alias_count++;
			}
		}

		g_hash_table_insert (query->aliases, fieldkey, alias);

	/* If was not optional but now is, update */
	} else if (!alias->optional && optional) {
		alias->optional = optional;
	}

	return alias;
}

static coll_query_alias_t *
query_get_alias (coll_query_t *query, const gchar *field)
{
	return g_hash_table_lookup (query->aliases, field);
}

/* Find the canonical name of a field (strip flags, if any) */
static gchar *
canonical_field_name (gchar *field) {
	if (*field == '-') {
		field++;
	} else if (*field == '~') {
		field = NULL;
	}
	return field;
}


/* Determine whether the given operator is a reference to "All Media" */
static gboolean
operator_is_allmedia (xmmsc_coll_t *op)
{
	gchar *target_name;
	xmmsc_coll_attribute_get (op, "reference", &target_name);
	return (target_name != NULL && strcmp (target_name, "All Media") == 0);
}

static void
query_append_uint (coll_query_t *query, guint i)
{
	g_string_append_printf (query->conditions, "%u", i);
}

static void
query_append_string (coll_query_t *query, const gchar *s)
{
	g_string_append (query->conditions, s);
}

static void
query_append_protect_string (coll_query_t *query, gchar *s)
{
	gchar *preps;
	if ((preps = sqlite_prepare_string (s)) != NULL) {  /* FIXME: Return oom error */
		query_append_string (query, preps);
		g_free (preps);
	}
}

static void
query_append_operand (coll_query_t *query, xmms_coll_dag_t *dag, xmmsc_coll_t *coll)
{
	xmmsc_coll_t *op;
	gchar *target_name;
	gchar *target_ns;
	guint  target_nsid;

	xmmsc_coll_operand_list_save (coll);
	xmmsc_coll_operand_list_first (coll);
	if (!xmmsc_coll_operand_list_entry (coll, &op)) {
		/* Ref'd coll not saved as operand, look for it */
		if (xmmsc_coll_attribute_get (coll, "reference", &target_name) &&
		    xmmsc_coll_attribute_get (coll, "namespace", &target_ns)) {

			target_nsid = xmms_collection_get_namespace_id (target_ns);
			op = xmms_collection_get_pointer (dag, target_name, target_nsid);
		}
	}
	xmmsc_coll_operand_list_restore (coll);

	/* Append reference operator */
	if (op != NULL) {
		xmms_collection_append_to_query (dag, op, query);

	/* Cannot find reference, append dummy TRUE */
	} else {
		query_append_string (query, "1");
	}
}

static void
query_append_intersect_operand (coll_query_t *query, xmms_coll_dag_t *dag,
                                xmmsc_coll_t *coll)
{
	xmmsc_coll_t *op;

	xmmsc_coll_operand_list_save (coll);
	xmmsc_coll_operand_list_first (coll);
	if (xmmsc_coll_operand_list_entry (coll, &op)) {
		if (!operator_is_allmedia (op)) {
			query_append_string (query, " AND ");
			xmms_collection_append_to_query (dag, op, query);
		}
	}
	xmmsc_coll_operand_list_restore (coll);
}

/* Append a filtering clause on the field value, depending on the operator type. */
static void
query_append_filter (coll_query_t *query, xmmsc_coll_type_t type,
                     gchar *key, gchar *value, gboolean case_sens)
{
	coll_query_alias_t *alias;
	gboolean optional;
	gchar *temp;
	gint i;

	if (type == XMMS_COLLECTION_TYPE_HAS) {
		optional = TRUE;
	} else {
		optional = FALSE;
	}

	alias = query_make_alias (query, key, optional);

	switch (type) {
	/* escape strings */
	case XMMS_COLLECTION_TYPE_EQUALS:
	case XMMS_COLLECTION_TYPE_MATCH:
		if (case_sens) {
			query_string_append_alias (query->conditions, alias);
		} else {
			query_append_string (query, "(");
			query_string_append_alias (query->conditions, alias);
			query_append_string (query, " COLLATE NOCASE)");
		}

		if (type == XMMS_COLLECTION_TYPE_EQUALS) {
			query_append_string (query, "=");
		} else {
			if (case_sens) {
				query_append_string (query, " GLOB ");
			} else {
				query_append_string (query, " LIKE ");
			}
		}

		if (type == XMMS_COLLECTION_TYPE_MATCH && !case_sens) {
			temp = g_strdup(value);
			for (i = 0; temp[i]; i++) {
				switch (temp[i]) {
					case '*': temp[i] = '%'; break;
					case '?': temp[i] = '_'; break;
					default :                break;
				}
			}
			query_append_protect_string (query, temp);
			g_free(temp);
		} else {
			query_append_protect_string (query, value);
		}
		break;

	/* do not escape numerical values */
	case XMMS_COLLECTION_TYPE_SMALLER:
	case XMMS_COLLECTION_TYPE_GREATER:
		query_string_append_alias (query->conditions, alias);
		if (type == XMMS_COLLECTION_TYPE_SMALLER) {
			query_append_string (query, " < ");
		} else {
			query_append_string (query, " > ");
		}
		query_append_string (query, value);
		break;

	case XMMS_COLLECTION_TYPE_HAS:
		query_string_append_alias (query->conditions, alias);
		query_append_string (query, " is not null");
		break;

	/* Called with invalid type? */
	default:
		g_assert_not_reached ();
		break;
	}
}

/* Append SELECT joins to the argument string for each alias of the hashtable. */
static void
query_string_append_joins (gpointer key, gpointer val, gpointer udata)
{
	gchar *field;
	GString *qstring;
	coll_query_alias_t *alias;

	field = key;
	qstring = (GString*)udata;
	alias = (coll_query_alias_t*)val;

	if ((alias->id > 0) && (alias->type == XMMS_QUERY_ALIAS_PROP)) {
		if (alias->optional) {
			g_string_append_printf (qstring, " LEFT");
		}

		g_string_append_printf (qstring,
		                        " JOIN Media as m%u ON m0.id=m%u.id"
		                        " AND m%u.key='%s'",
		                        alias->id, alias->id, alias->id, field);
	}
}

/* Given a list of fields, append the corresponding aliases to the argument string. */
static void
query_string_append_alias_list (coll_query_t *query, GString *qstring, GList *fields)
{
	GList *n;
	gboolean first = TRUE;

	for (n = fields; n; n = n->next) {
		coll_query_alias_t *alias;
		gchar *field = n->data;
		gchar *canon_field = canonical_field_name (field);

		if (first) first = FALSE;
		else {
			g_string_append (qstring, ", ");
		}

		if (canon_field != NULL) {
			alias = query_get_alias (query, canon_field);
			if (alias != NULL) {
				query_string_append_alias (qstring, alias);
			} else {
				if (*field != '~') {
					if (strcmp(canon_field, "id") == 0) {
						g_string_append (qstring, "m0.id");
					} else {
						g_string_append_printf (qstring,
							"(SELECT value FROM Media WHERE id = m0.id AND "
							"key='%s')", canon_field);
					}
				}
			}
		}

		/* special prefix for ordering */
		if (*field == '-') {
			g_string_append (qstring, " DESC");
		} else if (*field == '~') {
			/* FIXME: Temporary hack to allow custom ordering functions */
			g_string_append (qstring, field + 1);
		}
	}
}

static void
query_string_append_fetch (coll_query_t *query, GString *qstring)
{
	GList *n;
	coll_query_alias_t *alias;
	gboolean first = TRUE;

	for (n = query->params->fetch; n; n = n->next) {
		alias = query_make_alias (query, n->data, TRUE);

		if (first) first = FALSE;
		else {
			g_string_append (qstring, ", ");
		}

		query_string_append_alias (qstring, alias);
		g_string_append_printf (qstring, " AS %s", (gchar*)n->data);
	}
}

static void
query_string_append_alias (GString *qstring, coll_query_alias_t *alias)
{
	switch (alias->type) {
	case XMMS_QUERY_ALIAS_PROP:
		g_string_append_printf (qstring, "m%u.value", alias->id);
		break;

	case XMMS_QUERY_ALIAS_ID:
		g_string_append (qstring, "m0.id");
		break;

	default:
		break;
	}
}

/**
 * @}
 */
