/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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
 *  Manages collections
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <math.h>

#include "xmmspriv/xmms_collection.h"
#include "xmmspriv/xmms_playlist.h"
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"


/* Internal helper structures */

typedef void (*FuncApplyToColl)(xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);

typedef struct {
	guint limit_start;
	guint limit_len;
	GList *order;
	GList *fetch;
	GList *group;
} coll_query_params_t;

typedef struct {
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

typedef struct {
	gchar* name;
	gchar* namespace;
	xmmsc_coll_t *oldtarget;
	xmmsc_coll_t *newtarget;
} coll_rebind_infos_t;

typedef struct {
	gchar* oldname;
	gchar* newname;
	gchar* namespace;
} coll_rename_infos_t;

typedef struct {
	xmms_coll_dag_t *dag;
	FuncApplyToColl func;
	void *udata;
} coll_call_infos_t;

typedef struct {
	gchar *target_name;
	gchar *target_namespace;
	gboolean found;
} coll_refcheck_t;

typedef struct {
	gchar *key;
	xmmsc_coll_t *value;
} coll_table_pair_t;

typedef enum {
	XMMS_COLLECTION_FIND_STATE_UNCHECKED,
	XMMS_COLLECTION_FIND_STATE_MATCH,
	XMMS_COLLECTION_FIND_STATE_NOMATCH,
} coll_find_state_t;


/* Functions */

static void xmms_collection_destroy (xmms_object_t *object);

static gboolean xmms_collection_validate (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, gchar *save_name, gchar *save_namespace);
static gboolean xmms_collection_validate_recurs (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, gchar *save_name, gchar *save_namespace);
static gboolean xmms_collection_unreference (xmms_coll_dag_t *dag, gchar *name, guint nsid);


static xmms_collection_namespace_id_t xmms_collection_get_namespace_id (gchar *namespace);
static gchar* xmms_collection_get_namespace_string (xmms_collection_namespace_id_t nsid);
static gboolean xmms_collection_has_reference_to (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, gchar *tg_name, gchar *tg_ns);

static void xmms_collection_foreach_in_namespace (xmms_coll_dag_t *dag, guint nsid, GHFunc f, void *udata);
static void xmms_collection_apply_to_all_collections (xmms_coll_dag_t *dag, FuncApplyToColl f, void *udata);
static void xmms_collection_apply_to_collection (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, FuncApplyToColl f, void *udata);
static void xmms_collection_apply_to_collection_recurs (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, FuncApplyToColl f, void *udata);

static void call_apply_to_coll (gpointer name, gpointer coll, gpointer udata);
static void prepend_key_string (gpointer key, gpointer value, gpointer udata);
static gboolean value_match_save_key (gpointer key, gpointer val, gpointer udata);

static void bind_all_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);
static void rebind_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);
static void rename_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);
static void strip_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);
static void check_for_reference (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);

static void coll_unref (void *coll);

static GHashTable *xmms_collection_media_info (xmms_coll_dag_t *dag, guint mid, xmms_error_t *err);

static gchar * filter_get_mediainfo_field_string (xmmsc_coll_t *coll, GHashTable *mediainfo);
static gint filter_get_mediainfo_field_int (xmmsc_coll_t *coll, GHashTable *mediainfo);
static gchar * filter_get_operator_value_string (xmmsc_coll_t *coll);
static gint filter_get_operator_value_int (xmmsc_coll_t *coll);
static gboolean filter_get_operator_case (xmmsc_coll_t *coll);

static void build_match_table (gpointer key, gpointer value, gpointer udata);
static gboolean find_unchecked (gpointer name, gpointer value, gpointer udata);
static void build_list_matches (gpointer key, gpointer value, gpointer udata);

static gboolean xmms_collection_media_match (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_match_operand (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_match_reference (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table, gchar *refname, gchar *refns);
static gboolean xmms_collection_media_filter_has (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_match (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_contains (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_smaller (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_greater (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);


static void query_append_uint (coll_query_t *query, guint i);
static void query_append_string (coll_query_t *query, gchar *s);
static void query_append_protect_string (coll_query_t *query, gchar *s);
static void query_append_operand (coll_query_t *query, xmms_coll_dag_t *dag, xmmsc_coll_t *coll);
static void xmms_collection_append_to_query (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, coll_query_t *query);
static coll_query_t* init_query (coll_query_params_t *params);
static GString* xmms_collection_gen_query (coll_query_t *query);
static GString* xmms_collection_get_query (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, coll_query_params_t *params);


XMMS_CMD_DEFINE (collection_get, xmms_collection_get, xmms_coll_dag_t *, COLL, STRING, STRING);
XMMS_CMD_DEFINE (collection_list, xmms_collection_list, xmms_coll_dag_t *, LIST, STRING, NONE);
XMMS_CMD_DEFINE3(collection_save, xmms_collection_save, xmms_coll_dag_t *, NONE, STRING, STRING, COLL);
XMMS_CMD_DEFINE (collection_remove, xmms_collection_remove, xmms_coll_dag_t *, NONE, STRING, STRING);
XMMS_CMD_DEFINE (collection_find, xmms_collection_find, xmms_coll_dag_t *, LIST, UINT32, STRING);
XMMS_CMD_DEFINE3(collection_rename, xmms_collection_rename, xmms_coll_dag_t *, NONE, STRING, STRING, STRING);

XMMS_CMD_DEFINE4(query_ids, xmms_collection_query_ids, xmms_coll_dag_t *, LIST, COLL, UINT32, UINT32, STRINGLIST);
XMMS_CMD_DEFINE6(query_infos, xmms_collection_query_infos, xmms_coll_dag_t *, LIST, COLL, UINT32, UINT32, STRINGLIST, STRINGLIST, STRINGLIST);


GHashTable *
xmms_collection_changed_msg_new (xmms_collection_changed_actions_t type,
                                 gchar *plname, gchar *namespace)
{
	GHashTable *dict;
	xmms_object_cmd_value_t *val;
	dict = g_hash_table_new_full (g_str_hash, g_str_equal, 
	                              NULL, xmms_object_cmd_value_free);
	val = xmms_object_cmd_value_int_new (type);
	g_hash_table_insert (dict, "type", val);
	val = xmms_object_cmd_value_str_new (plname);
	g_hash_table_insert (dict, "name", val);
	val = xmms_object_cmd_value_str_new (namespace);
	g_hash_table_insert (dict, "namespace", val);

	return dict;
}

void
xmms_collection_changed_msg_send (xmms_coll_dag_t *colldag, GHashTable *dict)
{
	g_return_if_fail (colldag);
	g_return_if_fail (dict);

	xmms_object_emit_f (XMMS_OBJECT (colldag),
			    XMMS_IPC_SIGNAL_COLLECTION_CHANGED,
			    XMMS_OBJECT_CMD_ARG_DICT,
			    dict);

	g_hash_table_destroy (dict);
}

#define XMMS_COLLECTION_CHANGED_MSG(type, name, namespace) xmms_collection_changed_msg_send (dag, xmms_collection_changed_msg_new (type, name, namespace))


/** @defgroup Collection Collection
  * @ingroup XMMSServer
  * @brief This is the collection manager.
  *
  * The set of collections is stored as a DAG of collection operators.
  * Each collection namespace contains a list of saved collections,
  * with a pointer to the node in the graph.
  * @{
  */

/** Collection DAG structure */

struct xmms_coll_dag_St {
	xmms_object_t object;

	/* Ref to the playlist object, needed to notify it when a playlist changes */
	xmms_playlist_t *playlist;

	GHashTable *collrefs[XMMS_COLLECTION_NUM_NAMESPACES];

	GMutex *mutex;

};


/** Initializes a new xmms_coll_dag_t.
 *
 * @returns  The newly allocated collection DAG.
 */
xmms_coll_dag_t *
xmms_collection_init (xmms_playlist_t *playlist)
{
	gint i;
	xmms_coll_dag_t *ret;
	
	ret = xmms_object_new (xmms_coll_dag_t, xmms_collection_destroy);
	ret->mutex = g_mutex_new ();
	ret->playlist = playlist;

	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		ret->collrefs[i] = g_hash_table_new_full (g_str_hash, g_str_equal,
		                                          g_free, coll_unref);
	}

	xmms_ipc_object_register (XMMS_IPC_OBJECT_COLLECTION, XMMS_OBJECT (ret));

	xmms_ipc_broadcast_register (XMMS_OBJECT (ret),
	                             XMMS_IPC_SIGNAL_COLLECTION_CHANGED);

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_COLLECTION_GET, 
			     XMMS_CMD_FUNC (collection_get));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_COLLECTION_LIST, 
			     XMMS_CMD_FUNC (collection_list));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_COLLECTION_SAVE, 
			     XMMS_CMD_FUNC (collection_save));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_COLLECTION_REMOVE, 
			     XMMS_CMD_FUNC (collection_remove));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_COLLECTION_FIND, 
			     XMMS_CMD_FUNC (collection_find));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_COLLECTION_RENAME, 
			     XMMS_CMD_FUNC (collection_rename));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_QUERY_IDS, 
			     XMMS_CMD_FUNC (query_ids));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_QUERY_INFOS, 
			     XMMS_CMD_FUNC (query_infos));

	return ret;
}

/** Remove the given collection from the DAG.
 *
 * If to be removed from ALL namespaces, then all matching collections are removed.
 *
 * @param dag  The collection DAG.
 * @param name  The name of the collection to remove.
 * @param namespace  The namespace where the collection to remove is (can be ALL).
 * @param err  If an error occurs, a message is stored in it.
 * @returns  True on success, false otherwise.
 */
gboolean
xmms_collection_remove (xmms_coll_dag_t *dag, gchar *name, gchar *namespace, xmms_error_t *err)
{
	guint nsid;
	gboolean retval;
	guint i;

	XMMS_DBG("COLLECTIONS: Entering xmms_collection_remove");

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return FALSE;
	}

	g_mutex_lock (dag->mutex);

	/* Unreference the matching collection(s) */
	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
			retval = xmms_collection_unreference (dag, name, i) || retval;
		}
	}
	else {
		retval = xmms_collection_unreference (dag, name, nsid);
	}

	g_mutex_unlock (dag->mutex);

	if (retval == FALSE) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "No such collection!");
	}

	return retval;
}

/** Save the given collection in the DAG under the given name in the given namespace.
 *
 * @param dag  The collection DAG in which to save the collection.
 * @param name  The name under which to save the collection.
 * @param namespace  The namespace in which to save th collection.
 * @param coll  The collection structure to save.
 * @param err  If an error occurs, a message is stored in it.
 * @returns  True on success, false otherwise.
 */
gboolean
xmms_collection_save (xmms_coll_dag_t *dag, gchar *name, gchar *namespace,
                      xmmsc_coll_t *coll, xmms_error_t *err)
{
	xmmsc_coll_t *existing;
	guint nsid;
	gchar *alias;
	gchar *newkey = NULL;

	XMMS_DBG("COLLECTIONS: Entering xmms_collection_save");

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return FALSE;
	}
	else if (nsid == XMMS_COLLECTION_NSID_ALL) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "cannot save collection in all namespaces");
		return FALSE;
	}

	/* Validate collection structure */
	if (!xmms_collection_validate (dag, coll, name, namespace)) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection structure");
		return FALSE;
	}

	g_mutex_lock (dag->mutex);

	/* Unreference previously saved collection */
	existing = xmms_collection_get_pointer (dag, name, nsid);
	if (existing != NULL) {
		/* Rebind reference pointers to the new collection */
		coll_rebind_infos_t infos = { name, namespace, existing, coll };
		xmms_collection_apply_to_all_collections (dag, rebind_references, &infos);
	}

	/* Link references in newly saved collection to actual operators */
	xmms_collection_apply_to_collection (dag, coll, bind_all_references, NULL);

	/* Update existing collection in the table */
	if (existing != NULL) {
		while ((alias = xmms_collection_find_alias (dag, nsid,
		                                            existing, NULL)) != NULL) {
			newkey = g_strdup (alias);

			/* update all pairs pointing to the old coll */
			g_hash_table_replace (dag->collrefs[nsid], newkey, coll);
			xmmsc_coll_ref (coll);

			XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_UPDATE,
			                             newkey,
			                             namespace);
		}
	}
	/* Save new collection in the table */
	else {
		newkey = g_strdup (name);
		g_hash_table_replace (dag->collrefs[nsid], newkey, coll);
		xmmsc_coll_ref (coll);

		XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_ADD,
		                             newkey,
		                             namespace);
	}

	g_mutex_unlock (dag->mutex);

	/* If updating a playlist, trigger PLAYLIST_CHANGED */
	if (nsid == XMMS_COLLECTION_NSID_PLAYLISTS) {
		XMMS_PLAYLIST_COLLECTION_CHANGED_MSG (dag->playlist, newkey);
	}

	XMMS_DBG("COLLECTIONS: xmms_collection_save, end");

	return TRUE;
}


/** Retrieve the structure of a given collection.
 *
 * If looking in ALL namespaces, only the collection first found is returned!
 *
 * @param dag  The collection DAG.
 * @param name  The name of the collection to retrieve.
 * @param namespace  The namespace in which to look for the collection.
 * @param err  If an error occurs, a message is stored in it.
 * @returns  The collection structure if found, NULL otherwise.
 */
xmmsc_coll_t *
xmms_collection_get (xmms_coll_dag_t *dag, gchar *name, gchar *namespace, xmms_error_t *err)
{
	xmmsc_coll_t *coll = NULL;
	guint nsid;

	XMMS_DBG("COLLECTIONS: Entering xmms_collection_get");

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return NULL;
	}

	g_mutex_lock (dag->mutex);

	coll = xmms_collection_get_pointer (dag, name, nsid);

	/* Not found! */
	if(coll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such collection");
	}
	/* New reference, will be freed after being put in the return message */
	else {
		xmmsc_coll_ref (coll);
	}
	
	g_mutex_unlock (dag->mutex);

	XMMS_DBG("COLLECTIONS: xmms_collection_get, end");

	return coll;
}


/** Lists the collections in the given namespace.
 *
 * @param dag  The collection DAG.
 * @param namespace  The namespace to list collections from (can be ALL).
 * @returns A newly allocated GList with the list of collection names.
 * Remeber that it is only the LIST that is copied. Not the entries.
 * The entries are however referenced, and must be unreffed!
 */
GList *
xmms_collection_list (xmms_coll_dag_t *dag, gchar *namespace, xmms_error_t *err)
{
	GList *r = NULL;
	guint nsid;

	XMMS_DBG("COLLECTIONS: Entering xmms_collection_list");

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return NULL;
	}

	g_mutex_lock (dag->mutex);

	/* Get the list of collections in the given namespace */
	xmms_collection_foreach_in_namespace (dag, nsid, prepend_key_string, &r);

	g_mutex_unlock (dag->mutex);

	XMMS_DBG("COLLECTIONS: xmms_collection_list, end");

	return r;
}   


GList *
xmms_collection_find (xmms_coll_dag_t *dag, guint mid, gchar *namespace, xmms_error_t *err)
{
	GHashTable *mediainfo;
	GList *ret = NULL;
	guint nsid;
	gchar *open_name;
	GHashTable *match_table;
	xmmsc_coll_t *coll;

	/* Verify namespace */
	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return NULL;
	}
	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "cannot search in all namespaces");
		return NULL;
	}

	/* Prepare the match table of all collections for the given namespace */
	match_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	xmms_collection_foreach_in_namespace (dag, nsid, build_match_table, match_table);

	/* Get all infos for the given mid */
	mediainfo = xmms_collection_media_info (dag, mid, err);

	/* While not all collections have been checked, check next */
	while (g_hash_table_find (match_table, find_unchecked, &open_name) != NULL) {
		coll_find_state_t *match = g_new (coll_find_state_t, 1);
		coll = xmms_collection_get_pointer (dag, open_name, nsid);
		if (xmms_collection_media_match (dag, mediainfo, coll, nsid, match_table)) {
			*match = XMMS_COLLECTION_FIND_STATE_MATCH;
		}
		else {
			*match = XMMS_COLLECTION_FIND_STATE_NOMATCH;
		}
		g_hash_table_replace (match_table, g_strdup (open_name), match);
	}

	/* List matching collections */
	g_hash_table_foreach (match_table, build_list_matches, &ret);
	g_hash_table_destroy (match_table);

	g_hash_table_destroy (mediainfo);

	return ret;
}

gboolean xmms_collection_rename (xmms_coll_dag_t *dag, gchar *from_name,
                                 gchar *to_name, gchar *namespace,
                                 xmms_error_t *err)
{
	gboolean retval;
	guint nsid;
	xmmsc_coll_t *from_coll, *to_coll;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return FALSE;
	}
	else if (nsid == XMMS_COLLECTION_NSID_ALL) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "cannot rename collection in all namespaces");
		return FALSE;
	}

	g_mutex_lock (dag->mutex);

	from_coll = xmms_collection_get_pointer (dag, from_name, nsid);
	to_coll   = xmms_collection_get_pointer (dag, to_name, nsid);

	/* Input validation */
	if (from_coll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such collection");
		retval = FALSE;
	}
	else if (to_coll != NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "a collection already exists with the target name");
		retval = FALSE;
	}
	/* Update collection name everywhere */
	else {
		GHashTable *dict;

		/* insert new pair in hashtable */
		g_hash_table_replace (dag->collrefs[nsid], g_strdup (to_name), from_coll);
		xmmsc_coll_ref (from_coll);

		/* remove old pair from hashtable */
		g_hash_table_remove (dag->collrefs[nsid], from_name);

		/* update name in all reference operators */
		coll_rename_infos_t infos = { from_name, to_name, namespace };
		xmms_collection_apply_to_all_collections (dag, rename_references, &infos);

		/* Send _RENAME signal */
		dict = xmms_collection_changed_msg_new (XMMS_COLLECTION_CHANGED_RENAME,
		                                        from_name, namespace);
		g_hash_table_insert (dict, "newname",
		                     xmms_object_cmd_value_str_new (to_name));
		xmms_collection_changed_msg_send (dag, dict);

		retval = TRUE;
	}

	g_mutex_unlock (dag->mutex);

	return retval;
}


GList *
xmms_collection_query_ids (xmms_coll_dag_t *dag, xmmsc_coll_t *coll,
                           guint lim_start, guint lim_len, GList *order,
                           xmms_error_t *err)
{
	GList *res = NULL;
	GList *ids = NULL;
	GList *n = NULL;

	res = xmms_collection_query_infos (dag, coll, lim_start, lim_len, order, NULL, NULL, err);

	/* FIXME: get an int list directly !   or Int vs UInt? */
	for (n = res; n; n = n->next) {
		xmms_object_cmd_value_t *cmdval = (xmms_object_cmd_value_t*)n->data;
		gint *v = (gint*)g_hash_table_lookup (cmdval->value.dict, "id");
		ids = g_list_prepend (ids, xmms_object_cmd_value_int_new (*v));
		g_free (n->data);
	}

	g_list_free (res);

	return g_list_reverse (ids);
}


GList *
xmms_collection_query_infos (xmms_coll_dag_t *dag, xmmsc_coll_t *coll,
                             guint lim_start, guint lim_len, GList *order,
                             GList *fetch, GList *group, xmms_error_t *err)
{
	GList *res = NULL;
	GString *query;
	coll_query_params_t params = { lim_start, lim_len, order, fetch, group };

	/* validate the collection to query */
	if (!xmms_collection_validate (dag, coll, NULL, NULL)) {
		if (err) {
			xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection structure");
		}
		return NULL;
	}

	g_mutex_lock (dag->mutex);

	query = xmms_collection_get_query (dag, coll, &params);
	
	g_mutex_unlock (dag->mutex);

	XMMS_DBG ("COLLECTIONS: query_infos with %s", query->str);

	/* Run the query */
	xmms_medialib_session_t *session = xmms_medialib_begin ();
	res = xmms_medialib_select (session, query->str, err);
	xmms_medialib_end (session);

	g_string_free (query, TRUE);

	XMMS_DBG ("COLLECTIONS: done");

	return res;
}

/**
 * Update a reference to point to a new collection.
 *
 * @param dag  The collection DAG.
 * @param name The name of the reference to update.
 * @param nsid The namespace in which to locate the reference.
 * @param newtarget The new collection pointed to by the reference.
 */
void
xmms_collection_update_pointer (xmms_coll_dag_t *dag, gchar *name, guint nsid,
                                xmmsc_coll_t *newtarget)
{
	g_hash_table_replace (dag->collrefs[nsid], g_strdup (name), newtarget);
	xmmsc_coll_ref (newtarget);
}


/** Find the collection structure corresponding to the given name in the given namespace.
 *
 * @param dag  The collection DAG.
 * @param collname  The name of the collection to find.
 * @param nsid  The namespace id.
 * @returns  The collection structure if found, NULL otherwise.
 */
xmmsc_coll_t *
xmms_collection_get_pointer (xmms_coll_dag_t *dag, gchar *collname, guint nsid)
{
	gint i;
	xmmsc_coll_t *coll = NULL;

	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES && coll == NULL; ++i) {
			coll = g_hash_table_lookup (dag->collrefs[i], collname);
		}
	}
	else {
		coll = g_hash_table_lookup (dag->collrefs[nsid], collname);
	}

	return coll;
}

gint
xmms_collection_get_int_attr (xmmsc_coll_t *plcoll, gchar *attrname)
{
	gint val;
	gchar *str;
	gchar *endptr;

	if (!xmmsc_coll_attribute_get (plcoll, attrname, &str)) {
		val = -1;
	}
	else {
		val = strtol (str, &endptr, 10);
		if (*endptr != '\0') {
			/* FIXME: Invalid characters in the string ! */
		}
	}

	return val;
}

void
xmms_collection_set_int_attr (xmmsc_coll_t *plcoll, gchar *attrname,
                              gint newval)
{
	gchar *str;

	/* FIXME: Check for (soft) overflow */
	str = g_new (char, XMMS_MAX_INT_ATTRIBUTE_LEN + 1);
	g_snprintf (str, XMMS_MAX_INT_ATTRIBUTE_LEN, "%d", newval);

	xmmsc_coll_attribute_set (plcoll, attrname, str);
	g_free (str);
}


/**
 * Reverse-search the list of collections in the given namespace to
 * find the first pair whose value matches the argument.  If key is
 * not NULL, any pair with the same key will be ignored.
 *
 * @param dag  The collection DAG.
 * @param nsid  The id of the namespace to consider.
 * @param value  The value of the pair to find.
 * @param key  If not NULL, ignore any pair with that key.
 * @return The key of the found pair.
 */
gchar *
xmms_collection_find_alias (xmms_coll_dag_t *dag, guint nsid,
                            xmmsc_coll_t *value, gchar *key)
{
	gchar *otherkey = NULL;
	coll_table_pair_t search_pair = { key, value };

	if (g_hash_table_find (dag->collrefs[nsid], value_match_save_key,
	                       &search_pair) != NULL) {
		otherkey = search_pair.key;
	}

	return otherkey;
}


/**
 * Get a random media entry from the given collection.
 *
 * @param dag  The collection DAG.
 * @param source  The collection to query.
 * @return  A random media from the source collection, or 0 if none found.
 */
xmms_medialib_entry_t
xmms_collection_get_random_media (xmms_coll_dag_t *dag, xmmsc_coll_t *source)
{
	GList *res;
	GList *rorder = NULL;
	xmms_medialib_entry_t mid = 0;

	/* FIXME: Temporary hack to allow custom ordering functions */
	rorder = g_list_prepend (rorder, "~RANDOM()");

	res = xmms_collection_query_ids (dag, source, 0, 1, rorder, NULL);

	g_list_free (rorder);

	if (res != NULL) {
		xmms_object_cmd_value_t *cmdval = (xmms_object_cmd_value_t*)res->data;
		mid = cmdval->value.int32;
		g_list_free (res);
	}

	return mid;
}

/** @} */



/** Free the playlist and other memory in the xmms_playlist_t
 *
 *  This will free all entries in the list!
 */
static void
xmms_collection_destroy (xmms_object_t *object)
{
	gint i;
	xmms_coll_dag_t *dag = (xmms_coll_dag_t *)object;

	g_return_if_fail (dag);

	g_mutex_free (dag->mutex);

	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		g_hash_table_destroy (dag->collrefs[i]);  /* dag is freed here */
	}

	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_COLLECTION_CHANGED);

	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_COLLECTION);
}

/** Validate the given collection against a DAG.
 *
 * @param dag  The collection DAG.
 * @param coll  The collection to validate.
 * @param name  The name under which the collection will be saved (NULL
 *              if none).
 * @param namespace  The namespace in which the collection will be
 *                   saved (NULL if none).
 * @returns  True if the collection is valid, false otherwise.
 */
static gboolean
xmms_collection_validate (xmms_coll_dag_t *dag, xmmsc_coll_t *coll,
                          gchar *save_name, gchar *save_namespace)
{
	/* Special validation checks for the Playlists namespace */
	if (save_namespace != NULL &&
	    strcmp (save_namespace, XMMS_COLLECTION_NS_PLAYLISTS) == 0) {
		/* only accept idlists */
		if (xmmsc_coll_get_type (coll) != XMMS_COLLECTION_TYPE_IDLIST &&
		    xmmsc_coll_get_type (coll) != XMMS_COLLECTION_TYPE_QUEUE &&
		    xmmsc_coll_get_type (coll) != XMMS_COLLECTION_TYPE_PARTYSHUFFLE) {
			return FALSE;
		}
	}

	/* Standard checking of the whole coll DAG */
	return xmms_collection_validate_recurs (dag, coll, save_name,
	                                        save_namespace);
}

/**
 * Internal recursive validation function used to validate the whole
 * graph of a collection.
 */
static gboolean
xmms_collection_validate_recurs (xmms_coll_dag_t *dag, xmmsc_coll_t *coll,
                                 gchar *save_name, gchar *save_namespace)
{
	guint num_operands = 0;
	xmmsc_coll_t *op;
	gchar *attr, *attr2;
	gboolean valid = TRUE;
	xmms_collection_namespace_id_t nsid;

	/* count operands */
	xmmsc_coll_operand_list_save (coll);

	xmmsc_coll_operand_list_first (coll);
	while (xmmsc_coll_operand_list_entry (coll, &op)) {
		num_operands++;
		xmmsc_coll_operand_list_next (coll);
	}

	xmmsc_coll_operand_list_restore (coll);


	/* analyse by type */
	switch (xmmsc_coll_get_type (coll)) {
	case XMMS_COLLECTION_TYPE_REFERENCE:
		/* no operand */
		if (num_operands > 0) {
			XMMS_DBG("COLLECTIONS: validation, num_operands (ref)");
			return FALSE;
		}

		/* check if referenced collection exists */
		xmmsc_coll_attribute_get (coll, "reference", &attr);
		xmmsc_coll_attribute_get (coll, "namespace", &attr2);
		if (strcmp (attr, "All Media") != 0) {
			if (attr == NULL || attr2 == NULL) {
				XMMS_DBG("COLLECTIONS: validation, ref no attr");
				return FALSE;
			}

			nsid = xmms_collection_get_namespace_id (attr2);
			if (nsid == XMMS_COLLECTION_NSID_INVALID) {
				XMMS_DBG("COLLECTIONS: validation, ref invalid ns");
				return FALSE;
			}

			g_mutex_lock (dag->mutex);
			op = xmms_collection_get_pointer (dag, attr, nsid);
			if (op == NULL) {
				XMMS_DBG("COLLECTIONS: validation, ref invalid coll");
				g_mutex_unlock (dag->mutex);
				return FALSE;
			}

			/* check if the referenced coll references this one (loop!) */
			if (save_name && save_namespace &&
			    xmms_collection_has_reference_to (dag, op, save_name, save_namespace)) {
				XMMS_DBG("COLLECTIONS: validation, ref loop");
				g_mutex_unlock (dag->mutex);
				return FALSE;
			}
			g_mutex_unlock (dag->mutex);
		}
		break;

	case XMMS_COLLECTION_TYPE_UNION:
	case XMMS_COLLECTION_TYPE_INTERSECTION:
		/* need operand(s) */
		if (num_operands == 0) {
			XMMS_DBG("COLLECTIONS: validation, num_operands (set)");
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_COMPLEMENT:
		/* one operand */
		if (num_operands != 1) {
			XMMS_DBG("COLLECTIONS: validation, num_operands (complement)");
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_HAS:
		/* one operand */
		if (num_operands != 1) {
			XMMS_DBG("COLLECTIONS: validation, num_operands (filter)");
			return FALSE;
		}

		/* "field" attribute */
		/* with valid value */
		if (!xmmsc_coll_attribute_get (coll, "field", &attr)) {
			XMMS_DBG("COLLECTIONS: validation, field");
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_MATCH:
	case XMMS_COLLECTION_TYPE_CONTAINS:
	case XMMS_COLLECTION_TYPE_SMALLER:
	case XMMS_COLLECTION_TYPE_GREATER:
		/* one operand */
		if (num_operands != 1) {
			XMMS_DBG("COLLECTIONS: validation, num_operands (filter)");
			return FALSE;
		}

		/* "field"/"value" attributes */
		/* with valid values */
		if (!xmmsc_coll_attribute_get (coll, "field", &attr)) {
			XMMS_DBG("COLLECTIONS: validation, field");
			return FALSE;
		}
		/* FIXME: valid fields?
		else if (...) {
			return FALSE;
		}
		*/

		if (!xmmsc_coll_attribute_get (coll, "value", &attr)) {
			XMMS_DBG("COLLECTIONS: validation, value");
			return FALSE;
		}
		break;
	
	case XMMS_COLLECTION_TYPE_IDLIST:
	case XMMS_COLLECTION_TYPE_QUEUE:
		/* no operand */
		if (num_operands > 0) {
			XMMS_DBG("COLLECTIONS: validation, num_operands (idlist)");
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_PARTYSHUFFLE:
		/* one operand */
		if (num_operands != 1) {
			XMMS_DBG("COLLECTIONS: validation, num_operands (party shuffle)");
			return FALSE;
		}
		break;

	/* invalid type */
	default:
		XMMS_DBG("COLLECTIONS: validation, invalid type");
		return FALSE;
		break;
	}


	/* recurse in operands */
	xmmsc_coll_operand_list_save (coll);

	xmmsc_coll_operand_list_first (coll);
	while (xmmsc_coll_operand_list_entry (coll, &op) && valid) {
		if (!xmms_collection_validate_recurs (dag, op, save_name, save_namespace)) {
			valid = FALSE;
		}
		xmmsc_coll_operand_list_next (coll);
	}

	xmmsc_coll_operand_list_restore (coll);

	return valid;
}

/** Try to unreference a collection from a given namespace.
 *
 * @param dag  The collection DAG.
 * @param name  The name of the collection to remove.
 * @param nsid  The namespace in which to look for the collection (yes, redundant).
 * @returns  TRUE if a collection was removed, FALSE otherwise.
 */
static gboolean
xmms_collection_unreference (xmms_coll_dag_t *dag, gchar *name, guint nsid)
{
	xmmsc_coll_t *existing;
	gboolean retval = FALSE;

	existing = g_hash_table_lookup (dag->collrefs[nsid], name);
	if (existing != NULL) {
		gchar *matchkey;
		char *nsname = xmms_collection_get_namespace_string (nsid);
		coll_rebind_infos_t infos = { name, nsname, existing, NULL };

		/* Strip all references to the deleted coll, bind operator directly */
		xmms_collection_apply_to_all_collections (dag, strip_references, &infos);

		/* Remove all pairs pointing to that collection */
		while ((matchkey = xmms_collection_find_alias (dag, nsid,
		                                               existing, NULL)) != NULL) {

			XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_REMOVE,
			                             matchkey,
			                             nsname);

			g_hash_table_remove (dag->collrefs[nsid], matchkey);
		}

		retval = TRUE;
	}

	return retval;
}

/** Find the namespace id corresponding to a namespace string.
 *
 * @param namespace  The namespace string.
 * @returns  The namespace id.
 */
static xmms_collection_namespace_id_t
xmms_collection_get_namespace_id (gchar *namespace)
{
	guint nsid;

	if (strcmp (namespace, XMMS_COLLECTION_NS_ALL) == 0) {
		nsid = XMMS_COLLECTION_NSID_ALL;
	}
	else if (strcmp (namespace, XMMS_COLLECTION_NS_COLLECTIONS) == 0) {
		nsid = XMMS_COLLECTION_NSID_COLLECTIONS;
	}
	else if (strcmp (namespace, XMMS_COLLECTION_NS_PLAYLISTS) == 0) {
		nsid = XMMS_COLLECTION_NSID_PLAYLISTS;
	}
	else {
		nsid = XMMS_COLLECTION_NSID_INVALID;
	}

	return nsid;
}

/** Find the namespace name (string) corresponding to a namespace id.
 *
 * @param nsid  The namespace id.
 * @returns  The namespace name (string).
 */
static gchar*
xmms_collection_get_namespace_string (xmms_collection_namespace_id_t nsid)
{
	gchar *name;

	switch (nsid) {
	case XMMS_COLLECTION_NSID_ALL:
		name = XMMS_COLLECTION_NS_ALL;
		break;
	case XMMS_COLLECTION_NSID_COLLECTIONS:
		name = XMMS_COLLECTION_NS_COLLECTIONS;
		break;
	case XMMS_COLLECTION_NSID_PLAYLISTS:
		name = XMMS_COLLECTION_NS_PLAYLISTS;
		break;

	case XMMS_COLLECTION_NSID_INVALID:
	default:
		name = NULL;
		break;
	}

	return name;
}


/** Check whether a collection structure contains a reference to a given collection.
 * 
 * @param dag  The collection DAG.
 * @param coll  The collection to inspect for reference.
 * @param tg_name  The name of the collection to find a reference to.
 * @param tg_ns  The namespace of the collection to find a reference to.
 * @returns  True if the collection contains a reference to the given
 *           collection, false otherwise
 */
static gboolean
xmms_collection_has_reference_to (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, gchar *tg_name, gchar *tg_ns)
{
	coll_refcheck_t check = { tg_name, tg_ns, FALSE };
	xmms_collection_apply_to_collection (dag, coll, check_for_reference, &check);

	return check.found;
}


/** Apply a function to all the collections in a given namespace.
 *
 * @param dag  The collection DAG.
 * @param nsid  The namespace id.
 * @param f  The function to apply to all the collections.
 * @param udata  Additional user data parameter passed to the function.
 */
static void
xmms_collection_foreach_in_namespace (xmms_coll_dag_t *dag, guint nsid, GHFunc f, void *udata)
{
	gint i;

	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
			g_hash_table_foreach (dag->collrefs[i], f, udata);
		}
	}
	else if (nsid != XMMS_COLLECTION_NSID_INVALID) {
		g_hash_table_foreach (dag->collrefs[nsid], f, udata);
	}
}

/** Apply a function of type #FuncApplyToColl to all the collections in all namespaces.
 *
 * @param dag  The collection DAG.
 * @param f  The function to apply to all the collections.
 * @param udata  Additional user data parameter passed to the function.
 */
static void
xmms_collection_apply_to_all_collections (xmms_coll_dag_t *dag,
                                          FuncApplyToColl f, void *udata)
{
	gint i;
	coll_call_infos_t callinfos = { dag, f, udata };

	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		g_hash_table_foreach (dag->collrefs[i], call_apply_to_coll, &callinfos);
	}
}

/** Apply a function of type #FuncApplyToColl to the given collection.
 *
 * @param dag  The collection DAG.
 * @param coll  The collection on which to apply the function.
 * @param f  The function to apply to all the collections.
 * @param udata  Additional user data parameter passed to the function.
 */
static void
xmms_collection_apply_to_collection (xmms_coll_dag_t *dag,
                                     xmmsc_coll_t *coll,
                                     FuncApplyToColl f, void *udata)
{
	xmms_collection_apply_to_collection_recurs (dag, coll, NULL, f, udata);
}

/* Internal function used for recursion (parent param, NULL by default) */
static void
xmms_collection_apply_to_collection_recurs (xmms_coll_dag_t *dag,
                                            xmmsc_coll_t *coll,
                                            xmmsc_coll_t *parent,
                                            FuncApplyToColl f, void *udata)
{
	xmmsc_coll_t *op;

	/* First apply the function to the operator. */
	f (dag, coll, parent, udata);

	/* Then recurse into the parents (if not a reference) */
	if (xmmsc_coll_get_type (coll) != XMMS_COLLECTION_TYPE_REFERENCE) {
		xmmsc_coll_operand_list_save (coll);

		xmmsc_coll_operand_list_first (coll);
		while (xmmsc_coll_operand_list_entry (coll, &op)) {
			xmms_collection_apply_to_collection_recurs (dag, op, coll, f, udata);
			xmmsc_coll_operand_list_next (coll);
		}

		xmmsc_coll_operand_list_restore (coll);
	}
}


/**
 * Work-around function to call a function on the value of the pair.
 */
static void
call_apply_to_coll (gpointer name, gpointer coll, gpointer udata)
{
	coll_call_infos_t *callinfos = (coll_call_infos_t*)udata;

	xmms_collection_apply_to_collection (callinfos->dag, coll,
	                                     callinfos->func, callinfos->udata);
}

/**
 * Prepend the key string (name) to the udata list.
 */
static void
prepend_key_string (gpointer key, gpointer value, gpointer udata)
{
	xmms_object_cmd_value_t *val;
	GList **list = (GList**)udata;
	val = xmms_object_cmd_value_str_new (g_strdup(key));
	*list = g_list_prepend (*list, val);
}

/**
 * Returns TRUE if the value of the pair is equal to the value stored
 * in the udata structure, and save the corresponding key in that
 * structure.
 */
static gboolean
value_match_save_key (gpointer key, gpointer val, gpointer udata)
{
	gboolean found = FALSE;
	coll_table_pair_t *pair = (coll_table_pair_t*)udata;
	xmmsc_coll_t *coll = (xmmsc_coll_t*)val;

	/* value matching and key not ignored, found! */
	if ((coll == pair->value) &&
		(pair->key == NULL || strcmp (pair->key, key) != 0)) {
		pair->key = key;
		found = TRUE;
	}

	return found;
}

/**
 * If a reference, add the operator of the pointed collection as an
 * operand.
 */
static void
bind_all_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata)
{
	if (xmmsc_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE) {
		xmmsc_coll_t *target;
		gchar *target_name;
		gchar *target_namespace;
		gint   target_nsid;

		xmmsc_coll_attribute_get (coll, "reference", &target_name);
		xmmsc_coll_attribute_get (coll, "namespace", &target_namespace);
		if (target_name == NULL || target_namespace == NULL ||
		    strcmp (target_name, "All Media") == 0) {
			return;
		}

		target_nsid = xmms_collection_get_namespace_id (target_namespace);
		if (target_nsid == XMMS_COLLECTION_NSID_INVALID) {
			return;
		}

		target = xmms_collection_get_pointer (dag, target_name, target_nsid);
		if (target == NULL) {
			return;
		}

		xmmsc_coll_add_operand (coll, target);
		xmmsc_coll_ref (target);
	}
}

/**
 * If a reference, rebind the given operator to the new operator
 * representing the referenced collection (pointers and so are in the
 * udata structure).
 */
static void
rebind_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata)
{
	if (xmmsc_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE) {
		coll_rebind_infos_t *infos;

		gchar *target_name = NULL;
		gchar *target_namespace = NULL;

		infos = (coll_rebind_infos_t*)udata;

		/* FIXME: Or only compare operand vs oldtarget ? */

		xmmsc_coll_attribute_get (coll, "reference", &target_name);
		xmmsc_coll_attribute_get (coll, "namespace", &target_namespace);
		if (strcmp (infos->name, target_name) != 0 ||
		    strcmp (infos->namespace, target_namespace) != 0) {
			return;
		}

		xmmsc_coll_remove_operand (coll, infos->oldtarget);
		xmmsc_coll_unref (infos->oldtarget);

		xmmsc_coll_add_operand (coll, infos->newtarget);
		xmmsc_coll_ref (infos->newtarget);
	}
}

/**
 * If a reference with matching name, rename it according to the
 * rename infos in the udata structure.
 */
static void
rename_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata)
{
	if (xmmsc_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE) {
		coll_rename_infos_t *infos;

		gchar *target_name = NULL;
		gchar *target_namespace = NULL;

		infos = (coll_rename_infos_t*)udata;

		xmmsc_coll_attribute_get (coll, "reference", &target_name);
		xmmsc_coll_attribute_get (coll, "namespace", &target_namespace);
		if (strcmp (infos->oldname, target_name) == 0 &&
		    strcmp (infos->namespace, target_namespace) == 0) {
			xmmsc_coll_attribute_set (coll, "reference", infos->newname);
		}
	}
}

/**
 * Strip reference operators to the given collection by rebinding the
 * parent directly to the pointed operator.
 */
static void
strip_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata)
{
	if (xmmsc_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE) {
		coll_rebind_infos_t *infos;

		gchar *target_name = NULL;
		gchar *target_namespace = NULL;

		infos = (coll_rebind_infos_t*)udata;

		/* FIXME: Or only compare operand vs oldtarget ? */

		xmmsc_coll_attribute_get (coll, "reference", &target_name);
		xmmsc_coll_attribute_get (coll, "namespace", &target_namespace);
		if (strcmp (infos->name, target_name) != 0 ||
		    strcmp (infos->namespace, target_namespace) != 0) {
			return;
		}

		/* Rebind parent to ref'd coll directly, effectively strip reference */
		xmmsc_coll_remove_operand (coll, infos->oldtarget);
		xmmsc_coll_unref (infos->oldtarget);

		if (parent != NULL) {
			xmmsc_coll_remove_operand (parent, coll);
			xmmsc_coll_unref (coll);

			xmmsc_coll_add_operand (parent, infos->oldtarget);
			xmmsc_coll_ref (infos->oldtarget);
		}
	}
}

/**
 * Check if the current operator is a reference to a given collection,
 * and if so, update the structure passed as userdata.
 */
static void
check_for_reference (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata)
{
	coll_refcheck_t *check = (coll_refcheck_t*)udata;
	if (xmmsc_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE && !check->found) {
		gchar *target_name, *target_namespace;

		xmmsc_coll_attribute_get (coll, "reference", &target_name);
		xmmsc_coll_attribute_get (coll, "namespace", &target_namespace);
		if (strcmp (check->target_name, target_name) == 0 &&
		    strcmp (check->target_namespace, target_namespace) == 0) {
			check->found = TRUE;
		}
		else {
			xmmsc_coll_t *op;
			xmmsc_coll_operand_list_save (coll);
			xmmsc_coll_operand_list_first (coll);
			if (xmmsc_coll_operand_list_entry (coll, &op)) {
				xmms_collection_apply_to_collection_recurs (dag, op, coll,
				                                            check_for_reference,
				                                            udata);
			}
			xmmsc_coll_operand_list_restore (coll);
		}
	}
}


/** Forwarding function to fix type warnings.
 *
 * @param coll  The collection to unref.
 */
static void
coll_unref (void *coll)
{
	xmmsc_coll_unref (coll);
}




/* ============  FIND / COLLECTION MATCH FUNCTIONS ============ */

static void
build_match_table (gpointer key, gpointer value, gpointer udata)
{
	GHashTable *match_table = udata;
	coll_find_state_t *match = g_new (coll_find_state_t, 1);
	*match = XMMS_COLLECTION_FIND_STATE_UNCHECKED;
	g_hash_table_replace (match_table, g_strdup (key), match);
}

static gboolean
find_unchecked (gpointer name, gpointer value, gpointer udata)
{
	coll_find_state_t *match = value;
	gchar **open = udata;
	*open = name;
	return (*match == XMMS_COLLECTION_FIND_STATE_UNCHECKED);
}

static void
build_list_matches (gpointer key, gpointer value, gpointer udata)
{
	gchar *coll_name = key;
	coll_find_state_t *state = value;
	GList **list = udata;
	if (*state == XMMS_COLLECTION_FIND_STATE_MATCH) {
		*list = g_list_prepend (*list, xmms_object_cmd_value_str_new (coll_name));
	}
}


static gboolean
xmms_collection_media_match (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                             xmmsc_coll_t *coll, guint nsid,
                             GHashTable *match_table)
{
	gboolean match = FALSE;
	xmmsc_coll_t *op;
	gchar *attr1 = NULL, *attr2 = NULL;
	xmms_object_cmd_value_t *cmdval;
	guint32 *idlist;
	gint i;
	gint id;

	switch (xmmsc_coll_get_type (coll)) {
	case XMMS_COLLECTION_TYPE_REFERENCE:
		/* FIXME: Check, clean */
		if (xmmsc_coll_attribute_get (coll, "reference", &attr1)) {
			if (strcmp (attr1, "All Media") == 0) {
				match = TRUE;
			}
			else if (xmmsc_coll_attribute_get (coll, "namespace", &attr2)) {
				match = xmms_collection_media_match_reference (dag, mediainfo,
				                                               coll, nsid,
				                                               match_table,
				                                               attr1, attr2);
			}
		}
		break;

	case XMMS_COLLECTION_TYPE_UNION:
		/* if ANY matches */
		xmmsc_coll_operand_list_save (coll);
		xmmsc_coll_operand_list_first (coll);
		while (!match && xmmsc_coll_operand_list_entry (coll, &op)) {
			match = xmms_collection_media_match (dag, mediainfo, op, nsid, match_table);
			xmmsc_coll_operand_list_next (coll);
		}
		xmmsc_coll_operand_list_restore (coll);
		break;

	case XMMS_COLLECTION_TYPE_INTERSECTION:
		/* if ALL match */
		xmmsc_coll_operand_list_save (coll);
		xmmsc_coll_operand_list_first (coll);
		while (xmmsc_coll_operand_list_entry (coll, &op)) {
			match = match && xmms_collection_media_match (dag, mediainfo, op,
			                                              nsid, match_table);
			xmmsc_coll_operand_list_next (coll);
		}
		xmmsc_coll_operand_list_restore (coll);
		break;

	case XMMS_COLLECTION_TYPE_COMPLEMENT:
		/* invert result from operand */
		match = !xmms_collection_media_match_operand (dag, mediainfo, coll,
                                                      nsid, match_table);
		break;

	case XMMS_COLLECTION_TYPE_HAS:
		match = xmms_collection_media_filter_has (dag, mediainfo, coll,
		                                          nsid, match_table);
		break;

	case XMMS_COLLECTION_TYPE_MATCH:
		match = xmms_collection_media_filter_match (dag, mediainfo, coll,
		                                            nsid, match_table);
		break;

	case XMMS_COLLECTION_TYPE_CONTAINS:
		match = xmms_collection_media_filter_contains (dag, mediainfo, coll,
		                                               nsid, match_table);
		break;

	case XMMS_COLLECTION_TYPE_SMALLER:
		match = xmms_collection_media_filter_smaller (dag, mediainfo, coll,
		                                              nsid, match_table);
		break;

	case XMMS_COLLECTION_TYPE_GREATER:
		match = xmms_collection_media_filter_greater (dag, mediainfo, coll,
		                                              nsid, match_table);
		break;

	case XMMS_COLLECTION_TYPE_IDLIST:
	case XMMS_COLLECTION_TYPE_QUEUE:
	case XMMS_COLLECTION_TYPE_PARTYSHUFFLE:
		/* check if id in idlist */
		cmdval = g_hash_table_lookup (mediainfo, "id");
		if (cmdval != NULL) {
			id = cmdval->value.int32;
			idlist = xmmsc_coll_get_idlist (coll);
			for (i = 0; idlist[i] != 0; i++) {
				/* stop if mid in the list */
				if (idlist[i] == id) {
					match = TRUE;
					break;
				}
			}
		}
		break;

	/* invalid type */
	default:
		XMMS_DBG("invalid collection operator in xmms_collection_media_match");
		g_assert_not_reached ();
		break;
	}
	
	return match;
}

static gboolean
xmms_collection_media_match_reference (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                       xmmsc_coll_t *coll, guint nsid,
                                       GHashTable *match_table,
                                       gchar *refname, gchar *refns)
{
	gboolean match;
	guint refnsid;
	coll_find_state_t *matchstate;

	/* Same NS, should be in the match table */
	refnsid = xmms_collection_get_namespace_id (refns);
	if (refnsid == nsid) {
		matchstate = g_hash_table_lookup (match_table, refname);
		if (matchstate == XMMS_COLLECTION_FIND_STATE_UNCHECKED) {
			/* Check ref'd collection match status and save it */
			matchstate = g_new (coll_find_state_t, 1);
			match = xmms_collection_media_match_operand (dag,
			                                             mediainfo,
			                                             coll, nsid,
			                                             match_table);

			if (match) {
				*matchstate = XMMS_COLLECTION_FIND_STATE_MATCH;
			}
			else {
				*matchstate = XMMS_COLLECTION_FIND_STATE_NOMATCH;
			}
						
			g_hash_table_replace (match_table, g_strdup (refname), matchstate);
		}
		else {
			match = (*matchstate == XMMS_COLLECTION_FIND_STATE_MATCH);
		}
	}
	/* In another NS, just check if it matches */
	else {
		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);
	}

	return match;
}

static gboolean
xmms_collection_media_match_operand (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                     xmmsc_coll_t *coll, guint nsid,
                                     GHashTable *match_table)
{
	xmmsc_coll_t *op;
	gboolean match = FALSE;

	xmmsc_coll_operand_list_save (coll);
	xmmsc_coll_operand_list_first (coll);
	if (xmmsc_coll_operand_list_entry (coll, &op)) {
		match = xmms_collection_media_match (dag, mediainfo, op, nsid, match_table);
	}
	xmmsc_coll_operand_list_restore (coll);

	return match;
}

static GHashTable *
xmms_collection_media_info (xmms_coll_dag_t *dag, guint mid, xmms_error_t *err)
{
	GList *res;
	GList *n;
	GHashTable *infos;
	gchar *name;
	xmms_object_cmd_value_t *cmdval;
	xmms_object_cmd_value_t *value;
	guint state;

	res = xmms_medialib_info (NULL, mid, err);

	/* Transform the list into a HashMap */
	infos = g_hash_table_new_full (g_str_hash, g_str_equal,
	                               g_free, xmms_object_cmd_value_free);
	for (state = 0, n = res; n; state = (state + 1) % 3, n = n->next) {
		switch (state) {
		case 0:  /* source */
			break;

		case 1:  /* prop name */
			cmdval = n->data;
			name = g_strdup (cmdval->value.string);
			break;

		case 2:  /* prop value */
			value = xmms_object_cmd_value_copy (n->data);
		
			/* Only insert the first source */
			if (g_hash_table_lookup (infos, name) == NULL) {
				g_hash_table_replace (infos, name, value);
			}
			break;
		}

		xmms_object_cmd_value_free (n->data);
	}

	g_list_free (res);

	return infos;
}


static gchar *
filter_get_mediainfo_field_string (xmmsc_coll_t *coll, GHashTable *mediainfo)
{
	gchar *mediaval = NULL;
	gchar *attr;
	xmms_object_cmd_value_t *cmdval;

	if (xmmsc_coll_attribute_get (coll, "field", &attr)) {
		cmdval = g_hash_table_lookup (mediainfo, attr);
		if (cmdval != NULL) {
			mediaval = cmdval->value.string;
		}
	}

	return mediaval;
}

static gint
filter_get_mediainfo_field_int (xmmsc_coll_t *coll, GHashTable *mediainfo)
{
	gint mediaval = -1;	/* FIXME: if error, missing? */
	gchar *attr;
	xmms_object_cmd_value_t *cmdval;

	if (xmmsc_coll_attribute_get (coll, "field", &attr)) {
		cmdval = g_hash_table_lookup (mediainfo, attr);
		if (cmdval != NULL) {
			mediaval = cmdval->value.int32;
		}
	}

	return mediaval;
}

static gchar *
filter_get_operator_value_string (xmmsc_coll_t *coll)
{
	gchar *attr;
	return (xmmsc_coll_attribute_get (coll, "value", &attr) ? attr : NULL);
}

static gint
filter_get_operator_value_int (xmmsc_coll_t *coll)
{
	/* FIXME: if error, missing? */
	return xmms_collection_get_int_attr (coll, "value");
}

static gboolean
filter_get_operator_case (xmmsc_coll_t *coll)
{
	gchar *attr;
	gboolean case_sensit = FALSE;

	if (xmmsc_coll_attribute_get (coll, "case-sensitive", &attr)) {
		case_sensit = (strcmp (attr, "true") == 0);
	}

	return case_sensit;
}

static gboolean
xmms_collection_media_filter_has (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                  xmmsc_coll_t *coll, guint nsid,
                                  GHashTable *match_table)
{
	gboolean match = FALSE;
	gchar *mediaval;
	mediaval = filter_get_mediainfo_field_string (coll, mediainfo);

	/* If operator matches, recurse upwards in the operand */
	if (mediaval != NULL) {
		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);
	}

	return match;
}

static gboolean
xmms_collection_media_filter_match (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                    xmmsc_coll_t *coll, guint nsid,
                                    GHashTable *match_table)
{
	gboolean match = FALSE;
	gchar *mediaval;
	gchar *opval;
	gboolean case_sens;

	mediaval = filter_get_mediainfo_field_string (coll, mediainfo);
	opval = filter_get_operator_value_string (coll);
	case_sens = filter_get_operator_case (coll);

	if (mediaval && opval) {
		if (case_sens) {
			match = (strcmp (mediaval, opval) == 0);
		}
		else {
			match = (g_ascii_strcasecmp (mediaval, opval) == 0);
		}
	}

	/* If operator matches, recurse upwards in the operand */
	if (match) {
		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);
	}

	return match;
}

static gboolean
xmms_collection_media_filter_contains (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                       xmmsc_coll_t *coll, guint nsid,
                                       GHashTable *match_table)
{
	gboolean match;
	gchar *mediaval;
	gchar *opval;
	gboolean case_sens;
	gint i, len;

	mediaval = filter_get_mediainfo_field_string (coll, mediainfo);
	opval = filter_get_operator_value_string (coll);
	case_sens = filter_get_operator_case (coll);

	/* Prepare values */
	if (case_sens) {
		mediaval = g_strdup (mediaval);
	}
	else {
		opval = g_utf8_strdown (opval, strlen (opval));
		mediaval = g_utf8_strdown (mediaval, strlen (mediaval));
	}

	/* Transform SQL wildcards to GLib wildcards */
	for (i = 0, len = strlen (mediaval); i < len; i++) {
		switch (mediaval[i]) {
		case '%':  mediaval[i] = '*';  break;
		case '_':  mediaval[i] = '?';  break;
		default:                       break;
		}
	}

	match = g_pattern_match_simple (opval, mediaval);

	if (case_sens) {
		g_free (opval);
	}
	g_free (mediaval);

	/* If operator matches, recurse upwards in the operand */
	if (match) {
		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);
	}

	return match;
}

static gboolean
xmms_collection_media_filter_smaller (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                      xmmsc_coll_t *coll, guint nsid,
                                      GHashTable *match_table)
{
	gboolean match = FALSE;
	gint mediaval;
	gint opval;

	mediaval = filter_get_mediainfo_field_int (coll, mediainfo);
	opval = filter_get_operator_value_int (coll);

	/* If operator matches, recurse upwards in the operand */
	if (mediaval < opval) { /* FIXME: error, missing value ? */
		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);
	}

	return match;
}

static gboolean
xmms_collection_media_filter_greater (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                      xmmsc_coll_t *coll, guint nsid,
                                      GHashTable *match_table)
{
	gboolean match = FALSE;
	gint mediaval;
	gint opval;

	mediaval = filter_get_mediainfo_field_int (coll, mediainfo);
	opval = filter_get_operator_value_int (coll);

	/* If operator matches, recurse upwards in the operand */
	if (mediaval > opval) { /* FIXME: error, missing value ? */
		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);
	}

	return match;
}






/* ============  QUERY FUNCTIONS ============ */

/** Register a (unique) field alias in the query structure and return
 * the corresponding identifier.
 *
 * @param query  The query object to insert the alias in.
 * @param field  The name of the property that will correspond to the alias.
 * @param optional  Whether the property can be optional (i.e. LEFT JOIN)
 * @return  The id of the alias.
 */
guint
query_make_alias (coll_query_t *query, gchar *field, gboolean optional)
{
	coll_query_alias_t *alias;
	alias = g_hash_table_lookup (query->aliases, field);

	/* Insert in the hashtable */
	if (alias == NULL) {
		gchar *fieldkey = g_strdup (field);

		alias = g_new (coll_query_alias_t, 1);
		alias->optional = optional;

		if (query->alias_base == NULL && !optional) {  /* Found a base */
			alias->id = 0;
			query->alias_base = fieldkey;
		}
		else {
			alias->id = query->alias_count;
			query->alias_count++;
		}

		g_hash_table_insert (query->aliases, fieldkey, alias);
	}
	/* If was not optional but now is, update */
	else if (!alias->optional && optional) {
		alias->optional = optional;
	}

	return alias->id;
}

/* Find the canonical name of a field (strip flags, if any) */
static gchar *
canonical_field_name (gchar *field) {
	if (*field == '-') {
		field++;
	}
	else if (*field == '~') {
		field = NULL;
	}
	return field;
}

/* Initialize a query structure */
static coll_query_t*
init_query (coll_query_params_t *params)
{
	GList *n;
	coll_query_t *query;

	query = g_new (coll_query_t, 1);
	if(query == NULL) {
		return NULL;
	}

	query->aliases = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                        g_free, g_free);

	query->alias_count = 1;
	query->alias_base = NULL;
	query->conditions = g_string_new (NULL);
	query->params = params;

	/* Prepare aliases for the order/group fields */
	for (n = query->params->order; n; n = n->next) {
		gchar *field = canonical_field_name (n->data);
		if (field != NULL) {
			query_make_alias (query, field, TRUE);
		}
	}
	for (n = query->params->group; n; n = n->next) {
		query_make_alias (query, n->data, TRUE);
	}
	for (n = query->params->fetch; n; n = n->next) {
		query_make_alias (query, n->data, TRUE);
	}

	return query;
}

/* Copied from xmmsc_sqlite_prepare_string */
gchar *
sqlite_prepare_string (const gchar *input) {
	gchar *output;
	gint outsize, nquotes = 0;
	gint i, o;

	for (i = 0; input[i] != '\0'; i++) {
		if (input[i] == '\'') {
			nquotes++;
		}
	}

	outsize = strlen(input) + nquotes + 2 + 1; /* 2 quotes to terminate the string , and one \0 in the end */
	output = g_new (gchar, outsize);

	if (output == NULL) {
		return NULL;
	}

	i = o = 0;
	output[o++] = '\'';
	while (input[i] != '\0') {
		output[o++] = input[i];
		if (input[i++] == '\'') {
			output[o++] = '\'';
		}
	}
	output[o++] = '\'';
	output[o] = '\0';

	return output;
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
query_append_string (coll_query_t *query, gchar *s)
{
	g_string_append (query->conditions, s);
}

static void
query_append_protect_string (coll_query_t *query, gchar *s)
{
	gchar *preps;
	if((preps = sqlite_prepare_string (s)) != NULL) {  /* FIXME: Return oom error */
		query_append_string (query, preps);
		g_free (preps);
	}
}

static void
query_append_operand (coll_query_t *query, xmms_coll_dag_t *dag, xmmsc_coll_t *coll)
{
	xmmsc_coll_t *op;

	xmmsc_coll_operand_list_save (coll);
	xmmsc_coll_operand_list_first (coll);
	if (xmmsc_coll_operand_list_entry (coll, &op)) {
		xmms_collection_append_to_query (dag, op, query);
	}
	else {
		/* FIXME: Hackish to accomodate the absence of operand (i.e. All Media refs)  */
		query_append_string (query, "1");
	}
	xmmsc_coll_operand_list_restore (coll);
}

static void
query_append_intersect_operand (coll_query_t *query, xmms_coll_dag_t *dag, xmmsc_coll_t *coll)
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
	guint id;
	gboolean optional;

	if (type == XMMS_COLLECTION_TYPE_HAS) {
		optional = TRUE;
	}
	else {
		optional = FALSE;
	}

	id = query_make_alias (query, key, optional);

	switch (type) {
	/* escape strings */
	case XMMS_COLLECTION_TYPE_MATCH:
	case XMMS_COLLECTION_TYPE_CONTAINS:
		if (case_sens) {
			g_string_append_printf (query->conditions, "m%u.value", id);
		}
		else {
			g_string_append_printf (query->conditions, "LOWER(m%u.value)", id);
		}

		if (type == XMMS_COLLECTION_TYPE_MATCH) {
			query_append_string (query, "=");
		}
		else {
			query_append_string (query, " LIKE ");
		}

		if (case_sens) {
			query_append_protect_string (query, value);
		}
		else {
			query_append_string (query, "LOWER(");
			query_append_protect_string (query, value);
			query_append_string (query, ")");
		}
		break;

	/* do not escape numerical values */
	case XMMS_COLLECTION_TYPE_SMALLER:
	case XMMS_COLLECTION_TYPE_GREATER:
		g_string_append_printf (query->conditions, "m%u.value", id);
		if (type == XMMS_COLLECTION_TYPE_SMALLER) {
			query_append_string (query, " < ");
		}
		else {
			query_append_string (query, " > ");
		}
		query_append_string (query, value);
		break;

	case XMMS_COLLECTION_TYPE_HAS:
		g_string_append_printf (query->conditions, "m%u.value is not null", id);
		break;

	/* Called with invalid type? */
	default:
		g_assert_not_reached ();
		break;
	}
}

/* Recursively append conditions corresponding to the given collection to the query. */
static void
xmms_collection_append_to_query (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, coll_query_t *query)
{
	gint i;
	xmmsc_coll_t *op;
	guint *idlist;
	gchar *attr1, *attr2, *attr3;
	gboolean case_sens;

	xmmsc_coll_type_t type = xmmsc_coll_get_type (coll);
	switch (type) {
	case XMMS_COLLECTION_TYPE_REFERENCE:
		query_append_operand (query, dag, coll);
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
			}
			else {
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
	case XMMS_COLLECTION_TYPE_MATCH:
	case XMMS_COLLECTION_TYPE_CONTAINS:
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
		XMMS_DBG("Cannot append invalid collection operator!");
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

	if (alias->id > 0) {
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
			alias = g_hash_table_lookup (query->aliases, canon_field);
			if (alias != NULL) {
				g_string_append_printf (qstring, "m%u.value", alias->id);
			}
		}

		/* special prefix for ordering */
		if (*field == '-') {
			g_string_append (qstring, " DESC");
		}
		/* FIXME: Temporary hack to allow custom ordering functions */
		else if (*field == '~') {
			g_string_append (qstring, field + 1);
		}
	}
}

static void
query_string_append_fetch (coll_query_t *query, GString *qstring)
{
	GList *n;
	guint id;

	for (n = query->params->fetch; n; n = n->next) {
		id = query_make_alias (query, n->data, TRUE);
		g_string_append_printf (qstring, ", m%u.value AS %s", id, (gchar*)n->data);
	}

}

/* Generate a query string from a query structure. */
static GString*
xmms_collection_gen_query (coll_query_t *query)
{
	GString *qstring;

	/* If no alias base yet (m0), select the 'url' property as base */
	if (query->alias_base == NULL) {
		query_make_alias (query, "url", FALSE);
	}

	/* Append select and joins */
	qstring = g_string_new ("SELECT DISTINCT m0.id");
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
		}
		else {
			g_string_append_printf (qstring, " LIMIT %u",
			                        query->params->limit_len);
		}
	}

	return qstring;
}

/* Generate a query string from a collection and query parameters. */
static GString*
xmms_collection_get_query (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, coll_query_params_t* params)
{
	GString *qstring;
	coll_query_t *query;

	query = init_query (params);

	xmms_collection_append_to_query (dag, coll, query);
	qstring = xmms_collection_gen_query (query);

	/* free the query struct */
	g_hash_table_destroy (query->aliases);
	g_string_free (query->conditions, TRUE);
	g_free (query);

	return qstring;
}
