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
#include "xmmspriv/xmms_medialib.h"
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"


/* Internal helper structures */

typedef void (*FuncApplyToColl)(xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);

typedef struct {
	guint limit_start;
	guint limit_len;
	GList *order;
	GList *group;
	GList *fetch;
} coll_query_params_t;

typedef struct {
	guint alias_count;
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


static void query_append_uint (coll_query_t *query, guint i);
static void query_append_string (coll_query_t *query, gchar *s);
static void query_append_protect_string (coll_query_t *query, gchar *s);
static void query_append_currfield (coll_query_t *query);
static void query_append_currvalue (coll_query_t *query);
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
/* FIXME: Arrays, num args, etc?
XMMS_CMD_DEFINE6(query_infos, xmms_collection_query_infos, xmms_coll_dag_t *, LIST, COLL, UINT32, UINT32, LIST, LIST, LIST);
*/


static GHashTable *
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

	XMMS_DBG ("COLL-CHANGED: %d for %s in %s", type, plname, namespace);

	return dict;
}

static void
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

	GHashTable *collrefs[XMMS_COLLECTION_NUM_NAMESPACES];

	GMutex *mutex;

};


/** Initializes a new xmms_coll_dag_t.
 *
 * @returns  The newly allocated collection DAG.
 */
xmms_coll_dag_t *
xmms_collection_init (void)
{
	gint i;
	xmms_coll_dag_t *ret;
	
	ret = xmms_object_new (xmms_coll_dag_t, xmms_collection_destroy);
	ret->mutex = g_mutex_new ();

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

/*
	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_QUERY_INFOS, 
			     XMMS_CMD_FUNC (query_infos));
*/
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

	/* Save new collection in the table */
	if (existing != NULL) {
		coll_table_pair_t search_pair = { NULL, existing };
		while (g_hash_table_find (dag->collrefs[nsid], value_match_save_key,
		                          &search_pair) != NULL) {
			/* update all pairs pointing to the old coll */
			gchar *newkey = g_strdup (search_pair.key);
			g_hash_table_replace (dag->collrefs[nsid], newkey, coll);
			xmmsc_coll_ref (coll);

			XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_UPDATE,
			                             newkey,
			                             namespace);
		}
	}
	else {
		g_hash_table_replace (dag->collrefs[nsid], g_strdup (name), coll);
		xmmsc_coll_ref (coll);

		XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_ADD,
		                             name,
		                             namespace);
	}

	g_mutex_unlock (dag->mutex);

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
	/* FIXME: Code that later */
	return NULL;
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
	GString *query;
	coll_query_params_t params = { lim_start, lim_len, order };

	/* validate the collection to query */
	if (!xmms_collection_validate (dag, coll, NULL, NULL)) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection structure");
		return NULL;
	}

	g_mutex_lock (dag->mutex);

	query = xmms_collection_get_query (dag, coll, &params);
	
	g_mutex_unlock (dag->mutex);

	XMMS_DBG ("COLLECTIONS: query_ids with %s", query->str);

	/* Run the query */
	xmms_medialib_session_t *session = xmms_medialib_begin ();
	res = xmms_medialib_select (session, query->str, err);
	xmms_medialib_end (session);

	g_string_free (query, TRUE);

	/* FIXME: get an int list directly !   or Int vs UInt? */
	for (n = res; n; n = n->next) {
		xmms_object_cmd_value_t *cmdval = (xmms_object_cmd_value_t*)n->data;
		gint *v = (gint*)g_hash_table_lookup (cmdval->value.dict, "id");
		ids = g_list_prepend (ids, xmms_object_cmd_value_int_new (*v));
		g_free (n->data);
	}

	g_list_free (res);

	XMMS_DBG ("COLLECTIONS: done");

	return g_list_reverse (ids);
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
		if (xmmsc_coll_get_type (coll) != XMMS_COLLECTION_TYPE_IDLIST) {
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
		/* no operand */
		if (num_operands > 0) {
			XMMS_DBG("COLLECTIONS: validation, num_operands (idlist)");
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
		char *nsname = xmms_collection_get_namespace_string (nsid);
		coll_rebind_infos_t infos = { name, nsname, existing, NULL };
		coll_table_pair_t search_pair = { NULL, existing };

		/* Strip all references to the deleted coll, bind operator directly */
		xmms_collection_apply_to_all_collections (dag, strip_references, &infos);

		/* Remove all pairs pointing to that collection */
		while (g_hash_table_find (dag->collrefs[nsid], value_match_save_key,
		                          &search_pair) != NULL) {

			XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_REMOVE,
			                             search_pair.key,
			                             nsname);

			g_hash_table_remove (dag->collrefs[nsid], search_pair.key);
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

	if (coll == pair->value) {
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




/* ============  QUERY FUNCTIONS ============ */

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
query_append_currfield (coll_query_t *query)
{
	g_string_append_printf (query->conditions, "m%u.key", query->alias_count);
}

static void
query_append_currvalue (coll_query_t *query)
{
	g_string_append_printf (query->conditions, "m%u.value", query->alias_count);
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
	xmmsc_coll_operand_list_restore (coll);
}

static void
query_append_filter (coll_query_t *query, xmmsc_coll_type_t type, gchar *value)
{
		query_append_currvalue (query);
		switch (type) {
		case XMMS_COLLECTION_TYPE_MATCH:
			query_append_string (query, "=");
			query_append_protect_string (query, value);
			break;

		case XMMS_COLLECTION_TYPE_CONTAINS:
			query_append_string (query, " LIKE ");
			query_append_protect_string (query, value);
			break;

		/* FIXME: don't escape numerical values? */
		case XMMS_COLLECTION_TYPE_SMALLER:
			query_append_string (query, " < ");
			query_append_protect_string (query, value);
			break;

		/* FIXME: don't escape numerical values? */
		case XMMS_COLLECTION_TYPE_GREATER:
			query_append_string (query, " > ");
			query_append_protect_string (query, value);
			break;

		/* Called with invalid type? */
		default:
			g_assert_not_reached ();
			break;
		}
}

static void
xmms_collection_append_to_query (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, coll_query_t *query)
{
	gint i;
	xmmsc_coll_t *op;
	guint *idlist;
	gchar *attr1, *attr2;

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

	case XMMS_COLLECTION_TYPE_MATCH:
	case XMMS_COLLECTION_TYPE_CONTAINS:
	case XMMS_COLLECTION_TYPE_SMALLER:
	case XMMS_COLLECTION_TYPE_GREATER:
		xmmsc_coll_attribute_get (coll, "field", &attr1);
		xmmsc_coll_attribute_get (coll, "value", &attr2);

		query_append_string (query, "(");
		query_append_currfield (query);
		query_append_string (query, "=");
		query_append_protect_string (query, attr1);
		query_append_string (query, " AND ");
		query_append_filter (query, type, attr2);
		query_append_string (query, ")");
		query->alias_count++;
		break;
	
	case XMMS_COLLECTION_TYPE_IDLIST:
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
		break;
	}

}

static coll_query_t*
init_query (coll_query_params_t *params)
{
	coll_query_t *query;

	query = g_new (coll_query_t, 1);
	if(query == NULL) {
		return NULL;
	}

	query->alias_count = 1;
	query->conditions = g_string_new (NULL);
	query->params = params;

	return query;
}


static GString*
xmms_collection_gen_query (coll_query_t *query)
{
	GString *qstring;
	guint i;
	GList *n;

	/* Append select */
	qstring = g_string_new ("SELECT DISTINCT m0.id FROM Media as m0");
	for (i = 1; i < query->alias_count; i++) {
		g_string_append_printf (qstring,
		                        " JOIN Media as m%u ON m0.id = m%u.id", i, i);
	}

	/* FIXME: select fetch fields OR ids? */

	/* FIXME: DISABLED! Ordering is Teh Broken (source?)
	for (i = 1, n = query->params->order; n; i++, n = n->next) {
		g_string_append_printf (qstring, " LEFT JOIN Media as j%u"
		                                 " ON m0.id=j%u.id"
		                                 " AND j%u.key='%s'",
		                        i, i, i, (gchar*)n->data);
	}
	*/

	/* Append conditions */
	if (query->conditions->len > 0) {
		g_string_append_printf (qstring, " WHERE %s", query->conditions->str);
	}

	/* Append ordering */
	/* FIXME: DISABLED! Ordering is Teh Broken (source?)
	if (query->params->order != NULL) {
		g_string_append (qstring, " ORDER BY ");
		for (i = 1, n = query->params->order; n; i++, n = n->next) {
			if (i > 1) {
				g_string_append (qstring, ",");
			}
			g_string_append_printf (qstring, "j%u.value", i);
		}
	}
	*/

	/* FIXME: group, fetch => modular handling, with order too ! */

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


static GString*
xmms_collection_get_query (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, coll_query_params_t* params)
{
	GString *qstring;
	coll_query_t *query;

	query = init_query (params);

	xmms_collection_append_to_query (dag, coll, query);
	qstring = xmms_collection_gen_query (query);

	/* free the query struct */
	g_string_free (query->conditions, TRUE);
	g_free (query);

	return qstring;
}
