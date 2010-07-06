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
#include "xmmspriv/xmms_collquery.h"
#include "xmmspriv/xmms_collserial.h"
#include "xmmspriv/xmms_collsync.h"
#include "xmmspriv/xmms_xform.h"
#include "xmmspriv/xmms_streamtype.h"
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"


/* Internal helper structures */

typedef struct {
	const gchar *name;
	const gchar *namespace;
	xmmsv_coll_t *oldtarget;
	xmmsv_coll_t *newtarget;
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
	xmmsv_coll_t *value;
} coll_table_pair_t;

typedef enum {
	XMMS_COLLECTION_FIND_STATE_UNCHECKED,
	XMMS_COLLECTION_FIND_STATE_MATCH,
	XMMS_COLLECTION_FIND_STATE_NOMATCH,
} coll_find_state_t;

typedef struct add_metadata_from_tree_user_data_St {
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	guint src;
} add_metadata_from_tree_user_data_t;

static GList *global_stream_type;

/* Functions */

static void xmms_collection_destroy (xmms_object_t *object);

static gboolean xmms_collection_validate (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, const gchar *save_name, const gchar *save_namespace);
static gboolean xmms_collection_validate_recurs (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, const gchar *save_name, const gchar *save_namespace);
static gboolean xmms_collection_unreference (xmms_coll_dag_t *dag, const gchar *name, guint nsid);

static gboolean xmms_collection_has_reference_to (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, const gchar *tg_name, const gchar *tg_ns);

static void xmms_collection_apply_to_collection_recurs (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, xmmsv_coll_t *parent, FuncApplyToColl f, void *udata);

static void call_apply_to_coll (gpointer name, gpointer coll, gpointer udata);
static void prepend_key_string (gpointer key, gpointer value, gpointer udata);
static gboolean value_match_save_key (gpointer key, gpointer val, gpointer udata);

static void rebind_references (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, xmmsv_coll_t *parent, void *udata);
static void rename_references (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, xmmsv_coll_t *parent, void *udata);
static void strip_references (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, xmmsv_coll_t *parent, void *udata);
static void check_for_reference (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, xmmsv_coll_t *parent, void *udata);

static void coll_unref (void *coll);

static GHashTable *xmms_collection_media_info (xmms_medialib_entry_t mid, xmms_error_t *err);

static gboolean filter_get_mediainfo_field_string (xmmsv_coll_t *coll, GHashTable *mediainfo, gchar **val);
static gboolean filter_get_mediainfo_field_int (xmmsv_coll_t *coll, GHashTable *mediainfo, gint *val);
static gboolean filter_get_operator_value_string (xmmsv_coll_t *coll, const gchar **val);
static gboolean filter_get_operator_value_int (xmmsv_coll_t *coll, gint *val);
static gboolean filter_get_operator_case (xmmsv_coll_t *coll, gboolean *val);

static void build_match_table (gpointer key, gpointer value, gpointer udata);
static gboolean find_unchecked (gpointer name, gpointer value, gpointer udata);
static void build_list_matches (gpointer key, gpointer value, gpointer udata);

static gboolean xmms_collection_media_match (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsv_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_match_operand (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsv_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_match_reference (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsv_coll_t *coll, guint nsid, GHashTable *match_table, const gchar *refname, const gchar *refns);
static gboolean xmms_collection_media_filter_has (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsv_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_equals (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsv_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_match (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsv_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_smaller (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsv_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_greater (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsv_coll_t *coll, guint nsid, GHashTable *match_table);

static xmmsv_coll_t * xmms_collection_client_get (xmms_coll_dag_t *dag, const gchar *collname, const gchar *namespace, xmms_error_t *error);
static GList * xmms_collection_client_list (xmms_coll_dag_t *dag, const gchar *namespace, xmms_error_t *error);
static void xmms_collection_client_save (xmms_coll_dag_t *dag, const gchar *name, const gchar *namespace, xmmsv_coll_t *coll, xmms_error_t *error);
static void xmms_collection_client_remove (xmms_coll_dag_t *dag, const gchar *collname, const gchar *namespace, xmms_error_t *error);
static GList * xmms_collection_client_find (xmms_coll_dag_t *dag, gint32 mid, const gchar *namespace, xmms_error_t *error);
static void xmms_collection_client_rename (xmms_coll_dag_t *dag, const gchar *from_name, const gchar *to_name, const gchar *namespace, xmms_error_t *error);

static GList * xmms_collection_client_query_infos (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, gint32 lim_start, gint32 lim_len, xmmsv_t *order, xmmsv_t *fetch, xmmsv_t *group, xmms_error_t *err);
static GList * xmms_collection_client_query_ids (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, gint32 lim_start, gint32 lim_len, xmmsv_t *order, xmms_error_t *err);
static xmmsv_coll_t *xmms_collection_client_idlist_from_playlist (xmms_coll_dag_t *dag, const gchar *mediainfo, xmms_error_t *err);
static void xmms_collection_client_sync (xmms_coll_dag_t *dag, xmms_error_t *err);


#include "collection_ipc.c"

GTree *
xmms_collection_changed_msg_new (xmms_collection_changed_actions_t type,
                                 const gchar *plname, const gchar *namespace)
{
	GTree *dict;

	dict = g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
	                        NULL, (GDestroyNotify)xmmsv_unref);

	g_tree_insert (dict, (gpointer) "type", xmmsv_new_int (type));
	g_tree_insert (dict, (gpointer) "name", xmmsv_new_string (plname));
	g_tree_insert (dict, (gpointer) "namespace", xmmsv_new_string (namespace));

	return dict;
}

void
xmms_collection_changed_msg_send (xmms_coll_dag_t *colldag, GTree *dict)
{
	g_return_if_fail (colldag);
	g_return_if_fail (dict);

	xmms_object_emit_f (XMMS_OBJECT (colldag),
	                    XMMS_IPC_SIGNAL_COLLECTION_CHANGED,
	                    XMMSV_TYPE_DICT,
	                    dict);

	g_tree_destroy (dict);
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

static void
coll_sync_cb (xmms_object_t *object, xmmsv_t *val, gpointer udata)
{
	xmms_coll_sync_schedule_sync ();
}

/** Initializes a new xmms_coll_dag_t.
 *
 * @returns  The newly allocated collection DAG.
 */
xmms_coll_dag_t *
xmms_collection_init (xmms_playlist_t *playlist)
{
	gint i;
	xmms_coll_dag_t *ret;
	xmms_stream_type_t *f;

	ret = xmms_object_new (xmms_coll_dag_t, xmms_collection_destroy);
	ret->mutex = g_mutex_new ();
	ret->playlist = playlist;

	xmms_coll_sync_init (ret);

	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		ret->collrefs[i] = g_hash_table_new_full (g_str_hash, g_str_equal,
		                                          g_free, coll_unref);
	}

	xmms_collection_register_ipc_commands (XMMS_OBJECT (ret));

	/* Connection coll_sync_cb to some signals */
	xmms_object_connect (XMMS_OBJECT (ret),
	                     XMMS_IPC_SIGNAL_COLLECTION_CHANGED,
	                     coll_sync_cb, ret);

	/* FIXME: These signals should trigger COLLECTION_CHANGED */
	xmms_object_connect (XMMS_OBJECT (playlist),
	                     XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
	                     coll_sync_cb, ret);

	xmms_object_connect (XMMS_OBJECT (playlist),
	                     XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                     coll_sync_cb, ret);

	xmms_object_connect (XMMS_OBJECT (playlist),
	                     XMMS_IPC_SIGNAL_PLAYLIST_LOADED,
	                     coll_sync_cb, ret);


	xmms_collection_dag_restore (ret);

	f = _xmms_stream_type_new (XMMS_STREAM_TYPE_BEGIN,
	                           XMMS_STREAM_TYPE_MIMETYPE,
	                           "application/x-xmms2-playlist-entries",
	                           XMMS_STREAM_TYPE_END);
	global_stream_type = g_list_prepend (NULL, f);

	return ret;
}

static void
add_metadata_from_tree (const gchar *key, xmmsv_t *value, gpointer user_data)
{
	add_metadata_from_tree_user_data_t *ud = user_data;

	if (xmmsv_get_type (value) == XMMSV_TYPE_INT32) {
		gint iv;
		xmmsv_get_int (value, &iv);
		xmms_medialib_entry_property_set_int_source (ud->session, ud->entry,
		                                             key,
		                                             iv,
		                                             ud->src);
	} else if (xmmsv_get_type (value) == XMMSV_TYPE_STRING) {
		const gchar *sv;
		xmmsv_get_string (value, &sv);
		xmms_medialib_entry_property_set_str_source (ud->session, ud->entry,
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
static xmmsv_coll_t *
xmms_collection_client_idlist_from_playlist (xmms_coll_dag_t *dag,
                                             const gchar *path,
                                             xmms_error_t *err)
{
	xmms_xform_t *xform;
	GList *lst, *n;
	xmmsv_coll_t *coll;
	xmms_medialib_session_t *session;
	guint src;
	const gchar *buf;

	/* we don't want any effects for playlist, so just report we're rehashing */
	xform = xmms_xform_chain_setup_url (0, path, global_stream_type, TRUE);

	if (!xform) {
		xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "We can't handle this type of playlist or URL");
		return NULL;
	}

	lst = xmms_xform_browse_method (xform, "/", err);
	if (xmms_error_iserror (err)) {
		xmms_object_unref (xform);
		return NULL;
	}

	coll = xmmsv_coll_new (XMMS_COLLECTION_TYPE_IDLIST);
	session = xmms_medialib_begin_write ();
	src = xmms_medialib_source_to_id (session, "plugin/playlist");

	n = lst;
	while (n) {
		xmms_medialib_entry_t entry;

		xmmsv_t *a = n->data;
		xmmsv_t *b;

		if (!xmmsv_dict_get (a, "realpath", &b)) {
			xmms_log_error ("Playlist plugin did not set realpath; probably a bug in plugin");
			xmmsv_unref (a);
			n = g_list_delete_link (n, n);
			continue;
		}

		xmmsv_get_string (b, &buf);
		entry = xmms_medialib_entry_new_encoded (session, buf, err);
		xmmsv_dict_remove (a, "realpath");
		xmmsv_dict_remove (a, "path");

		if (entry) {
			add_metadata_from_tree_user_data_t udata;
			udata.session = session;
			udata.entry = entry;
			udata.src = src;

			xmmsv_dict_foreach(a, add_metadata_from_tree, &udata);

			xmmsv_coll_idlist_append (coll, entry);
		} else {
			xmmsv_get_string (b, &buf);
			xmms_log_error ("couldn't add %s to collection!", buf);
		}

		xmmsv_unref (a);
		n = g_list_delete_link (n, n);
	}

	xmms_medialib_end (session);
	xmms_object_unref (xform);

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
	guint nsid;
	gboolean retval = FALSE;
	guint i;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return;
	}

	g_mutex_lock (dag->mutex);

	/* Unreference the matching collection(s) */
	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
			retval = xmms_collection_unreference (dag, name, i) || retval;
		}
	} else {
		retval = xmms_collection_unreference (dag, name, nsid);
	}

	g_mutex_unlock (dag->mutex);

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
                             xmmsv_coll_t *coll, xmms_error_t *err)
{
	xmmsv_coll_t *existing;
	guint nsid;
	const gchar *alias;
	gchar *newkey = NULL;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return;
	} else if (nsid == XMMS_COLLECTION_NSID_ALL) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "cannot save collection in all namespaces");
		return;
	}

	/* Validate collection structure */
	if (!xmms_collection_validate (dag, coll, name, namespace)) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection structure");
		return;
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
			xmms_collection_dag_replace (dag, nsid, newkey, coll);
			xmmsv_coll_ref (coll);

			XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_UPDATE,
			                             newkey,
			                             namespace);
		}

	/* Save new collection in the table */
	} else {
		newkey = g_strdup (name);
		xmms_collection_dag_replace (dag, nsid, newkey, coll);
		xmmsv_coll_ref (coll);

		XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_ADD,
		                             newkey,
		                             namespace);
	}

	g_mutex_unlock (dag->mutex);

	/* If updating a playlist, trigger PLAYLIST_CHANGED */
	if (nsid == XMMS_COLLECTION_NSID_PLAYLISTS) {
		XMMS_PLAYLIST_COLLECTION_CHANGED_MSG (dag->playlist, newkey);
	}

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
xmmsv_coll_t *
xmms_collection_client_get (xmms_coll_dag_t *dag, const gchar *name,
                            const gchar *namespace, xmms_error_t *err)
{
	xmmsv_coll_t *coll = NULL;
	guint nsid;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return NULL;
	}

	g_mutex_lock (dag->mutex);

	coll = xmms_collection_get_pointer (dag, name, nsid);

	/* Not found! */
	if (coll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such collection");

	/* New reference, will be freed after being put in the return message */
	} else {
		xmmsv_coll_ref (coll);
	}

	g_mutex_unlock (dag->mutex);

	return coll;
}


/** Synchronize collection data to the database (i.e. to disk).
 *
 * @param dag  The collection DAG.
 * @param err  If an error occurs, a message is stored in it.
 */

void
xmms_collection_sync (xmms_coll_dag_t *dag)
{
	g_return_if_fail (dag);

	g_mutex_lock (dag->mutex);

	xmms_collection_dag_save (dag);

	g_mutex_unlock (dag->mutex);
}


void
xmms_collection_client_sync (xmms_coll_dag_t *dag, xmms_error_t *err)
{
	xmms_collection_sync (dag);
}


/** Lists the collections in the given namespace.
 *
 * @param dag  The collection DAG.
 * @param namespace  The namespace to list collections from (can be ALL).
 * @param err  If an error occurs, a message is stored in it.
 * @returns A newly allocated GList with the list of collection names.
 * Remember that it is only the LIST that is copied. Not the entries.
 * The entries are however referenced, and must be unreffed!
 */
GList *
xmms_collection_client_list (xmms_coll_dag_t *dag, const gchar *namespace,
                             xmms_error_t *err)
{
	GList *r = NULL;
	guint nsid;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return NULL;
	}

	g_mutex_lock (dag->mutex);

	/* Get the list of collections in the given namespace */
	xmms_collection_foreach_in_namespace (dag, nsid, prepend_key_string, &r);

	g_mutex_unlock (dag->mutex);

	return r;
}


/** Find all collections in the given namespace that contain a given media.
 *
 * @param dag  The collection DAG.
 * @param mid  The id of the media.
 * @param namespace  The namespace in which to look for collections.
 * @param err  If an error occurs, a message is stored in it.
 * @returns A newly allocated GList with the names of the matching collections.
 */
GList *
xmms_collection_client_find (xmms_coll_dag_t *dag, gint32 mid, const gchar *namespace,
                             xmms_error_t *err)
{
	GHashTable *mediainfo;
	GList *ret = NULL;
	guint nsid;
	gchar *open_name;
	GHashTable *match_table;
	xmmsv_coll_t *coll;

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
	mediainfo = xmms_collection_media_info (mid, err);

	/* While not all collections have been checked, check next */
	while (g_hash_table_find (match_table, find_unchecked, &open_name) != NULL) {
		coll_find_state_t *match = g_new (coll_find_state_t, 1);
		coll = xmms_collection_get_pointer (dag, open_name, nsid);
		if (xmms_collection_media_match (dag, mediainfo, coll, nsid, match_table)) {
			*match = XMMS_COLLECTION_FIND_STATE_MATCH;
		} else {
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
	gboolean retval;
	guint nsid;
	xmmsv_coll_t *from_coll, *to_coll;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return;
	} else if (nsid == XMMS_COLLECTION_NSID_ALL) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "cannot rename collection in all namespaces");
		return;
	}

	g_mutex_lock (dag->mutex);

	from_coll = xmms_collection_get_pointer (dag, from_name, nsid);
	to_coll   = xmms_collection_get_pointer (dag, to_name, nsid);

	/* Input validation */
	if (from_coll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such collection");
		retval = FALSE;

	} else if (to_coll != NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "a collection already exists with the target name");
		retval = FALSE;

	/* Update collection name everywhere */
	} else {
		GTree *dict;

		/* insert new pair in hashtable */
		xmms_collection_dag_replace (dag, nsid, g_strdup (to_name), from_coll);
		xmmsv_coll_ref (from_coll);

		/* remove old pair from hashtable */
		g_hash_table_remove (dag->collrefs[nsid], from_name);

		/* update name in all reference operators */
		coll_rename_infos_t infos = { from_name, to_name, namespace };
		xmms_collection_apply_to_all_collections (dag, rename_references, &infos);

		/* Send _RENAME signal */
		dict = xmms_collection_changed_msg_new (XMMS_COLLECTION_CHANGED_RENAME,
		                                        from_name, namespace);
		g_tree_insert (dict, (gpointer) "newname", xmmsv_new_string (to_name));
		xmms_collection_changed_msg_send (dag, dict);

		retval = TRUE;
	}

	g_mutex_unlock (dag->mutex);

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
GList *
xmms_collection_query_ids (xmms_coll_dag_t *dag, xmmsv_coll_t *coll,
                           gint32 lim_start, gint32 lim_len, xmmsv_t *order,
                           xmms_error_t *err)
{
	GList *res, *n;
	xmmsv_t *fetch, *group, *idval;

	/* no grouping, fetch only id */
	group = xmmsv_new_list ();
	fetch = xmmsv_new_list ();
	idval = xmmsv_new_string ("id");
	xmmsv_list_append (fetch, idval);

	res = xmms_collection_client_query_infos (dag, coll, lim_start, lim_len, order, fetch, group, err);

	/* FIXME: get an uint list directly ! (we're getting ints here actually) */
	for (n = res; n; n = n->next) {
		xmms_medialib_entry_t id;
		xmmsv_t *id_val, *cmdval = n->data;

		xmmsv_dict_get (cmdval, "id", &id_val);
		xmmsv_get_int (id_val, &id);
		n->data = xmmsv_new_int (id);

		xmmsv_unref (cmdval);
	}

	xmmsv_unref (group);
	xmmsv_unref (fetch);
	xmmsv_unref (idval);

	return res;
}


GList *
xmms_collection_client_query_ids (xmms_coll_dag_t *dag, xmmsv_coll_t *coll,
                                  gint32 lim_start, gint32 lim_len, xmmsv_t *order,
                                  xmms_error_t *err)
{
	return xmms_collection_query_ids (dag, coll, lim_start, lim_len, order, err);
}
/** Find the properties of the media matched by a collection.
 *
 * @param dag  The collection DAG.
 * @param coll  The collection used to match media.
 * @param lim_start  The beginning index of the LIMIT statement (0 to disable).
 * @param lim_len  The number of entries of the LIMIT statement (0 to disable).
 * @param order  The list of properties to order by, prefix by '-' to invert (empty to disable).
 * @param fetch  The list of properties to be retrieved.
 * @param group  The list of properties to group by (empty to disable).
 * @param err  If an error occurs, a message is stored in it.
 * @return A list of property dicts for each entry.
 */
GList *
xmms_collection_client_query_infos (xmms_coll_dag_t *dag, xmmsv_coll_t *coll,
                                    gint32 lim_start, gint32 lim_len, xmmsv_t *order,
                                    xmmsv_t *fetch, xmmsv_t *group, xmms_error_t *err)
{
	GList *res = NULL;
	GString *query;

	/* check that fetch is not empty */
	if (xmmsv_list_get_size (fetch) == 0) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "fetch list must not be empty!");
		return NULL;
	}

	/* check for invalid property strings */
	if (!check_string_list (order)) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "invalid order list!");
		return NULL;
	}
	if (!check_string_list (fetch)) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "invalid fetch list!");
		return NULL;
	}
	if (!check_string_list (group)) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "invalid group list!");
		return NULL;
	}

	/* validate the collection to query */
	if (!xmms_collection_validate (dag, coll, NULL, NULL)) {
		if (err) {
			xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection structure");
		}
		return NULL;
	}

	g_mutex_lock (dag->mutex);

	query = xmms_collection_get_query (dag, coll, lim_start, lim_len,
	                                   order, fetch, group);

	g_mutex_unlock (dag->mutex);

	XMMS_DBG ("COLLECTIONS: query_infos with %s", query->str);

	/* Run the query */
	xmms_medialib_session_t *session = xmms_medialib_begin ();
	res = xmms_medialib_select (session, query->str, err);
	xmms_medialib_end (session);

	g_string_free (query, TRUE);

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
xmms_collection_update_pointer (xmms_coll_dag_t *dag, const gchar *name,
                                guint nsid, xmmsv_coll_t *newtarget)
{
	xmms_collection_dag_replace (dag, nsid, g_strdup (name), newtarget);
	xmmsv_coll_ref (newtarget);
}

/** Update the DAG to update the value of the pair with the given key. */
void
xmms_collection_dag_replace (xmms_coll_dag_t *dag,
                             xmms_collection_namespace_id_t nsid,
                             gchar *key, xmmsv_coll_t *newcoll)
{
	g_hash_table_replace (dag->collrefs[nsid], key, newcoll);
}

/** Find the collection structure corresponding to the given name in the given namespace.
 *
 * @param dag  The collection DAG.
 * @param collname  The name of the collection to find.
 * @param nsid  The namespace id.
 * @returns  The collection structure if found, NULL otherwise.
 */
xmmsv_coll_t *
xmms_collection_get_pointer (xmms_coll_dag_t *dag, const gchar *collname,
                             guint nsid)
{
	gint i;
	xmmsv_coll_t *coll = NULL;

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
xmms_collection_get_int_attr (xmmsv_coll_t *coll, const gchar *attrname, gint *val)
{
	gboolean retval = FALSE;
	gint buf;
	gchar *str;
	gchar *endptr;

	if (xmmsv_coll_attribute_get (coll, attrname, &str)) {
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
xmms_collection_set_int_attr (xmmsv_coll_t *coll, const gchar *attrname,
                              gint newval)
{
	gboolean retval = FALSE;
	gchar str[XMMS_MAX_INT_ATTRIBUTE_LEN + 1];
	gint written;

	written = g_snprintf (str, sizeof (str), "%d", newval);
	if (written < XMMS_MAX_INT_ATTRIBUTE_LEN) {
		xmmsv_coll_attribute_set (coll, attrname, str);
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
 * @return The key of the found pair.
 */
const gchar *
xmms_collection_find_alias (xmms_coll_dag_t *dag, guint nsid,
                            xmmsv_coll_t *value, const gchar *key)
{
	const gchar *otherkey = NULL;
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
xmms_collection_get_random_media (xmms_coll_dag_t *dag, xmmsv_coll_t *source)
{
	GList *res;
	xmms_medialib_entry_t mid = 0;
	xmmsv_t *rorder = xmmsv_new_list ();
	xmmsv_t *randval = xmmsv_new_string ("~RANDOM()");

	/* FIXME: Temporary hack to allow custom ordering functions */
	xmmsv_list_append (rorder, randval);

	res = xmms_collection_query_ids (dag, source, 0, 1, rorder, NULL);

	if (res != NULL) {
		xmmsv_t *val = (xmmsv_t *) res->data;
		xmmsv_get_int (val, &mid);
		xmmsv_unref (val);
		g_list_free (res);
	}

	xmmsv_unref (rorder);
	xmmsv_unref (randval);

	return mid;
}

/** @} */



/** Free the collection DAG and other memory in the xmms_coll_dag_t
 *
 *  This will free all collections in the DAG!
 */
static void
xmms_collection_destroy (xmms_object_t *object)
{
	gint i;
	xmms_coll_dag_t *dag = (xmms_coll_dag_t *)object;

	g_return_if_fail (dag);

	xmms_coll_sync_shutdown ();
	xmms_collection_dag_save (dag);

	g_mutex_free (dag->mutex);

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
xmms_collection_validate (xmms_coll_dag_t *dag, xmmsv_coll_t *coll,
                          const gchar *save_name, const gchar *save_namespace)
{
	/* Special validation checks for the Playlists namespace */
	if (save_namespace != NULL &&
	    strcmp (save_namespace, XMMS_COLLECTION_NS_PLAYLISTS) == 0) {
		/* only accept idlists */
		if (xmmsv_coll_get_type (coll) != XMMS_COLLECTION_TYPE_IDLIST) {
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
xmms_collection_validate_recurs (xmms_coll_dag_t *dag, xmmsv_coll_t *coll,
                                 const gchar *save_name, const gchar *save_namespace)
{
	guint num_operands = 0;
	xmmsv_coll_t *op, *ref;
	gchar *attr, *attr2;
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
			return FALSE;
		}

		/* check if referenced collection exists */
		xmmsv_coll_attribute_get (coll, "reference", &attr);
		if (attr == NULL) {
			return FALSE;
		} else if (strcmp (attr, "All Media") != 0) {
			xmmsv_coll_attribute_get (coll, "namespace", &attr2);

			if (attr2 == NULL) {
				return FALSE;
			}

			nsid = xmms_collection_get_namespace_id (attr2);
			if (nsid == XMMS_COLLECTION_NSID_INVALID) {
				return FALSE;
			}

			g_mutex_lock (dag->mutex);
			ref = xmms_collection_get_pointer (dag, attr, nsid);
			if (ref == NULL) {
				g_mutex_unlock (dag->mutex);
				return FALSE;
			}

			if (save_name && save_namespace) {
				/* self-reference is of course forbidden */
				if (strcmp (attr, save_name) == 0 &&
				    strcmp (attr2, save_namespace) == 0) {

					g_mutex_unlock (dag->mutex);
					return FALSE;

				/* check if the referenced coll references this one (loop!) */
				} else if (xmms_collection_has_reference_to (dag, ref, save_name,
				                                             save_namespace)) {
					g_mutex_unlock (dag->mutex);
					return FALSE;
				}
			}

			g_mutex_unlock (dag->mutex);
		} else {
			/* "All Media" reference, so no referenced coll pointer */
			ref = NULL;
		}

		/* ensure that the operand is consistent with the reference infos */
		if (num_operands == 1) {
			xmmsv_t *val;
			xmmsv_list_get (xmmsv_coll_operands_get (coll), 0, &val);
			xmmsv_get_coll (val, &op);

			if (op != ref) {
				return FALSE;
			}
		}
		break;

	case XMMS_COLLECTION_TYPE_UNION:
	case XMMS_COLLECTION_TYPE_INTERSECTION:
		/* need operand(s) */
		if (num_operands == 0) {
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_COMPLEMENT:
		/* one operand */
		if (num_operands != 1) {
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_FILTER:
		if (!xmmsv_coll_attribute_get (coll, "operation", &attr)) {
			return FALSE;
		}

		if (strcmp (attr, "has") == 0) {
			/* one operand */
			if (num_operands != 1) {
				return FALSE;
			}

		} else if (strcmp (attr, "=") == 0
				|| strcmp (attr, "<") == 0
				|| strcmp (attr, ">") == 0
				|| strcmp (attr, "match") == 0) {
			/* one operand */
			if (num_operands != 1) {
				return FALSE;
			}

			if (!xmmsv_coll_attribute_get (coll, "value", &attr)) {
				return FALSE;
			}
		}

		/* check that type equals "id", "value"
		 * or not set (defaults to "value") */
		if (xmmsv_coll_attribute_get (coll, "type", &attr)) {
			if (strcmp (attr, "id") && strcmp (attr, "value")) {
				return FALSE;
			}
		}
		break;

	case XMMS_COLLECTION_TYPE_IDLIST:
		if (!xmmsv_coll_attribute_get (coll, "type", &attr)) {
			attr = (char*)"list";
		}

		if (strcmp (attr, "list") == 0 || strcmp (attr, "queue") == 0) {
			/* no operand */
			if (num_operands > 0) {
				return FALSE;
			}
		} else if (strcmp (attr, "pshuffle") == 0) {
			if (num_operands != 1) {
				return FALSE;
			}
		} else {
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_UNIVERSE:
		/* No operands */
		if (num_operands > 0) {
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_ORDER:
		/* One operand */
		if (num_operands != 1) {
			return FALSE;
		}

		if (xmmsv_coll_attribute_get (coll, "order", &attr) &&
				strcmp (attr, "ASC") != 0 &&
				strcmp (attr, "DESC") != 0) {
			return FALSE;
		}

		if (!xmmsv_coll_attribute_get (coll, "type", &attr) ||
				strcmp (attr, "value") == 0) {
			/* If it's a sorting on values we need a field to sort on */
			if (!xmmsv_coll_attribute_get (coll, "field", &attr)) {
				return FALSE;
			}
		}
		break;

	case XMMS_COLLECTION_TYPE_LIMIT:
		/* One operand */
		if (num_operands != 1) {
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_MEDIASET:
		/* One operand */
		if (num_operands != 1) {
			return FALSE;
		}
		break;

	/* invalid type */
	default:
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

			xmmsv_t *val;
			xmmsv_list_iter_entry (iter, &val);
			xmmsv_get_coll (val, &op);

			if (!xmms_collection_validate_recurs (dag, op, save_name,
			                                      save_namespace)) {
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
xmms_collection_unreference (xmms_coll_dag_t *dag, const gchar *name, guint nsid)
{
	xmmsv_coll_t *existing, *active_pl;
	gboolean retval = FALSE;

	existing  = g_hash_table_lookup (dag->collrefs[nsid], name);
	active_pl = g_hash_table_lookup (dag->collrefs[XMMS_COLLECTION_NSID_PLAYLISTS],
	                                 XMMS_ACTIVE_PLAYLIST);

	/* Unref if collection exists, and is not pointed at by _active playlist */
	if (existing != NULL && existing != active_pl) {
		const gchar *matchkey;
		const gchar *nsname = xmms_collection_get_namespace_string (nsid);
		coll_rebind_infos_t infos = { name, nsname, existing, NULL };

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
	guint nsid;

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
xmms_collection_has_reference_to (xmms_coll_dag_t *dag, xmmsv_coll_t *coll,
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
xmms_collection_foreach_in_namespace (xmms_coll_dag_t *dag, guint nsid, GHFunc f, void *udata)
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
                                     xmmsv_coll_t *coll,
                                     FuncApplyToColl f, void *udata)
{
	xmms_collection_apply_to_collection_recurs (dag, coll, NULL, f, udata);
}

/* Internal function used for recursion (parent param, NULL by default) */
static void
xmms_collection_apply_to_collection_recurs (xmms_coll_dag_t *dag,
                                            xmmsv_coll_t *coll,
                                            xmmsv_coll_t *parent,
                                            FuncApplyToColl f, void *udata)
{
	xmmsv_coll_t *op;

	/* Apply the function to the operator. */
	f (dag, coll, parent, udata);

	/* Recurse into the operands (if not a reference) */
	if (xmmsv_coll_get_type (coll) != XMMS_COLLECTION_TYPE_REFERENCE) {
		xmmsv_list_iter_t *iter;
		xmmsv_get_list_iter (xmmsv_coll_operands_get (coll), &iter);

		for (xmmsv_list_iter_first (iter);
		     xmmsv_list_iter_valid (iter);
		     xmmsv_list_iter_next (iter)) {

			xmmsv_t *val;
			xmmsv_list_iter_entry (iter, &val);

			xmmsv_get_coll (val, &op);

			xmms_collection_apply_to_collection_recurs (dag, op, coll, f,
			                                            udata);
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
	GList **list = (GList**)udata;
	*list = g_list_prepend (*list, xmmsv_new_string (key));
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
	xmmsv_coll_t *coll = (xmmsv_coll_t*)val;

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
void
bind_all_references (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, xmmsv_coll_t *parent, void *udata)
{
	if (xmmsv_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE) {
		xmmsv_coll_t *target;
		gchar *target_name;
		gchar *target_namespace;
		gint   target_nsid;

		xmmsv_coll_attribute_get (coll, "reference", &target_name);
		xmmsv_coll_attribute_get (coll, "namespace", &target_namespace);
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

		xmmsv_coll_add_operand (coll, target);
	}
}

/**
 * If a reference, rebind the given operator to the new operator
 * representing the referenced collection (pointers and so are in the
 * udata structure).
 */
static void
rebind_references (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, xmmsv_coll_t *parent, void *udata)
{
	if (xmmsv_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE) {
		coll_rebind_infos_t *infos;

		gchar *target_name = NULL;
		gchar *target_namespace = NULL;

		infos = (coll_rebind_infos_t*)udata;

		/* FIXME: Or only compare operand vs oldtarget ? */

		xmmsv_coll_attribute_get (coll, "reference", &target_name);
		xmmsv_coll_attribute_get (coll, "namespace", &target_namespace);
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
rename_references (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, xmmsv_coll_t *parent, void *udata)
{
	if (xmmsv_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE) {
		coll_rename_infos_t *infos;

		gchar *target_name = NULL;
		gchar *target_namespace = NULL;

		infos = (coll_rename_infos_t*)udata;

		xmmsv_coll_attribute_get (coll, "reference", &target_name);
		xmmsv_coll_attribute_get (coll, "namespace", &target_namespace);
		if (strcmp (infos->oldname, target_name) == 0 &&
		    strcmp (infos->namespace, target_namespace) == 0) {
			xmmsv_coll_attribute_set (coll, "reference", infos->newname);
		}
	}
}

/**
 * Strip reference operators to the given collection by rebinding the
 * parent directly to the pointed operator.
 */
static void
strip_references (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, xmmsv_coll_t *parent, void *udata)
{
	xmmsv_coll_t *op;
	coll_rebind_infos_t *infos;
	gchar *target_name = NULL;
	gchar *target_namespace = NULL;
	xmmsv_list_iter_t *iter;
	xmmsv_t *tmp;

	infos = (coll_rebind_infos_t*)udata;

	xmmsv_get_list_iter (xmmsv_coll_operands_get (coll), &iter);
	for (xmmsv_list_iter_first (iter);
	     xmmsv_list_iter_valid (iter);
	     xmmsv_list_iter_next (iter)) {

		xmmsv_list_iter_entry (iter, &tmp);
		xmmsv_get_coll (tmp, &op);

		/* Skip if not potential reference */
		if (xmmsv_coll_get_type (op) != XMMS_COLLECTION_TYPE_REFERENCE) {
			continue;
		}

		xmmsv_coll_attribute_get (op, "reference", &target_name);
		xmmsv_coll_attribute_get (op, "namespace", &target_namespace);
		if (strcmp (infos->name, target_name) != 0 ||
		    strcmp (infos->namespace, target_namespace) != 0) {
			continue;
		}

		/* Rebind coll to ref'd operand directly, effectively strip reference */
		/* FIXME: Do we really need to do this _clear? */
		xmmsv_list_clear (xmmsv_coll_operands_get (op));

		xmmsv_list_iter_remove (iter);

		tmp = xmmsv_new_coll (infos->oldtarget);
		xmmsv_list_iter_insert (iter, tmp);
		xmmsv_unref (tmp);
	}
	xmmsv_list_iter_explicit_destroy (iter);
}

/**
 * Check if the current operator is a reference to a given collection,
 * and if so, update the structure passed as userdata.
 */
static void
check_for_reference (xmms_coll_dag_t *dag, xmmsv_coll_t *coll, xmmsv_coll_t *parent, void *udata)
{
	coll_refcheck_t *check = (coll_refcheck_t*)udata;
	if (xmmsv_coll_get_type (coll) == XMMS_COLLECTION_TYPE_REFERENCE && !check->found) {
		gchar *target_name, *target_namespace;

		xmmsv_coll_attribute_get (coll, "reference", &target_name);
		xmmsv_coll_attribute_get (coll, "namespace", &target_namespace);
		if (strcmp (check->target_name, target_name) == 0 &&
		    strcmp (check->target_namespace, target_namespace) == 0) {
			check->found = TRUE;
		} else {
			xmmsv_coll_t *op;
			xmmsv_t *tmp;

			if (xmmsv_list_get (xmmsv_coll_operands_get (coll), 0, &tmp)) {
				xmmsv_get_coll (tmp, &op);
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
	xmmsv_coll_unref (coll);
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
	gchar *coll_name = key;
	coll_find_state_t *state = value;
	GList **list = udata;
	if (*state == XMMS_COLLECTION_FIND_STATE_MATCH) {
		*list = g_list_prepend (*list, xmmsv_new_string (coll_name));
	}
}

/** Determine whether the mediainfos match the given collection.
 *
 * @param dag  The collection DAG.
 * @param mediainfo  The properties of the media to match against.
 * @param coll  The collection to match with the mediainfos.
 * @param nsid  The namespace id of the collection.
 * @param match_table  The match_table for all collections in that namespace.
 * @return  TRUE if the collection matches, FALSE otherwise.
 */
static gboolean
xmms_collection_media_match (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                             xmmsv_coll_t *coll, guint nsid,
                             GHashTable *match_table)
{
	gboolean match = FALSE;
	xmmsv_coll_t *op;
	gchar *attr1 = NULL, *attr2 = NULL;
	xmmsv_t *val;
	xmms_medialib_entry_t entry, id;
	xmmsv_list_iter_t *iter;

	switch (xmmsv_coll_get_type (coll)) {
	case XMMS_COLLECTION_TYPE_REFERENCE:
		if (xmmsv_coll_attribute_get (coll, "reference", &attr1)) {
			if (strcmp (attr1, "All Media") == 0) {
				match = TRUE;
			} else if (xmmsv_coll_attribute_get (coll, "namespace", &attr2)) {
				match = xmms_collection_media_match_reference (dag, mediainfo,
				                                               coll, nsid,
				                                               match_table,
				                                               attr1, attr2);
			}
		}
		break;

	case XMMS_COLLECTION_TYPE_UNION:
		/* if ANY matches */
		xmmsv_get_list_iter (xmmsv_coll_operands_get (coll), &iter);

		for (xmmsv_list_iter_first (iter);
		     !match && xmmsv_list_iter_valid (iter);
		     xmmsv_list_iter_next (iter)) {

			xmmsv_list_iter_entry (iter, &val);
			xmmsv_get_coll (val, &op);

			match = xmms_collection_media_match (dag, mediainfo, op,
			                                     nsid, match_table);
		}
		xmmsv_list_iter_explicit_destroy (iter);
		break;

	case XMMS_COLLECTION_TYPE_INTERSECTION:
		/* if ALL match */
		match = TRUE;
		xmmsv_get_list_iter (xmmsv_coll_operands_get (coll), &iter);

		for (xmmsv_list_iter_first (iter);
		     match && xmmsv_list_iter_valid (iter);
		     xmmsv_list_iter_next (iter)) {

			xmmsv_list_iter_entry (iter, &val);
			xmmsv_get_coll (val, &op);

			match = xmms_collection_media_match (dag, mediainfo, op,
			                                     nsid, match_table);
		}
		xmmsv_list_iter_explicit_destroy (iter);
		break;

	case XMMS_COLLECTION_TYPE_COMPLEMENT:
		/* invert result from operand */
		match = !xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                              nsid, match_table);
		break;

	case XMMS_COLLECTION_TYPE_FILTER:
		if (!xmmsv_coll_attribute_get (coll, "operation", &attr1))
			break;

		if (strcmp (attr1, "has")) {
			match = xmms_collection_media_filter_has (dag, mediainfo, coll,
													  nsid, match_table);
		} else if (strcmp (attr1, "=")) {
			match = xmms_collection_media_filter_equals (dag, mediainfo, coll,
														nsid, match_table);
		} else if (strcmp (attr1, "match")) {
			match = xmms_collection_media_filter_match (dag, mediainfo, coll,
														   nsid, match_table);
		} else if (strcmp (attr1, "<")) {
			match = xmms_collection_media_filter_smaller (dag, mediainfo, coll,
														  nsid, match_table);
		} else if (strcmp (attr1, ">")) {
			match = xmms_collection_media_filter_greater (dag, mediainfo, coll,
														  nsid, match_table);
		}
		break;

	case XMMS_COLLECTION_TYPE_IDLIST:
		/* check if id in idlist */
		val = g_hash_table_lookup (mediainfo, "id");
		if (val != NULL) {
			xmmsv_get_int (val, &id);

			xmmsv_get_list_iter (xmmsv_coll_idlist_get (coll), &iter);
			for (xmmsv_list_iter_first (iter);
			     xmmsv_list_iter_valid (iter);
			     xmmsv_list_iter_next (iter)) {

				xmmsv_list_iter_entry_int (iter, &entry);
				if (entry == id) {
					match = TRUE;
					break;
				}
			}
			xmmsv_list_iter_explicit_destroy (iter);
		}
		break;

	/* invalid type */
	default:
		XMMS_DBG ("invalid collection operator in xmms_collection_media_match");
		g_assert_not_reached ();
		break;
	}

	return match;
}

/** Determine whether the mediainfos match the given reference operator.
 *
 * @param dag  The collection DAG.
 * @param mediainfo  The properties of the media to match against.
 * @param coll  The collection (ref op) to match with the mediainfos.
 * @param nsid  The namespace id of the collection.
 * @param match_table  The match_table for all collections in that namespace.
 * @param refname  The name of the referenced collection.
 * @param refns  The namespace of the referenced collection.
 * @return  TRUE if the collection matches, FALSE otherwise.
 */
static gboolean
xmms_collection_media_match_reference (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                       xmmsv_coll_t *coll, guint nsid,
                                       GHashTable *match_table,
                                       const gchar *refname, const gchar *refns)
{
	gboolean match;
	guint refnsid;
	coll_find_state_t *matchstate;

	/* Same NS, should be in the match table */
	refnsid = xmms_collection_get_namespace_id (refns);
	if (refnsid == nsid) {
		matchstate = g_hash_table_lookup (match_table, refname);
		if (*matchstate == XMMS_COLLECTION_FIND_STATE_UNCHECKED) {
			/* Check ref'd collection match status and save it */
			matchstate = g_new (coll_find_state_t, 1);
			match = xmms_collection_media_match_operand (dag,
			                                             mediainfo,
			                                             coll, nsid,
			                                             match_table);

			if (match) {
				*matchstate = XMMS_COLLECTION_FIND_STATE_MATCH;
			} else {
				*matchstate = XMMS_COLLECTION_FIND_STATE_NOMATCH;
			}

			g_hash_table_replace (match_table, g_strdup (refname), matchstate);

		} else {
			match = (*matchstate == XMMS_COLLECTION_FIND_STATE_MATCH);
		}

	/* In another NS, just check if it matches */
	} else {
		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);
	}

	return match;
}

/** Determine whether the mediainfos match the first operand of the
 * given operator.
 *
 * @param dag  The collection DAG.
 * @param mediainfo  The properties of the media to match against.
 * @param coll  Match the mediainfos with the operand of that collection.
 * @param nsid  The namespace id of the collection.
 * @param match_table  The match_table for all collections in that namespace.
 * @return  TRUE if the collection matches, FALSE otherwise.
 */
static gboolean
xmms_collection_media_match_operand (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                     xmmsv_coll_t *coll, guint nsid,
                                     GHashTable *match_table)
{
	xmmsv_coll_t *op;
	xmmsv_t *tmp;
	gboolean match = FALSE;

	if (xmmsv_list_get (xmmsv_coll_operands_get (coll), 0, &tmp)) {
		xmmsv_get_coll (tmp, &op);

		match = xmms_collection_media_match (dag, mediainfo, op, nsid, match_table);
	}

	return match;
}

/** Get all the properties for the given media.
 *
 * @param mid  The id of the media.
 * @return  A HashTable with all the properties.
 */
static GHashTable *
xmms_collection_media_info (xmms_medialib_entry_t mid, xmms_error_t *err)
{
	GList *res;
	GList *n;
	GHashTable *infos;
	gchar *name;
	const gchar *buf;
	xmmsv_t *cmdval;
	xmmsv_t *value;
	guint state;

	/* FIXME: could probably reuse tree from medialib_info directly. ignores sources? */
	res = xmms_medialib_info_list (NULL, mid, err);

	/* Transform the list into a HashMap */
	infos = g_hash_table_new_full (g_str_hash, g_str_equal,
	                               g_free, (GDestroyNotify) xmmsv_unref);
	for (state = 0, n = res; n; state = (state + 1) % 3, n = n->next) {
		switch (state) {
		case 0:  /* source */
			break;

		case 1:  /* prop name */
			cmdval = n->data;
			xmmsv_get_string (cmdval, &buf);
			name = g_strdup (buf);
			break;

		case 2:  /* prop value */
			value = xmmsv_ref (n->data);

			/* Only insert the first source */
			if (g_hash_table_lookup (infos, name) == NULL) {
				g_hash_table_replace (infos, name, value);
			}
			break;
		}

		xmmsv_unref (n->data);
	}

	g_list_free (res);

	return infos;
}

/** Get the string associated to the property of the mediainfo
 *  identified by the "field" attribute of the collection.
 *
 * @return  The property value as a string.
 */
static gboolean
filter_get_mediainfo_field_string (xmmsv_coll_t *coll,
                                   GHashTable *mediainfo, gchar **val)
{
	gboolean retval = FALSE;
	gchar *attr;
	xmmsv_t *cmdval;

	if (xmmsv_coll_attribute_get (coll, "field", &attr)) {
		cmdval = g_hash_table_lookup (mediainfo, attr);
		if (cmdval != NULL) {
			switch (xmmsv_get_type (cmdval)) {
			case XMMSV_TYPE_STRING:
			{
				const gchar *s;
				xmmsv_get_string (cmdval, &s);
				*val = g_strdup (s);
				retval = TRUE;
				break;
			}
			case XMMSV_TYPE_INT32:
			{
				gint i;
				xmmsv_get_int (cmdval, &i);
				*val = g_strdup_printf ("%d", i);
				retval = TRUE;
				break;
			}
			default:
				break;
			}
		}
	}

	return retval;
}

/** Get the integer associated to the property of the mediainfo
 *  identified by the "field" attribute of the collection.
 *
 * @return  The property value as an integer.
 */
static gboolean
filter_get_mediainfo_field_int (xmmsv_coll_t *coll, GHashTable *mediainfo, gint *val)
{
	gboolean retval = FALSE;
	gchar *attr;
	xmmsv_t *cmdval;

	if (xmmsv_coll_attribute_get (coll, "field", &attr)) {
		cmdval = g_hash_table_lookup (mediainfo, attr);
		if (cmdval != NULL && xmmsv_get_type (cmdval) == XMMSV_TYPE_INT32) {
			xmmsv_get_int (cmdval, val);
			retval = TRUE;
		}
	}

	return retval;
}

/* Get the string value of the "value" attribute of the collection. */
static gboolean
filter_get_operator_value_string (xmmsv_coll_t *coll, const gchar **val)
{
	gchar *attr;
	gboolean valid;

	valid = xmmsv_coll_attribute_get (coll, "value", &attr);
	if (valid) {
		*val = attr;
	}

	return valid;
}

/* Get the integer value of the "value" attribute of the collection. */
static gboolean
filter_get_operator_value_int (xmmsv_coll_t *coll, gint *val)
{
	gint buf;
	gboolean valid;

	valid = xmms_collection_get_int_attr (coll, "value", &buf);
	if (valid) {
		*val = buf;
	}

	return valid;
}

/* Check whether the given operator has the "case-sensitive" attribute
 * or not. */
static gboolean
filter_get_operator_case (xmmsv_coll_t *coll, gboolean *val)
{
	gchar *attr;

	if (xmmsv_coll_attribute_get (coll, "case-sensitive", &attr)) {
		*val = (strcmp (attr, "true") == 0);
	}
	else {
		*val = FALSE;
	}

	return TRUE;
}

/* Check whether the HAS filter operator matches the mediainfo. */
static gboolean
xmms_collection_media_filter_has (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                  xmmsv_coll_t *coll, guint nsid,
                                  GHashTable *match_table)
{
	gboolean match = FALSE;
	gchar *mediaval;

	/* If operator matches, recurse upwards in the operand */
	if (filter_get_mediainfo_field_string (coll, mediainfo, &mediaval)) {
		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);

		g_free (mediaval);
	}

	return match;
}

/* Check whether the MATCH filter operator matches the mediainfo. */
static gboolean
xmms_collection_media_filter_equals (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                    xmmsv_coll_t *coll, guint nsid,
                                    GHashTable *match_table)
{
	gboolean match = FALSE;
	gchar *mediaval = NULL;
	const gchar *opval;
	gboolean case_sens;

	if (filter_get_mediainfo_field_string (coll, mediainfo, &mediaval) &&
	    filter_get_operator_value_string (coll, &opval) &&
	    filter_get_operator_case (coll, &case_sens)) {

		if (case_sens) {
			match = (strcmp (mediaval, opval) == 0);
		} else {
			match = (g_ascii_strcasecmp (mediaval, opval) == 0);
		}
	}

	/* If operator matches, recurse upwards in the operand */
	if (match) {
		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);
	}

	if (mediaval != NULL) {
		g_free (mediaval);
	}

	return match;
}

/* Check whether the MATCH filter operator matches the mediainfo. */
static gboolean
xmms_collection_media_filter_match (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                       xmmsv_coll_t *coll, guint nsid,
                                       GHashTable *match_table)
{
	gboolean match = FALSE;
	gchar *buf, *opval, *mediaval;
	const gchar *s;
	gboolean case_sens;

	if (filter_get_mediainfo_field_string (coll, mediainfo, &buf) &&
	    filter_get_operator_value_string (coll, &s) &&
	    filter_get_operator_case (coll, &case_sens)) {

		/* Prepare values */
		if (case_sens) {
			opval = g_strdup (s);
			mediaval = g_strdup (buf);
		} else {
			opval = g_utf8_strdown (s, -1);
			mediaval = g_utf8_strdown (buf, -1);
		}

		match = g_pattern_match_simple (opval, mediaval);

		g_free (buf);
		g_free (opval);
		g_free (mediaval);

		/* If operator matches, recurse upwards in the operand */
		if (match) {
			match = xmms_collection_media_match_operand (dag, mediainfo, coll,
			                                             nsid, match_table);
		}
	}

	return match;
}

/* Check whether the SMALLER filter operator matches the mediainfo. */
static gboolean
xmms_collection_media_filter_smaller (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                      xmmsv_coll_t *coll, guint nsid,
                                      GHashTable *match_table)
{
	gboolean match = FALSE;
	gint mediaval;
	gint opval;

	/* If operator matches, recurse upwards in the operand */
	if (filter_get_mediainfo_field_int (coll, mediainfo, &mediaval) &&
	    filter_get_operator_value_int (coll, &opval) &&
	    (mediaval < opval) ) {

		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);
	}

	return match;
}

/* Check whether the GREATER filter operator matches the mediainfo. */
static gboolean
xmms_collection_media_filter_greater (xmms_coll_dag_t *dag, GHashTable *mediainfo,
                                      xmmsv_coll_t *coll, guint nsid,
                                      GHashTable *match_table)
{
	gboolean match = FALSE;
	gint mediaval;
	gint opval;

	/* If operator matches, recurse upwards in the operand */
	if (filter_get_mediainfo_field_int (coll, mediainfo, &mediaval) &&
	    filter_get_operator_value_int (coll, &opval) &&
	    (mediaval > opval) ) {

		match = xmms_collection_media_match_operand (dag, mediainfo, coll,
		                                             nsid, match_table);
	}

	return match;
}
