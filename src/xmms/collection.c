/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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

#include <xmmspriv/xmms_collection.h>
#include <xmmspriv/xmms_xform.h>
#include <xmmspriv/xmms_streamtype.h>
#include <xmmspriv/xmms_medialib.h>
#include <xmms/xmms_ipc.h>
#include <xmms/xmms_log.h>


/* Internal helper structures */

typedef struct {
	const gchar *name;
	const gchar *namespace;
	xmmsv_t *oldtarget;
	xmmsv_t *newtarget;
} coll_rebind_infos_t;

typedef struct {
	const gchar* oldname;
	const gchar* newname;
	const gchar* namespace;
} coll_rename_infos_t;

typedef struct {
	xmms_coll_dag_t *dag;
	FuncApplyToColl func;
	void *udata;
} coll_call_infos_t;

typedef struct {
	const gchar *target_name;
	const gchar *target_namespace;
	gboolean found;
} coll_refcheck_t;

typedef struct {
	const gchar *key;
	xmmsv_t *value;
} coll_table_pair_t;

typedef enum {
	XMMS_COLLECTION_FIND_STATE_UNCHECKED,
	XMMS_COLLECTION_FIND_STATE_MATCH,
	XMMS_COLLECTION_FIND_STATE_NOMATCH,
} coll_find_state_t;

typedef struct add_metadata_from_tree_user_data_St {
	xmms_medialib_entry_t entry;
	xmms_medialib_session_t *session;
	const gchar* src;
} add_metadata_from_tree_user_data_t;


/* Functions */

static void xmms_collection_destroy (xmms_object_t *object);

static gboolean xmms_collection_validate (xmms_coll_dag_t *dag, xmmsv_t *coll, const gchar *save_name, const gchar *save_namespace, const gchar **err);
static gboolean xmms_collection_validate_recurs (xmms_coll_dag_t *dag, xmmsv_t *coll, const gchar *save_name, const gchar *save_namespace, const gchar **err);
static gboolean xmms_collection_unreference (xmms_coll_dag_t *dag, const gchar *name, xmms_collection_namespace_id_t nsid);

static gboolean xmms_collection_has_reference_to (xmms_coll_dag_t *dag, xmmsv_t *coll, const gchar *tg_name, const gchar *tg_ns);

static void xmms_collection_apply_to_collection_recurs (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, FuncApplyToColl f, void *udata);

static void call_apply_to_coll (gpointer name, gpointer coll, gpointer udata);
static void prepend_key_string (gpointer key, gpointer value, gpointer udata);
static gboolean value_match_save_key (gpointer key, gpointer val, gpointer udata);

static void rebind_references (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata);
static void rename_references (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata);
static void strip_references (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata);
static void check_for_reference (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata);
static void bind_all_references (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata);
static void unbind_all_references (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata);

static void coll_unref (void *coll);

static void build_match_table (gpointer key, gpointer value, gpointer udata);
static gboolean find_unchecked (gpointer name, gpointer value, gpointer udata);
static void build_list_matches (gpointer key, gpointer value, gpointer udata);

static xmmsv_t * xmms_collection_client_get (xmms_coll_dag_t *dag, const gchar *collname, const gchar *namespace, xmms_error_t *error);
static xmmsv_t * xmms_collection_client_list (xmms_coll_dag_t *dag, const gchar *namespace, xmms_error_t *error);
static void xmms_collection_client_save (xmms_coll_dag_t *dag, const gchar *name, const gchar *namespace, xmmsv_t *coll, xmms_error_t *error);
static void xmms_collection_client_remove (xmms_coll_dag_t *dag, const gchar *collname, const gchar *namespace, xmms_error_t *error);
static xmmsv_t * xmms_collection_client_find (xmms_coll_dag_t *dag, gint32 mid, const gchar *namespace, xmms_error_t *error);
static void xmms_collection_client_rename (xmms_coll_dag_t *dag, const gchar *from_name, const gchar *to_name, const gchar *namespace, xmms_error_t *error);

static xmmsv_t * xmms_collection_client_query_infos (xmms_coll_dag_t *dag, xmmsv_t *coll, int limit_start, int limit_len, xmmsv_t *fetch, xmmsv_t *group, xmms_error_t *err);
static xmmsv_t * xmms_collection_client_query (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *fetch, xmms_error_t *err);
static xmmsv_t *xmms_collection_client_idlist_from_playlist (xmms_coll_dag_t *dag, const gchar *mediainfo, xmms_error_t *err);


#include "collection_ipc.c"

xmmsv_t *
xmms_collection_changed_msg_new (xmms_collection_changed_actions_t type,
                                 const gchar *plname, const gchar *namespace)
{
	return xmmsv_build_dict (XMMSV_DICT_ENTRY_INT ("type", type),
	                         XMMSV_DICT_ENTRY_STR ("name", plname),
	                         XMMSV_DICT_ENTRY_STR ("namespace", namespace),
	                         XMMSV_DICT_END);
}

void
xmms_collection_changed_msg_send (xmms_coll_dag_t *colldag, xmmsv_t *dict)
{
	g_return_if_fail (colldag);
	g_return_if_fail (dict);

	xmms_object_emit (XMMS_OBJECT (colldag),
	                  XMMS_IPC_SIGNAL_COLLECTION_CHANGED,
	                  dict);
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

	GMutex mutex;

	xmms_medialib_t *medialib;
};

/** Initializes a new xmms_coll_dag_t.
 *
 * @returns  The newly allocated collection DAG.
 */
xmms_coll_dag_t *
xmms_collection_init (xmms_medialib_t *medialib)
{
	xmms_coll_dag_t *ret;
	gint i;

	ret = xmms_object_new (xmms_coll_dag_t, xmms_collection_destroy);
	g_mutex_init (&ret->mutex);

	xmms_object_ref (medialib);
	ret->medialib = medialib;

	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		ret->collrefs[i] = g_hash_table_new_full (g_str_hash, g_str_equal,
		                                          g_free, coll_unref);
	}

	xmms_collection_register_ipc_commands (XMMS_OBJECT (ret));

	return ret;
}

static void
add_metadata_from_tree (const gchar *key, xmmsv_t *value, gpointer user_data)
{
	add_metadata_from_tree_user_data_t *ud = user_data;

	if (xmmsv_get_type (value) == XMMSV_TYPE_INT32) {
		gint iv;
		xmmsv_get_int (value, &iv);
		xmms_medialib_entry_property_set_int_source (ud->session,
		                                             ud->entry,
		                                             key,
		                                             iv,
		                                             ud->src);
	} else if (xmmsv_get_type (value) == XMMSV_TYPE_STRING) {
		const gchar *sv;
		xmmsv_get_string (value, &sv);
		xmms_medialib_entry_property_set_str_source (ud->session,
		                                             ud->entry,
		                                             key,
		                                             sv,
		                                             ud->src);
	}
}


/** Create a idlist from a playlist file
 * @param dag  The collection DAG.
 * @param path  URL to the playlist file
 * @param err  If error occurs, a message is stored in this variable.
 * @returns  A idlist
 */
static xmmsv_t *
xmms_collection_client_idlist_from_playlist (xmms_coll_dag_t *dag,
                                             const gchar *path,
                                             xmms_error_t *err)
{
	xmms_stream_type_t *stream_type;
	xmms_xform_t *xform;
	GList *stream_types;
	xmmsv_t *dict, *list, *coll;
	xmmsv_list_iter_t *it;
	const gchar* src;

	stream_type = _xmms_stream_type_new (XMMS_STREAM_TYPE_BEGIN,
	                                     XMMS_STREAM_TYPE_MIMETYPE,
	                                     "application/x-xmms2-playlist-entries",
	                                     XMMS_STREAM_TYPE_END);
	stream_types = g_list_prepend (NULL, stream_type);

	/* we don't want any effects for playlist, so just report we're rehashing */
	xform = xmms_xform_chain_setup_url (dag->medialib, 0, path, stream_types, TRUE);

	if (!xform) {
		xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "We can't handle this type of playlist or URL");
		return NULL;
	}

	list = xmms_xform_browse_method (xform, "/", err);
	if (xmms_error_iserror (err)) {
		xmms_object_unref (xform);
		return NULL;
	}

	coll = xmmsv_new_coll (XMMS_COLLECTION_TYPE_IDLIST);
	src = "plugin/playlist";

	xmmsv_get_list_iter (list, &it);

	while (xmmsv_list_iter_entry (it, &dict)) {
		xmms_medialib_session_t *session;
		xmms_medialib_entry_t entry;
		xmmsv_t *value;
		const gchar *realpath;

		xmmsv_list_iter_next (it);

		if (!xmmsv_dict_get (dict, "realpath", &value)) {
			xmms_log_error ("Playlist plugin did not set realpath; probably a bug in plugin");
			continue;
		}

		xmmsv_get_string (value, &realpath);
		xmmsv_dict_remove (dict, "realpath");
		xmmsv_dict_remove (dict, "path");

		do {
			session = xmms_medialib_session_begin (dag->medialib);
			entry = xmms_medialib_entry_new_encoded (session, realpath, err);

			if (entry) {
				add_metadata_from_tree_user_data_t udata;
				udata.entry = entry;
				udata.src = src;
				udata.session = session;


				xmmsv_dict_foreach (dict, add_metadata_from_tree, &udata);

				xmmsv_coll_idlist_append (coll, entry);
			} else {
				xmms_log_error ("couldn't add %s to collection!", realpath);
			}
		} while (!xmms_medialib_session_commit (session));
	}

	xmmsv_unref (list);

	xmms_object_unref (xform);
	xmms_object_unref (stream_type);
	g_list_free (stream_types);

	return coll;
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
void
xmms_collection_client_remove (xmms_coll_dag_t *dag, const gchar *name,
                               const gchar *namespace, xmms_error_t *err)
{
	xmms_collection_namespace_id_t nsid;
	gboolean retval = FALSE;
	guint i;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return;
	}

	g_mutex_lock (&dag->mutex);

	/* Unreference the matching collection(s) */
	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
			retval = xmms_collection_unreference (dag, name, i) || retval;
		}
	} else {
		retval = xmms_collection_unreference (dag, name, nsid);
	}

	g_mutex_unlock (&dag->mutex);

	if (retval == FALSE) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Failed to remove this collection!");
	}

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
void
xmms_collection_client_save (xmms_coll_dag_t *dag, const gchar *name, const gchar *namespace,
                             xmmsv_t *coll, xmms_error_t *err)
{
	const gchar *valerr = "Invalid collection: unknown reason. This is "
	                      "probably a bug in xmms2d.";
	xmms_collection_namespace_id_t nsid;
	xmmsv_t *existing;
	gchar *alias;
	GList *list, *item;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return;
	} else if (nsid == XMMS_COLLECTION_NSID_ALL) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "cannot save collection in all namespaces");
		return;
	}

	/* Validate collection structure */
	if (!xmms_collection_validate (dag, coll, name, namespace, &valerr)) {
		xmms_error_set (err, XMMS_ERROR_INVAL, valerr);
		return;
	}

	g_mutex_lock (&dag->mutex);

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
		list = NULL;
		while ((alias = xmms_collection_find_alias (dag, nsid, existing, NULL)) != NULL) {
			/* update all pairs pointing to the old coll */
			xmms_collection_update_pointer (dag, alias, nsid, coll);

			list = g_list_prepend (list, alias);
		}

		for (item = g_list_first (list); item; item = g_list_next (item)) {
			alias = item->data;

			XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_UPDATE,
			                             alias, namespace);
			g_free (alias);
		}
		g_list_free (list);

	/* Save new collection in the table */
	} else {
		xmms_collection_update_pointer (dag, name, nsid, coll);

		XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_ADD,
		                             name, namespace);
	}

	g_mutex_unlock (&dag->mutex);
}


/** Retrieve the structure of a given collection.
 *
 * If looking in ALL namespaces, only the collection first found is returned!
 *
 * @param dag  The collection DAG.
 * @param name  The name of the collection to retrieve.
 * @param namespace  The namespace in which to look for the collection.
 * @param err  If an error occurs, a message is stored in it.
 * @returns  A copy of the collection structure if found, NULL otherwise.
 */
xmmsv_t *
xmms_collection_client_get (xmms_coll_dag_t *dag, const gchar *name,
                            const gchar *namespace, xmms_error_t *err)
{
	xmms_collection_namespace_id_t nsid;
	xmmsv_t *coll, *result = NULL;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return NULL;
	}

	g_mutex_lock (&dag->mutex);

	coll = xmms_collection_get_pointer (dag, name, nsid);

	/* Not found! */
	if (coll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such collection");
	} else {
		result = xmmsv_copy (coll);
	}

	g_mutex_unlock (&dag->mutex);

	return result;
}

/** Lists the collections in the given namespace.
 *
 * @param dag  The collection DAG.
 * @param namespace  The namespace to list collections from (can be ALL).
 * @param err  If an error occurs, a message is stored in it.
 * @returns A newly allocated xmmsv_t with the list of collection names.
 * Remember that it is only the LIST that is copied. Not the entries.
 * The entries are however referenced, and must be unreffed!
 */
xmmsv_t *
xmms_collection_client_list (xmms_coll_dag_t *dag, const gchar *namespace,
                             xmms_error_t *err)
{
	xmms_collection_namespace_id_t nsid;
	xmmsv_t *result;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return NULL;
	}

	result = xmmsv_new_list ();

	g_mutex_lock (&dag->mutex);

	/* Get the list of collections in the given namespace */
	xmms_collection_foreach_in_namespace (dag, nsid, prepend_key_string, result);

	g_mutex_unlock (&dag->mutex);

	return result;
}


/** Find all collections in the given namespace that contain a given media.
 *
 * @param dag  The collection DAG.
 * @param mid  The id of the media.
 * @param namespace  The namespace in which to look for collections.
 * @param err  If an error occurs, a message is stored in it.
 * @returns A newly allocated xmmsv_t list with the names of the matching collections.
 */
xmmsv_t *
xmms_collection_client_find (xmms_coll_dag_t *dag, gint32 mid, const gchar *namespace,
                             xmms_error_t *err)
{
	xmms_collection_namespace_id_t nsid;
	xmmsv_t *result;
	gchar *open_name;
	GHashTable *match_table;
	xmmsv_t *coll, *filter_coll;
	xmmsv_t *idlist;

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

	filter_coll = xmmsv_new_coll (XMMS_COLLECTION_TYPE_EQUALS);
	xmmsv_coll_attribute_set_string (filter_coll, "type", "id");
	xmms_collection_set_int_attr (filter_coll, "value", mid);

	/* While not all collections have been checked, check next */
	while (g_hash_table_find (match_table, find_unchecked, &open_name) != NULL) {
		coll_find_state_t *match = g_new (coll_find_state_t, 1);
		coll = xmms_collection_get_pointer (dag, open_name, nsid);

		xmmsv_coll_add_operand (filter_coll, coll);
		idlist = xmms_collection_query_ids (dag, coll, err);

		if (xmmsv_list_get_size (idlist) > 0) {
			*match = XMMS_COLLECTION_FIND_STATE_MATCH;
		} else {
			*match = XMMS_COLLECTION_FIND_STATE_NOMATCH;
		}

		xmmsv_unref (idlist);
		xmmsv_coll_remove_operand (filter_coll, coll);
		g_hash_table_replace (match_table, g_strdup (open_name), match);
	}

	xmmsv_unref (filter_coll);

	result = xmmsv_new_list ();

	/* List matching collections */
	g_hash_table_foreach (match_table, build_list_matches, result);
	g_hash_table_destroy (match_table);

	return result;
}


/** Rename a collection in a given namespace.
 *
 * @param dag  The collection DAG.
 * @param from_name  The name of the collection to rename.
 * @param to_name  The new name of the collection.
 * @param namespace  The namespace to consider (cannot be ALL).
 * @param err  If an error occurs, a message is stored in it.
 * @return True if a collection was found and renamed.
 */
void
xmms_collection_client_rename (xmms_coll_dag_t *dag, const gchar *from_name,
                               const gchar *to_name, const gchar *namespace,
                               xmms_error_t *err)
{
	xmms_collection_namespace_id_t nsid;
	xmmsv_t *from_coll, *to_coll;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return;
	} else if (nsid == XMMS_COLLECTION_NSID_ALL) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "cannot rename collection in all namespaces");
		return;
	}

	g_mutex_lock (&dag->mutex);

	from_coll = xmms_collection_get_pointer (dag, from_name, nsid);
	to_coll   = xmms_collection_get_pointer (dag, to_name, nsid);

	/* Input validation */
	if (from_coll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such collection");
	} else if (to_coll != NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "a collection already exists with the target name");
	} else {
		/* Update collection name everywhere */
		xmmsv_t *dict;

		/* insert new pair in hashtable */
		xmms_collection_update_pointer (dag, to_name, nsid, from_coll);

		/* remove old pair from hashtable */
		g_hash_table_remove (dag->collrefs[nsid], from_name);

		/* update name in all reference operators */
		coll_rename_infos_t infos = { from_name, to_name, namespace };
		xmms_collection_apply_to_all_collections (dag, rename_references, &infos);

		/* Send _RENAME signal */
		dict = xmms_collection_changed_msg_new (XMMS_COLLECTION_CHANGED_RENAME,
		                                        from_name, namespace);
		xmmsv_dict_set_string (dict, "newname", to_name);
		xmms_collection_changed_msg_send (dag, dict);
	}

	g_mutex_unlock (&dag->mutex);

}

/** Find the ids of the media matched by a collection.
 *
 * @param dag  The collection DAG.
 * @param coll  The collection used to match media.
 * @param lim_start  The beginning index of the LIMIT statement (0 to disable).
 * @param lim_len  The number of entries of the LIMIT statement (0 to disable).
 * @param order  The list of properties to order by (empty to disable).
 * @param err  If an error occurs, a message is stored in it.
 * @return A list of media ids.
 */
xmmsv_t *
xmms_collection_query_ids (xmms_coll_dag_t *dag, xmmsv_t *coll,
                           xmms_error_t *err)
{
	xmmsv_t *ret, *spec, *metadata, *get;

	get = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("id"),
	                        XMMSV_LIST_END);

	metadata = xmmsv_build_dict (XMMSV_DICT_ENTRY_STR ("type", "metadata"),
	                             XMMSV_DICT_ENTRY_STR ("aggregate", "first"),
	                             XMMSV_DICT_ENTRY ("get", get),
	                             XMMSV_DICT_END);

	spec = xmmsv_build_dict (XMMSV_DICT_ENTRY_STR ("type", "cluster-list"),
	                         XMMSV_DICT_ENTRY_STR ("cluster-by", "position"),
	                         XMMSV_DICT_ENTRY ("data", metadata),
	                         XMMSV_DICT_END);

	ret = xmms_collection_client_query (dag, coll, spec, err);
	xmmsv_unref (spec);

	return ret;
}

static xmmsv_t *
xmms_collection_query_infos_spec (xmmsv_t *fields, xmmsv_t *grouping)
{
	xmmsv_t *entry, *spec, *org_dict, *org_data;
	xmmsv_list_iter_t *it;
	const gchar *string;
	gint i;

	/* Contsruct an organize spec for the metadata fields we're interested in */
	org_data = xmmsv_new_dict ();
	for (i = 0; xmmsv_list_get_string (fields, i, &string); i++) {
		xmmsv_t *metadata;

		if (strcmp (string, "id") == 0) {
			metadata = xmmsv_build_metadata (NULL,
			                                 xmmsv_new_string ("id"),
			                                 "first", NULL);
		} else {
			metadata = xmmsv_build_metadata (xmmsv_new_string (string),
			                                 xmmsv_new_string ("value"),
			                                 "first", NULL);
		}

		xmmsv_dict_set (org_data, string, metadata);
		xmmsv_unref (metadata);
	}
	org_dict = xmmsv_build_organize (org_data);

	spec = org_dict;

	xmmsv_get_list_iter (grouping, &it);
	xmmsv_list_iter_last (it);

	/* Create grouping by recursing cluster-list specs */
	while (xmmsv_list_iter_entry (it, &entry)) {
		xmmsv_t *cluster_by, *cluster_field = NULL;
		const char *value;

		xmmsv_get_string (entry, &value);

		if (strcmp (value, "position") == 0) {
			cluster_by = xmmsv_ref (entry);
		} else if (strcmp (value, "id") == 0) {
			cluster_by = xmmsv_ref (entry);
		} else {
			cluster_by = xmmsv_new_string ("value");
			cluster_field = xmmsv_ref (entry);
		}

		spec = xmmsv_build_cluster_list (cluster_by, cluster_field, spec);
		xmmsv_dict_set_string (spec, "cluster-fallback", "");

		xmmsv_list_iter_prev (it);
	}

	return spec;
}


xmmsv_t *
xmms_collection_client_query_infos (xmms_coll_dag_t *dag, xmmsv_t *coll,
                                    int limit_start, int limit_len,
                                    xmmsv_t *fetch,
                                    xmmsv_t *group, xmms_error_t *err)
{
	xmmsv_t *spec, *unflattened, *ret, *limited;

	if (xmmsv_list_get_size (fetch) == 0) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "Empty fetch list");
	} else if (!xmmsv_list_has_type (fetch, XMMSV_TYPE_STRING)) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "Invalid fetch list");
	} else if (group != NULL && !xmmsv_list_has_type (group, XMMSV_TYPE_STRING)) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "Invalid group list");
	}

	limited = xmmsv_coll_add_limit_operator (coll, limit_start, limit_len);

	/* If collection is not the same, we've applied some limits to it, and need
	 * to take grouping into account when limiting.
	 */
	if (coll != limited) {
		const char *str;
		GString *sb;
		int i;

		/* As collection attributes can't hold anything other than strings yet,
		 * convert the list of grouping attributes to a comma separated string
		 */
		sb = g_string_new ("");
		for (i = 0; xmmsv_list_get_string (group, i, &str); i++) {
			if (i != 0)
				g_string_append (sb, ",");
			g_string_append (sb, str);
		}

		if (sb->len == 0) {
			xmmsv_coll_attribute_set_string (limited, "type", "position");
		} else {
			xmmsv_coll_attribute_set_string (limited, "type", "value");
			xmmsv_coll_attribute_set_string (limited, "fields", sb->str);
		}

		g_string_free (sb, TRUE);
	}

	if (group == NULL || xmmsv_list_get_size (group) <= 0) {
		group = xmmsv_new_list ();
		xmmsv_list_append (group, xmmsv_new_string ("position"));
	} else {
		group = xmmsv_ref (group);
	}

	spec = xmms_collection_query_infos_spec (fetch, group);

	unflattened = xmms_collection_client_query (dag, limited, spec, err);

	xmmsv_unref (limited);
	xmmsv_unref (spec);

	if (xmmsv_list_get_size (group) > 0) {
		ret = xmmsv_list_flatten (unflattened, xmmsv_list_get_size (group) - 1);
		xmmsv_unref (unflattened);
	} else {
		ret = unflattened;
	}

	xmmsv_unref (group);

	return ret;
}

xmmsv_t *
xmms_collection_client_query (xmms_coll_dag_t *dag, xmmsv_t *coll,
                              xmmsv_t *fetch, xmms_error_t *err)
{
	const gchar *valerr = "Invalid collection: unknown reason. This is "
	                      "probably a bug in xmms2d.";
	xmms_medialib_session_t *session;
	xmmsv_t *ret;

	/* validate the collection to query */
	if (!xmms_collection_validate (dag, coll, NULL, NULL, &valerr)) {
		if (err) {
			xmms_error_set (err, XMMS_ERROR_INVAL, valerr);
		}
		return NULL;
	}

	g_mutex_lock (&dag->mutex);

	xmms_collection_apply_to_collection (dag, coll, bind_all_references, NULL);

	do {
		session = xmms_medialib_session_begin_ro (dag->medialib);
		ret = xmms_medialib_query (session, coll, fetch, err);
	} while (!xmms_medialib_session_commit (session));

	g_mutex_unlock (&dag->mutex);

	return ret;
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
xmms_collection_update_pointer (xmms_coll_dag_t *dag, const gchar *name,
                                xmms_collection_namespace_id_t nsid, xmmsv_t *newtarget)
{
	g_hash_table_replace (dag->collrefs[nsid], g_strdup (name), newtarget);
	xmmsv_ref (newtarget);
}

/** Find the collection structure corresponding to the given name in the given namespace.
 *
 * @param dag  The collection DAG.
 * @param collname  The name of the collection to find.
 * @param nsid  The namespace id.
 * @returns  The collection structure if found, NULL otherwise.
 */
xmmsv_t *
xmms_collection_get_pointer (xmms_coll_dag_t *dag, const gchar *collname,
                             xmms_collection_namespace_id_t nsid)
{
	gint i;
	xmmsv_t *coll = NULL;

	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES && coll == NULL; ++i) {
			coll = g_hash_table_lookup (dag->collrefs[i], collname);
		}
	} else {
		coll = g_hash_table_lookup (dag->collrefs[nsid], collname);
	}

	return coll;
}

/** Extract an attribute from a collection as an integer.
 *
 * @param coll  The collection to extract the attribute from.
 * @param attrname  The name of the attribute.
 * @param val  The integer value of the attribute will be saved in this pointer.
 * @return TRUE if attribute correctly read, FALSE otherwise
 */
gboolean
xmms_collection_get_int_attr (xmmsv_t *coll, const gchar *attrname, gint *val)
{
	gboolean retval = FALSE;
	gint buf;
	const gchar *str;
	gchar *endptr;

	if (xmmsv_coll_attribute_get_int (coll, attrname, val)) {
		retval = TRUE;
	} else if (xmmsv_coll_attribute_get_string (coll, attrname, &str)) {
		buf = strtol (str, &endptr, 10);

		/* Valid integer string */
		if (*endptr == '\0') {
			*val = buf;
			retval = TRUE;
		}
	}

	return retval;
}

/** Set the attribute of a collection as an integer.
 *
 * @param coll  The collection in which to set the attribute.
 * @param attrname  The name of the attribute.
 * @param newval  The new value of the attribute.
 * @return TRUE if attribute successfully saved, FALSE otherwise.
 */
gboolean
xmms_collection_set_int_attr (xmmsv_t *coll, const gchar *attrname,
                              gint newval)
{
	gboolean retval = FALSE;
	gchar str[XMMS_MAX_INT_ATTRIBUTE_LEN + 1];
	gint written;

	written = g_snprintf (str, sizeof (str), "%d", newval);
	if (written < XMMS_MAX_INT_ATTRIBUTE_LEN) {
		xmmsv_coll_attribute_set_string (coll, attrname, str);
		retval = TRUE;
	}

	return retval;
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
 * @return A copy of the key of the found pair, needs to be freed.
 */
gchar *
xmms_collection_find_alias (xmms_coll_dag_t *dag, xmms_collection_namespace_id_t nsid,
                            xmmsv_t *value, const gchar *key)
{
	gchar *otherkey = NULL;
	coll_table_pair_t search_pair = { key, value };

	if (g_hash_table_find (dag->collrefs[nsid], value_match_save_key,
	                       &search_pair) != NULL) {
		otherkey = g_strdup (search_pair.key);
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
xmms_collection_get_random_media (xmms_coll_dag_t *dag, xmmsv_t *source)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t ret;

	g_mutex_lock (&dag->mutex);
	xmms_collection_apply_to_collection (dag, source, bind_all_references, NULL);

	do {
		session = xmms_medialib_session_begin_ro (dag->medialib);
		ret = xmms_medialib_query_random_id (session, source);
	} while (!xmms_medialib_session_commit (session));

	g_mutex_unlock (&dag->mutex);

	return ret;
}

/** @} */



/** Free the collection DAG and other memory in the xmms_coll_dag_t
 *
 *  This will free all collections in the DAG!
 */
static void
xmms_collection_destroy (xmms_object_t *object)
{
	xmms_coll_dag_t *dag = (xmms_coll_dag_t *)object;
	gint i;

	XMMS_DBG ("Deactivating collection object.");

	g_return_if_fail (dag);

	xmms_object_unref (dag->medialib);
	g_mutex_clear (&dag->mutex);

	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		g_hash_table_destroy (dag->collrefs[i]);  /* dag is freed here */
	}

	xmms_collection_unregister_ipc_commands ();
}

/** Validate the given collection against a DAG.
 *
 * @param dag  The collection DAG.
 * @param coll  The collection to validate.
 * @param save_name  The name under which the collection will be saved (NULL
 *                   if none).
 * @param save_namespace  The namespace in which the collection will be
 *                        saved (NULL if none).
 * @returns  True if the collection is valid, false otherwise.
 */
static gboolean
xmms_collection_validate (xmms_coll_dag_t *dag, xmmsv_t *coll,
                          const gchar *save_name, const gchar *save_namespace,
                          const gchar **err)
{
	const gchar *backuperr;

	/* Save us some NULL checking */
	if (!err) {
		err = &backuperr;
	}

	/* Special validation checks for the Playlists namespace */
	if (save_namespace != NULL &&
	    strcmp (save_namespace, XMMS_COLLECTION_NS_PLAYLISTS) == 0) {
		/* only accept idlists */
		if (xmmsv_coll_get_type (coll) != XMMS_COLLECTION_TYPE_IDLIST) {
			*err = "non-IDLIST in namespace Playlists.";
			return FALSE;
		}
	}

	/* Standard checking of this collection */
	return xmms_collection_validate_recurs (dag, coll, save_name,
	                                        save_namespace, err);
}

/**
 * Internal recursive validation function used to validate the whole
 * graph of a collection.
 */
static gboolean
xmms_collection_validate_recurs (xmms_coll_dag_t *dag, xmmsv_t *coll,
                                 const gchar *save_name,
                                 const gchar *save_namespace,
                                 const gchar **err)
{
	guint num_operands = 0;
	xmmsv_t *op, *ref;
	const gchar *attr, *attr2;
	gboolean valid = TRUE;
	xmmsv_coll_type_t type;
	xmms_collection_namespace_id_t nsid;

	/* count operands */
	num_operands = xmmsv_list_get_size (xmmsv_coll_operands_get (coll));

	/* analyse by type */
	type = xmmsv_coll_get_type (coll);
	switch (type) {
	case XMMS_COLLECTION_TYPE_REFERENCE:
		/* zero or one (bound in DAG) operand */
		if (num_operands > 1) {
			*err = "Invalid collection: REFERENCE with more than one operand.";
			return FALSE;
		}

		/* check if referenced collection exists */
		xmmsv_coll_attribute_get_string (coll, "reference", &attr);
		if (attr == NULL) {
			*err = "Invalid collection: REFERENCE without \"reference\"-"
			      "attribute.";
			return FALSE;
		} else if (strcmp (attr, "All Media") != 0) {
			xmmsv_coll_attribute_get_string (coll, "namespace", &attr2);

			if (attr2 == NULL) {
				*err = "Invalid collection: REFERENCE without \"namespace\"-"
				      "attribute.";
				return FALSE;
			}

			nsid = xmms_collection_get_namespace_id (attr2);
			if (nsid == XMMS_COLLECTION_NSID_INVALID) {
				*err = "Invalid collection: REFERENCE with invalid "
				      "\"namespace\"-attribute.";
				return FALSE;
			}

			g_mutex_lock (&dag->mutex);
			ref = xmms_collection_get_pointer (dag, attr, nsid);
			if (ref == NULL) {
				g_mutex_unlock (&dag->mutex);
				*err = "Invalid collection: REFERENCE refers to inexistent "
				       "collection.";
				return FALSE;
			}

			if (save_name && save_namespace) {
				/* self-reference is of course forbidden */
				if (strcmp (attr, save_name) == 0 &&
				    strcmp (attr2, save_namespace) == 0) {

					g_mutex_unlock (&dag->mutex);
					*err = "Invalid collection: cycle detected: REFERENCE "
					       "refers to intended name and namespace.";
					return FALSE;

				/* check if the referenced coll references this one (loop!) */
				} else if (xmms_collection_has_reference_to (dag, ref, save_name,
				                                             save_namespace)) {
					g_mutex_unlock (&dag->mutex);
					*err = "Invalid collection: cycle detected: REFERENCE "
					       "refers to collection that (possibly indirectly) "
					       "refers to intended name and namespace.";
					return FALSE;
				}
			}

			g_mutex_unlock (&dag->mutex);
		} else {
			/* "All Media" reference, so no referenced coll pointer */
			ref = NULL;
		}

		/* ensure that the operand is consistent with the reference infos */
		if (num_operands == 1) {
			xmmsv_list_get (xmmsv_coll_operands_get (coll), 0, &op);
			if (op != ref) {
				*err = "Invalid collection: REFERENCE refers to other "
				       "collection than its operand. This is probably a bug in "
				       "xmms2d.";
				xmms_log_error ("%s", *err);
				return FALSE;
			}
		}
		break;

	case XMMS_COLLECTION_TYPE_UNION:
	case XMMS_COLLECTION_TYPE_INTERSECTION:
		/* need operand(s) */
		if (num_operands == 0) {
			*err = "Invalid collection: UNION or INTERSECTION with zero "
			       "operands.";
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_COMPLEMENT:
		/* one operand */
		if (num_operands != 1) {
			*err = "Invalid collection: COMPLEMENT with fewer or more than one "
			       "operand.";
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_HAS:
	case XMMS_COLLECTION_TYPE_MATCH:
	case XMMS_COLLECTION_TYPE_TOKEN:
	case XMMS_COLLECTION_TYPE_EQUALS:
	case XMMS_COLLECTION_TYPE_NOTEQUAL:
	case XMMS_COLLECTION_TYPE_SMALLER:
	case XMMS_COLLECTION_TYPE_SMALLEREQ:
	case XMMS_COLLECTION_TYPE_GREATER:
	case XMMS_COLLECTION_TYPE_GREATEREQ:
		/* one operand */
		if (num_operands != 1) {
			*err = "Invalid collection: FILTER with fewer or more than one "
			       "operand.";
			return FALSE;
		}

		if (type != XMMS_COLLECTION_TYPE_HAS &&
		    !xmmsv_coll_attribute_get_string (coll, "value", &attr)) {

			*err = "Invalid collection: non-HAS FILTER without \"value\"-"
			       "attribute.";
			return FALSE;
		}

		/* check that type equals "id", "value"
		 * or not set (defaults to "value") */
		if (xmmsv_coll_attribute_get_string (coll, "type", &attr)) {
			if (strcmp (attr, "id") && strcmp (attr, "value")) {
				*err = "Invalid collection: FILTER with invalid \"type\"-"
				       "attribute.";
				return FALSE;
			}
		}
		break;

	case XMMS_COLLECTION_TYPE_IDLIST:
		if (!xmmsv_coll_attribute_get_string (coll, "type", &attr)) {
			attr = (char*)"list";
		}

		if (strcmp (attr, "list") == 0 || strcmp (attr, "queue") == 0) {
			/* no operand */
			if (num_operands > 0) {
				*err = "Invalid collection: list- or queue-IDLIST with "
				       "operand(s)";
				return FALSE;
			}
		} else if (strcmp (attr, "pshuffle") == 0) {
			if (num_operands != 1) {
				*err = "Invalid collection: pshuffle-IDLIST with fewer or more "
				       "than one operand.";
				return FALSE;
			}
		} else {
			*err = "Invalid collection: IDLIST with invalid \"type\"-"
			       "attribute.";
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_UNIVERSE:
		/* No operands */
		if (num_operands > 0) {
			*err = "Invalid collection: UNIVERSE with operand(s).";
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_ORDER:
		/* One operand */
		if (num_operands != 1) {
			*err = "Invalid collection: ORDER with fewer or more than one "
			       "operand.";
			return FALSE;
		}

		if (xmmsv_coll_attribute_get_string (coll, "direction", &attr)
		    && strcmp (attr, "ASC") != 0
		    && strcmp (attr, "DESC") != 0) {
			*err = "Invalid collection: ORDER with invalid \"order\"-"
			       "attribute.";
			return FALSE;
		}

		if (!xmmsv_coll_attribute_get_string (coll, "type", &attr)) {
			attr = "value";
		}

		if (strcmp (attr, "value") == 0) {
			xmmsv_t *field;

			/* If it's a sorting on values we need a field to sort on */
			if (!xmmsv_coll_attribute_get_value (coll, "field", &field)) {
				*err = "Invalid collection: ORDER without required \"field\"-"
				       "attribute";
				return FALSE;
			}

			if (!xmmsv_is_type (field, XMMSV_TYPE_STRING) ||
			    (xmmsv_is_type (field, XMMSV_TYPE_LIST) &&
			     !xmmsv_list_has_type (field, XMMSV_TYPE_STRING))) {
				*err = "Invalid collection: ORDER with \"field\" set to neither "
				       "a string or a list of strings.";

			}
		} else if (strcmp (attr, "id") != 0 && strcmp (attr, "random") != 0) {
			*err = "Invalid collection: ORDER must have \"type\" set to either "
			       "\"id\", \"value\", or \"random\"";
			return FALSE;
		}
		break;


	case XMMS_COLLECTION_TYPE_LIMIT:
		/* One operand */
		if (num_operands != 1) {
			*err = "Invalid collection: LIMIT with fewer or more than one "
			       "operand.";
			return FALSE;
		}
		if (xmmsv_coll_attribute_get_string (coll, "type", &attr) && strcmp ("value", attr) == 0) {
			/* If it's a limit on values we need a fields to limit on */
			if (!xmmsv_coll_attribute_get_string (coll, "fields", &attr)) {
				*err = "Invalid collection: LIMIT without required \"fields\"-"
				       "attribute";
				return FALSE;
			}
		}
		break;

	case XMMS_COLLECTION_TYPE_MEDIASET:
		/* One operand */
		if (num_operands != 1) {
			*err = "Invalid collection: MEDIASET with fewer or more than one "
			       "operand.";
			return FALSE;
		}
		break;

	/* invalid type */
	default:
		*err = "Invalid collection: operator with invalid type.";
		return FALSE;
		break;
	}


	/* recurse in operands */
	if (num_operands > 0 && type != XMMS_COLLECTION_TYPE_REFERENCE) {
		xmmsv_list_iter_t *iter;
		xmmsv_get_list_iter (xmmsv_coll_operands_get (coll), &iter);

		for (xmmsv_list_iter_first (iter);
		     valid && xmmsv_list_iter_valid (iter);
		     xmmsv_list_iter_next (iter)) {

			xmmsv_list_iter_entry (iter, &op);

			if (!xmms_collection_validate_recurs (dag, op, save_name,
			                                      save_namespace, err)) {
				valid = FALSE;
			}
		}

		xmmsv_list_iter_explicit_destroy (iter);
	}

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
xmms_collection_unreference (xmms_coll_dag_t *dag, const gchar *name,
                             xmms_collection_namespace_id_t nsid)
{
	xmmsv_t *existing, *active_pl;
	gboolean retval = FALSE;

	existing  = g_hash_table_lookup (dag->collrefs[nsid], name);
	active_pl = g_hash_table_lookup (dag->collrefs[XMMS_COLLECTION_NSID_PLAYLISTS],
	                                 XMMS_ACTIVE_PLAYLIST);

	/* Unref if collection exists, and is not pointed at by _active playlist */
	if (existing != NULL && existing != active_pl) {
		const gchar *nsname = xmms_collection_get_namespace_string (nsid);
		coll_rebind_infos_t infos = { name, nsname, existing, NULL };
		gchar *matchkey;

		/* FIXME: if reference pointed to by a label, we should update
		 * the label to point to the ref'd operator instead ! */

		/* Strip all references to the deleted coll, bind operator directly */
		xmms_collection_apply_to_all_collections (dag, strip_references, &infos);

		/* Remove all pairs pointing to that collection */
		while ((matchkey = xmms_collection_find_alias (dag, nsid,
		                                               existing, NULL)) != NULL) {

			XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_REMOVE,
			                             matchkey,
			                             nsname);

			g_hash_table_remove (dag->collrefs[nsid], matchkey);
			g_free (matchkey);
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
xmms_collection_namespace_id_t
xmms_collection_get_namespace_id (const gchar *namespace)
{
	xmms_collection_namespace_id_t nsid;

	if (strcmp (namespace, XMMS_COLLECTION_NS_ALL) == 0) {
		nsid = XMMS_COLLECTION_NSID_ALL;
	} else if (strcmp (namespace, XMMS_COLLECTION_NS_COLLECTIONS) == 0) {
		nsid = XMMS_COLLECTION_NSID_COLLECTIONS;
	} else if (strcmp (namespace, XMMS_COLLECTION_NS_PLAYLISTS) == 0) {
		nsid = XMMS_COLLECTION_NSID_PLAYLISTS;
	} else {
		nsid = XMMS_COLLECTION_NSID_INVALID;
	}

	return nsid;
}

/** Find the namespace name (string) corresponding to a namespace id.
 *
 * @param nsid  The namespace id.
 * @returns  The namespace name (string).
 */
const gchar *
xmms_collection_get_namespace_string (xmms_collection_namespace_id_t nsid)
{
	const gchar *name;

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
xmms_collection_has_reference_to (xmms_coll_dag_t *dag, xmmsv_t *coll,
                                  const gchar *tg_name, const gchar *tg_ns)
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
void
xmms_collection_foreach_in_namespace (xmms_coll_dag_t *dag,
                                      xmms_collection_namespace_id_t nsid,
                                      GHFunc f, void *udata)
{
	gint i;

	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
			g_hash_table_foreach (dag->collrefs[i], f, udata);
		}
	} else if (nsid != XMMS_COLLECTION_NSID_INVALID) {
		g_hash_table_foreach (dag->collrefs[nsid], f, udata);
	}
}

/** Apply a function of type #FuncApplyToColl to all the collections in all namespaces.
 *
 * @param dag  The collection DAG.
 * @param f  The function to apply to all the collections.
 * @param udata  Additional user data parameter passed to the function.
 */
void
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
void
xmms_collection_apply_to_collection (xmms_coll_dag_t *dag,
                                     xmmsv_t *coll,
                                     FuncApplyToColl f, void *udata)
{
	xmms_collection_apply_to_collection_recurs (dag, coll, NULL, f, udata);
}

/* Internal function used for recursion (parent param, NULL by default) */
static void
xmms_collection_apply_to_collection_recurs (xmms_coll_dag_t *dag,
                                            xmmsv_t *coll,
                                            xmmsv_t *parent,
                                            FuncApplyToColl f, void *udata)
{
	xmmsv_t *op;

	/* Apply the function to the operator. */
	f (dag, coll, parent, udata);

	/* Recurse into the operands (if not a reference) */
	if (xmmsv_coll_get_type (coll) != XMMS_COLLECTION_TYPE_REFERENCE) {
		xmmsv_list_iter_t *iter;
		xmmsv_get_list_iter (xmmsv_coll_operands_get (coll), &iter);

		for (xmmsv_list_iter_first (iter);
		     xmmsv_list_iter_valid (iter);
		     xmmsv_list_iter_next (iter)) {

			xmmsv_list_iter_entry (iter, &op);

			xmms_collection_apply_to_collection_recurs (dag, op, coll, f, udata);
		}

		xmmsv_list_iter_explicit_destroy (iter);
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
	xmmsv_t *list = (xmmsv_t *) udata;
	xmmsv_list_insert_string (list, 0, key);
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
	xmmsv_t *coll = (xmmsv_t*)val;

	/* value matching and key not ignored, found! */
	if ((coll == pair->value) &&
	    (pair->key == NULL || strcmp (pair->key, key) != 0)) {
		pair->key = key;
		found = TRUE;
	}

	return found;
}

xmmsv_t *
xmms_collection_snapshot (xmms_coll_dag_t *dag)
{
	GHashTableIter iter;
	xmmsv_t *result, *collections, *playlists, *coll, *active_playlist;
	gchar *name;

	result = xmmsv_new_dict ();

	collections = xmmsv_new_dict ();
	xmmsv_dict_set (result, "collections", collections);
	xmmsv_unref (collections);

	playlists = xmmsv_new_dict ();
	xmmsv_dict_set (result, "playlists", playlists);
	xmmsv_unref (playlists);

	g_mutex_lock (&dag->mutex);

	xmms_collection_apply_to_all_collections (dag, unbind_all_references, NULL);

	g_hash_table_iter_init (&iter, dag->collrefs[XMMS_COLLECTION_NSID_COLLECTIONS]);
	while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer *) &coll)) {
		xmmsv_t *copy = xmmsv_copy (coll);
		xmmsv_dict_set (collections, name, copy);
		xmmsv_unref (copy);
	}

	active_playlist = xmms_collection_get_pointer (dag, XMMS_ACTIVE_PLAYLIST,
	                                               XMMS_COLLECTION_NSID_PLAYLISTS);

	g_hash_table_iter_init (&iter, dag->collrefs[XMMS_COLLECTION_NSID_PLAYLISTS]);
	while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer *) &coll)) {
		if (coll != active_playlist || strcmp (name, XMMS_ACTIVE_PLAYLIST) != 0) {
			xmmsv_t *copy = xmmsv_copy (coll);
			xmmsv_dict_set (playlists, name, copy);
			xmmsv_unref (copy);
		}
	}

	name = xmms_collection_find_alias (dag, XMMS_COLLECTION_NSID_PLAYLISTS,
	                                   active_playlist, XMMS_ACTIVE_PLAYLIST);
	xmmsv_dict_set_string (result, "active-playlist", name);
	g_free (name);

	xmms_collection_apply_to_all_collections (dag, bind_all_references, NULL);

	g_mutex_unlock (&dag->mutex);

	return result;
}

static void
xmms_collection_restore_collection (const gchar *name, xmmsv_t *coll, void *udata)
{
	xmms_coll_dag_t *dag = (xmms_coll_dag_t *) udata;
	xmms_collection_update_pointer (dag, name, XMMS_COLLECTION_NSID_COLLECTIONS, coll);
}

static void
xmms_collection_restore_playlist (const gchar *name, xmmsv_t *coll, void *udata)
{
	xmms_coll_dag_t *dag = (xmms_coll_dag_t *) udata;
	xmms_collection_update_pointer (dag, name, XMMS_COLLECTION_NSID_PLAYLISTS, coll);
}

static void
xmms_collection_restore_check_collection (const gchar *name, xmmsv_t *coll, void *udata)
{
	gboolean *error = (gboolean *) udata;
	if (!xmmsv_is_type (coll, XMMSV_TYPE_COLL))
		*error = TRUE;
}

static void
xmms_collection_restore_check_playlist (const gchar *name, xmmsv_t *coll, void *udata)
{
	gboolean *error = (gboolean *) udata;
	if (!xmmsv_is_type (coll, XMMSV_TYPE_COLL))
		*error = TRUE;
	else if (!xmmsv_coll_is_type (coll, XMMS_COLLECTION_TYPE_IDLIST))
		*error = TRUE;
}

void
xmms_collection_restore (xmms_coll_dag_t *dag, xmmsv_t *snapshot)
{
	xmmsv_t *collections, *playlists, *value, *alias;
	const gchar *name;

	alias = NULL;

	g_mutex_lock (&dag->mutex);

	if (snapshot != NULL && xmmsv_is_type (snapshot, XMMSV_TYPE_DICT)) {
		if (xmmsv_dict_get (snapshot, "playlists", &playlists) &&
		    xmmsv_is_type (playlists, XMMSV_TYPE_DICT)) {
			gboolean error = FALSE;
			xmmsv_dict_foreach (playlists, xmms_collection_restore_check_playlist, &error);
			if (error != TRUE)
				xmmsv_dict_foreach (playlists, xmms_collection_restore_playlist, dag);
		}

		if (xmmsv_dict_get (snapshot, "collections", &collections) &&
		    xmmsv_is_type (collections, XMMSV_TYPE_DICT)) {
			gboolean error = FALSE;
			xmmsv_dict_foreach (collections, xmms_collection_restore_check_collection, &error);
			if (error != TRUE)
				xmmsv_dict_foreach (collections, xmms_collection_restore_collection, dag);
		}

		if (xmmsv_dict_get (snapshot, "active-playlist", &value) &&
		    xmmsv_is_type (value, XMMSV_TYPE_STRING)) {
			xmmsv_get_string (value, &name);
			alias = xmms_collection_get_pointer (dag, name, XMMS_COLLECTION_NSID_PLAYLISTS);
		}
	}

	/* No active playlist found, check if there's a 'Default' playlist. */
	if (alias == NULL)
		alias = xmms_collection_get_pointer (dag, "Default", XMMS_COLLECTION_NSID_PLAYLISTS);

	/* Still out of luck, create an empty 'Default' playlist. */
	if (alias == NULL) {
		alias = xmmsv_new_coll (XMMS_COLLECTION_TYPE_IDLIST);
		xmms_collection_update_pointer (dag, "Default", XMMS_COLLECTION_NSID_PLAYLISTS, alias);
		xmmsv_unref (alias);
	}

	xmms_collection_update_pointer (dag, XMMS_ACTIVE_PLAYLIST, XMMS_COLLECTION_NSID_PLAYLISTS, alias);

	xmms_collection_apply_to_all_collections (dag, bind_all_references, NULL);

	/* TODO: validate everything and nuke stuff that fails, also take care of
	 *       creating a default playlist if active is missing
	 */

	g_mutex_unlock (&dag->mutex);
}

/**
 * If a reference, add the operator of the pointed collection as an
 * operand.
 */
static void
bind_all_references (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata)
{
	if (xmmsv_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE) {
		xmms_collection_namespace_id_t target_nsid;
		xmmsv_t *target;
		xmmsv_t *operands;
		const gchar *target_name;
		const gchar *target_namespace;

		xmmsv_coll_attribute_get_string (coll, "reference", &target_name);
		xmmsv_coll_attribute_get_string (coll, "namespace", &target_namespace);
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

		operands = xmmsv_coll_operands_get (coll);
		if (xmmsv_list_index_of (operands, target) < 0) {
			xmmsv_coll_add_operand (coll, target);
		}
	}
}

/**
 * If a reference, remove the operand.
 */
static void
unbind_all_references (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata)
{
	xmmsv_t *list;

	if (xmmsv_coll_is_type (coll, XMMS_COLLECTION_TYPE_REFERENCE)) {
		list = xmmsv_coll_operands_get (coll);
		xmmsv_list_clear (list);
	}
}

/**
 * If a reference, rebind the given operator to the new operator
 * representing the referenced collection (pointers and so are in the
 * udata structure).
 */
static void
rebind_references (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata)
{
	if (xmmsv_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE) {
		coll_rebind_infos_t *infos;

		const gchar *target_name = NULL;
		const gchar *target_namespace = NULL;

		infos = (coll_rebind_infos_t*)udata;

		/* FIXME: Or only compare operand vs oldtarget ? */

		xmmsv_coll_attribute_get_string (coll, "reference", &target_name);
		xmmsv_coll_attribute_get_string (coll, "namespace", &target_namespace);
		if (strcmp (infos->name, target_name) != 0 ||
		    strcmp (infos->namespace, target_namespace) != 0) {
			return;
		}

		xmmsv_coll_remove_operand (coll, infos->oldtarget);
		xmmsv_coll_add_operand (coll, infos->newtarget);
	}
}

/**
 * If a reference with matching name, rename it according to the
 * rename infos in the udata structure.
 */
static void
rename_references (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata)
{
	if (xmmsv_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE) {
		coll_rename_infos_t *infos;

		const gchar *target_name = NULL;
		const gchar *target_namespace = NULL;

		infos = (coll_rename_infos_t*)udata;

		xmmsv_coll_attribute_get_string (coll, "reference", &target_name);
		xmmsv_coll_attribute_get_string (coll, "namespace", &target_namespace);
		if (strcmp (infos->oldname, target_name) == 0 &&
		    strcmp (infos->namespace, target_namespace) == 0) {
			xmmsv_coll_attribute_set_string (coll, "reference", infos->newname);
		}
	}
}

/**
 * Strip reference operators to the given collection by rebinding the
 * parent directly to the pointed operator.
 */
static void
strip_references (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata)
{
	coll_rebind_infos_t *infos;
	const gchar *target_name = NULL;
	const gchar *target_namespace = NULL;
	xmmsv_list_iter_t *iter;
	xmmsv_t *op;

	infos = (coll_rebind_infos_t*)udata;

	xmmsv_get_list_iter (xmmsv_coll_operands_get (coll), &iter);
	for (xmmsv_list_iter_first (iter);
	     xmmsv_list_iter_valid (iter);
	     xmmsv_list_iter_next (iter)) {

		xmmsv_list_iter_entry (iter, &op);

		/* Skip if not potential reference */
		if (xmmsv_coll_get_type (op) != XMMS_COLLECTION_TYPE_REFERENCE) {
			continue;
		}

		xmmsv_coll_attribute_get_string (op, "reference", &target_name);
		xmmsv_coll_attribute_get_string (op, "namespace", &target_namespace);
		if (strcmp (infos->name, target_name) != 0 ||
		    strcmp (infos->namespace, target_namespace) != 0) {
			continue;
		}

		/* Rebind coll to ref'd operand directly, effectively strip reference */
		/* FIXME: Do we really need to do this _clear? */
		xmmsv_list_clear (xmmsv_coll_operands_get (op));

		xmmsv_list_iter_remove (iter);

		xmmsv_list_iter_insert (iter, infos->oldtarget);
	}
	xmmsv_list_iter_explicit_destroy (iter);
}

/**
 * Check if the current operator is a reference to a given collection,
 * and if so, update the structure passed as userdata.
 */
static void
check_for_reference (xmms_coll_dag_t *dag, xmmsv_t *coll, xmmsv_t *parent, void *udata)
{
	coll_refcheck_t *check = (coll_refcheck_t*)udata;
	if (xmmsv_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE && !check->found) {
		const gchar *target_name, *target_namespace;

		xmmsv_coll_attribute_get_string (coll, "reference", &target_name);
		xmmsv_coll_attribute_get_string (coll, "namespace", &target_namespace);
		if (strcmp (check->target_name, target_name) == 0 &&
		    strcmp (check->target_namespace, target_namespace) == 0) {
			check->found = TRUE;
		} else {
			xmmsv_t *op;

			if (xmmsv_list_get (xmmsv_coll_operands_get (coll), 0, &op)) {
				xmms_collection_apply_to_collection_recurs (dag, op, coll,
				                                            check_for_reference,
				                                            udata);
			}
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
	xmmsv_unref (coll);
}



/* ============  FIND / COLLECTION MATCH FUNCTIONS ============ */

/* Generate a build_match hashtable, states initialized to UNCHECKED. */
static void
build_match_table (gpointer key, gpointer value, gpointer udata)
{
	GHashTable *match_table = udata;
	coll_find_state_t *match = g_new (coll_find_state_t, 1);
	*match = XMMS_COLLECTION_FIND_STATE_UNCHECKED;
	g_hash_table_replace (match_table, g_strdup (key), match);
}

/* Return the first unchecked element from the match_table, set the
 * udata pointer to contain the key of that element.
 */
static gboolean
find_unchecked (gpointer name, gpointer value, gpointer udata)
{
	coll_find_state_t *match = value;
	gchar **open = udata;
	*open = name;
	return (*match == XMMS_COLLECTION_FIND_STATE_UNCHECKED);
}

/* Build a list of all matched entries of the match_table in the udata
 * pointer.
 */
static void
build_list_matches (gpointer key, gpointer value, gpointer udata)
{
	coll_find_state_t *state = value;
	xmmsv_t *list = (xmmsv_t *) udata;
	gchar *coll_name = key;
	if (*state == XMMS_COLLECTION_FIND_STATE_MATCH) {
		xmmsv_list_insert_string (list, 0, coll_name);
	}
}
