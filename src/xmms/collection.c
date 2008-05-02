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
#include "xmmspriv/xmms_xform.h"
#include "xmmspriv/xmms_streamtype.h"
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"


/* Internal helper structures */

typedef struct {
	const gchar *name;
	const gchar *namespace;
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
	const gchar *key;
	xmmsc_coll_t *value;
} coll_table_pair_t;

typedef enum {
	XMMS_COLLECTION_FIND_STATE_UNCHECKED,
	XMMS_COLLECTION_FIND_STATE_MATCH,
	XMMS_COLLECTION_FIND_STATE_NOMATCH,
} coll_find_state_t;

typedef struct add_metadata_from_hash_user_data_St {
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	guint src;
} add_metadata_from_hash_user_data_t;

static GList *global_stream_type;

/* Functions */

static void xmms_collection_destroy (xmms_object_t *object);

static gboolean xmms_collection_validate (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, gchar *save_name, gchar *save_namespace);
static gboolean xmms_collection_validate_recurs (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, gchar *save_name, gchar *save_namespace);
static gboolean xmms_collection_unreference (xmms_coll_dag_t *dag, gchar *name, guint nsid);

static gboolean xmms_collection_has_reference_to (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, gchar *tg_name, gchar *tg_ns);

static void xmms_collection_apply_to_collection_recurs (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, FuncApplyToColl f, void *udata);

static void call_apply_to_coll (gpointer name, gpointer coll, gpointer udata);
static void prepend_key_string (gpointer key, gpointer value, gpointer udata);
static gboolean value_match_save_key (gpointer key, gpointer val, gpointer udata);

static void rebind_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);
static void rename_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);
static void strip_references (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);
static void check_for_reference (xmms_coll_dag_t *dag, xmmsc_coll_t *coll, xmmsc_coll_t *parent, void *udata);

static void coll_unref (void *coll);

static GHashTable *xmms_collection_media_info (guint mid, xmms_error_t *err);

static gboolean filter_get_mediainfo_field_string (xmmsc_coll_t *coll, GHashTable *mediainfo, gchar **val);
static gboolean filter_get_mediainfo_field_int (xmmsc_coll_t *coll, GHashTable *mediainfo, gint *val);
static gboolean filter_get_operator_value_string (xmmsc_coll_t *coll, gchar **val);
static gboolean filter_get_operator_value_int (xmmsc_coll_t *coll, gint *val);
static gboolean filter_get_operator_case (xmmsc_coll_t *coll, gboolean *val);

static void build_match_table (gpointer key, gpointer value, gpointer udata);
static gboolean find_unchecked (gpointer name, gpointer value, gpointer udata);
static void build_list_matches (gpointer key, gpointer value, gpointer udata);

static gboolean xmms_collection_media_match (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_match_operand (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_match_reference (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table, gchar *refname, gchar *refns);
static gboolean xmms_collection_media_filter_has (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_equals (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_match (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_smaller (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static gboolean xmms_collection_media_filter_greater (xmms_coll_dag_t *dag, GHashTable *mediainfo, xmmsc_coll_t *coll, guint nsid, GHashTable *match_table);
static xmmsc_coll_t *xmms_collection_idlist_from_pls (xmms_coll_dag_t *dag, gchar *mediainfo, xmms_error_t *err);


XMMS_CMD_DEFINE  (collection_get, xmms_collection_get, xmms_coll_dag_t *, COLL, STRING, STRING);
XMMS_CMD_DEFINE  (collection_list, xmms_collection_list, xmms_coll_dag_t *, LIST, STRING, NONE);
XMMS_CMD_DEFINE3 (collection_save, xmms_collection_save, xmms_coll_dag_t *, NONE, STRING, STRING, COLL);
XMMS_CMD_DEFINE  (collection_remove, xmms_collection_remove, xmms_coll_dag_t *, NONE, STRING, STRING);
XMMS_CMD_DEFINE  (collection_find, xmms_collection_find, xmms_coll_dag_t *, LIST, UINT32, STRING);
XMMS_CMD_DEFINE3 (collection_rename, xmms_collection_rename, xmms_coll_dag_t *, NONE, STRING, STRING, STRING);
XMMS_CMD_DEFINE  (collection_from_pls, xmms_collection_idlist_from_pls, xmms_coll_dag_t *, COLL, STRING, NONE);
XMMS_CMD_DEFINE  (collection_sync, xmms_collection_sync, xmms_coll_dag_t *, NONE, NONE, NONE);


XMMS_CMD_DEFINE4 (query_ids, xmms_collection_query_ids, xmms_coll_dag_t *, LIST, COLL, UINT32, UINT32, STRINGLIST);
XMMS_CMD_DEFINE6 (query_infos, xmms_collection_query_infos, xmms_coll_dag_t *, LIST, COLL, UINT32, UINT32, STRINGLIST, STRINGLIST, STRINGLIST);


GHashTable *
xmms_collection_changed_msg_new (xmms_collection_changed_actions_t type,
                                 const gchar *plname, const gchar *namespace)
{
	GHashTable *dict;
	xmms_object_cmd_value_t *val;
	dict = g_hash_table_new_full (g_str_hash, g_str_equal,
	                              NULL, (GDestroyNotify)xmms_object_cmd_value_unref);
	val = xmms_object_cmd_value_int_new (type);
	g_hash_table_insert (dict, (gpointer) "type", val);
	val = xmms_object_cmd_value_str_new (plname);
	g_hash_table_insert (dict, (gpointer) "name", val);
	val = xmms_object_cmd_value_str_new (namespace);
	g_hash_table_insert (dict, (gpointer) "namespace", val);

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
	xmms_stream_type_t *f;

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

	xmms_object_cmd_add (XMMS_OBJECT (ret),
	                     XMMS_IPC_CMD_IDLIST_FROM_PLS,
	                     XMMS_CMD_FUNC (collection_from_pls));

	xmms_object_cmd_add (XMMS_OBJECT (ret),
	                     XMMS_IPC_CMD_COLLECTION_SYNC,
	                     XMMS_CMD_FUNC (collection_sync));

	xmms_collection_dag_restore (ret);

	f = _xmms_stream_type_new (NULL,
	                           XMMS_STREAM_TYPE_MIMETYPE,
	                           "application/x-xmms2-playlist-entries",
	                           XMMS_STREAM_TYPE_END);
	global_stream_type = g_list_prepend (NULL, f);

	return ret;
}

static void
add_metadata_from_hash (gpointer key, gpointer value, gpointer user_data)
{
	add_metadata_from_hash_user_data_t *ud = user_data;
	xmms_object_cmd_value_t *b = value;

	if (b->type == XMMS_OBJECT_CMD_ARG_INT32) {
		xmms_medialib_entry_property_set_int_source (ud->session, ud->entry,
		                                             (const gchar *)key,
		                                             b->value.int32,
		                                             ud->src);
	} else if (b->type == XMMS_OBJECT_CMD_ARG_STRING) {
		xmms_medialib_entry_property_set_str_source (ud->session, ud->entry,
		                                             (const gchar *)key,
		                                             b->value.string,
		                                             ud->src);
	}
}

/** Create a idlist from a playlist file
 * @param dag  The collection DAG.
 * @param path  URL to the playlist file
 * @param err  If error occurs, a message is stored in this variable.
 * @returns  A idlist
 */
static xmmsc_coll_t *
xmms_collection_idlist_from_pls (xmms_coll_dag_t *dag, gchar *path, xmms_error_t *err)
{
	xmms_xform_t *xform;
	GList *lst, *n;
	xmmsc_coll_t *coll;
	xmms_medialib_session_t *session;
	guint src;

	xform = xmms_xform_chain_setup_url_without_effects (0, path, global_stream_type);

	if (!xform) {
		xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "We can't handle this type of playlist or URL");
		return NULL;
	}

	lst = xmms_xform_browse_method (xform, "/", err);
	if (xmms_error_iserror (err)) {
		xmms_object_unref (xform);
		return NULL;
	}

	coll = xmmsc_coll_new (XMMS_COLLECTION_TYPE_IDLIST);
	session = xmms_medialib_begin_write ();
	src = xmms_medialib_source_to_id (session, "plugin/playlist");

	n = lst;
	while (n) {
		xmms_medialib_entry_t entry;

		xmms_object_cmd_value_t *a = n->data;
		xmms_object_cmd_value_t *b;
		b = g_hash_table_lookup (a->value.dict, "realpath");

		if (!b) {
			xmms_log_error ("Playlist plugin did not set realpath; probably a bug in plugin");
			xmms_object_cmd_value_unref (a);
			n = g_list_delete_link (n, n);
			continue;
		}

		entry = xmms_medialib_entry_new_encoded (session,
		                                         b->value.string,
		                                         err);
		g_hash_table_remove (a->value.dict, "realpath");
		g_hash_table_remove (a->value.dict, "path");

		if (entry) {
			add_metadata_from_hash_user_data_t udata;
			udata.session = session;
			udata.entry = entry;
			udata.src = src;

			g_hash_table_foreach (a->value.dict, add_metadata_from_hash, &udata);

			xmmsc_coll_idlist_append (coll, entry);
		} else {
			xmms_log_error ("couldn't add %s to collection!", b->value.string);
		}

		xmms_object_cmd_value_unref (a);
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
gboolean
xmms_collection_remove (xmms_coll_dag_t *dag, gchar *name, gchar *namespace, xmms_error_t *err)
{
	guint nsid;
	gboolean retval = FALSE;
	guint i;

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
	} else {
		retval = xmms_collection_unreference (dag, name, nsid);
	}

	g_mutex_unlock (dag->mutex);

	if (retval == FALSE) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Failed to remove this collection!");
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
	const gchar *alias;
	gchar *newkey = NULL;

	nsid = xmms_collection_get_namespace_id (namespace);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		return FALSE;
	} else if (nsid == XMMS_COLLECTION_NSID_ALL) {
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
			xmms_collection_dag_replace (dag, nsid, newkey, coll);
			xmmsc_coll_ref (coll);

			XMMS_COLLECTION_CHANGED_MSG (XMMS_COLLECTION_CHANGED_UPDATE,
			                             newkey,
			                             namespace);
		}

	/* Save new collection in the table */
	} else {
		newkey = g_strdup (name);
		xmms_collection_dag_replace (dag, nsid, newkey, coll);
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
		xmmsc_coll_ref (coll);
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
xmms_collection_sync (xmms_coll_dag_t *dag, xmms_error_t *err)
{
	g_return_if_fail (dag);

	g_mutex_lock (dag->mutex);

	xmms_collection_dag_save (dag);

	g_mutex_unlock (dag->mutex);
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
xmms_collection_list (xmms_coll_dag_t *dag, gchar *namespace, xmms_error_t *err)
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
	} else if (nsid == XMMS_COLLECTION_NSID_ALL) {
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

	} else if (to_coll != NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "a collection already exists with the target name");
		retval = FALSE;

	/* Update collection name everywhere */
	} else {
		GHashTable *dict;

		/* insert new pair in hashtable */
		xmms_collection_dag_replace (dag, nsid, g_strdup (to_name), from_coll);
		xmmsc_coll_ref (from_coll);

		/* remove old pair from hashtable */
		g_hash_table_remove (dag->collrefs[nsid], from_name);

		/* update name in all reference operators */
		coll_rename_infos_t infos = { from_name, to_name, namespace };
		xmms_collection_apply_to_all_collections (dag, rename_references, &infos);

		/* Send _RENAME signal */
		dict = xmms_collection_changed_msg_new (XMMS_COLLECTION_CHANGED_RENAME,
		                                        from_name, namespace);
		g_hash_table_insert (dict, (gpointer) "newname",
		                     xmms_object_cmd_value_str_new (to_name));
		xmms_collection_changed_msg_send (dag, dict);

		retval = TRUE;
	}

	g_mutex_unlock (dag->mutex);

	return retval;
}


/** Find the ids of the media matched by a collection.
 *
 * @param dag  The collection DAG.
 * @param coll  The collection used to match media.
 * @param lim_start  The beginning index of the LIMIT statement (0 to disable).
 * @param lim_len  The number of entries of the LIMIT statement (0 to disable).
 * @param order  The list of properties to order by (NULL to disable).
 * @param err  If an error occurs, a message is stored in it.
 * @return A list of media ids.
 */
GList *
xmms_collection_query_ids (xmms_coll_dag_t *dag, xmmsc_coll_t *coll,
                           guint lim_start, guint lim_len, GList *order,
                           xmms_error_t *err)
{
	GList *res = NULL;
	GList *ids = NULL;
	GList *fetch = g_list_prepend (NULL, (gpointer) "id");
	GList *n = NULL;

	res = xmms_collection_query_infos (dag, coll, lim_start, lim_len, order, fetch, NULL, err);

	/* FIXME: get an int list directly ! */
	for (n = res; n; n = n->next) {
		xmms_object_cmd_value_t *buf;
		xmms_object_cmd_value_t *cmdval = (xmms_object_cmd_value_t*)n->data;
		buf = g_hash_table_lookup (cmdval->value.dict, "id");
		ids = g_list_prepend (ids, xmms_object_cmd_value_uint_new (buf->value.int32));
		xmms_object_cmd_value_unref (n->data);
	}

	g_list_free (res);

	return g_list_reverse (ids);
}


/** Find the properties of the media matched by a collection.
 *
 * @param dag  The collection DAG.
 * @param coll  The collection used to match media.
 * @param lim_start  The beginning index of the LIMIT statement (0 to disable).
 * @param lim_len  The number of entries of the LIMIT statement (0 to disable).
 * @param order  The list of properties to order by, prefix by '-' to invert (NULL to disable).
 * @param fetch  The list of properties to be retrieved (NULL to only retrieve id).
 * @param group  The list of properties to group by (NULL to disable).
 * @param err  If an error occurs, a message is stored in it.
 * @return A list of property dicts for each entry.
 */
GList *
xmms_collection_query_infos (xmms_coll_dag_t *dag, xmmsc_coll_t *coll,
                             guint lim_start, guint lim_len, GList *order,
                             GList *fetch, GList *group, xmms_error_t *err)
{
	GList *res = NULL;
	GString *query;

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
                                guint nsid, xmmsc_coll_t *newtarget)
{
	xmms_collection_dag_replace (dag, nsid, g_strdup (name), newtarget);
	xmmsc_coll_ref (newtarget);
}

/** Update the DAG to update the value of the pair with the given key. */
void
xmms_collection_dag_replace (xmms_coll_dag_t *dag,
                             xmms_collection_namespace_id_t nsid,
                             gchar *key, xmmsc_coll_t *newcoll)
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
xmmsc_coll_t *
xmms_collection_get_pointer (xmms_coll_dag_t *dag, const gchar *collname,
                             guint nsid)
{
	gint i;
	xmmsc_coll_t *coll = NULL;

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
xmms_collection_get_int_attr (xmmsc_coll_t *coll, const gchar *attrname, gint *val)
{
	gboolean retval = FALSE;
	gint buf;
	gchar *str;
	gchar *endptr;

	if (xmmsc_coll_attribute_get (coll, attrname, &str)) {
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
xmms_collection_set_int_attr (xmmsc_coll_t *coll, const gchar *attrname,
                              gint newval)
{
	gboolean retval = FALSE;
	gchar str[XMMS_MAX_INT_ATTRIBUTE_LEN + 1];
	gint written;

	written = g_snprintf (str, sizeof (str), "%d", newval);
	if (written < XMMS_MAX_INT_ATTRIBUTE_LEN) {
		xmmsc_coll_attribute_set (coll, attrname, str);
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
                            xmmsc_coll_t *value, const gchar *key)
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
xmms_collection_get_random_media (xmms_coll_dag_t *dag, xmmsc_coll_t *source)
{
	GList *res;
	GList *rorder = NULL;
	xmms_medialib_entry_t mid = 0;

	/* FIXME: Temporary hack to allow custom ordering functions */
	rorder = g_list_prepend (rorder, (gpointer) "~RANDOM()");

	res = xmms_collection_query_ids (dag, source, 0, 1, rorder, NULL);

	g_list_free (rorder);

	if (res != NULL) {
		xmms_object_cmd_value_t *cmdval = (xmms_object_cmd_value_t*)res->data;
		mid = cmdval->value.int32;
		xmms_object_cmd_value_unref (res->data);
		g_list_free (res);
	}

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

	xmms_collection_dag_save (dag);

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
 * @param save_name  The name under which the collection will be saved (NULL
 *                   if none).
 * @param save_namespace  The namespace in which the collection will be
 *                        saved (NULL if none).
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
	xmmsc_coll_t *op, *ref;
	gchar *attr, *attr2;
	gboolean valid = TRUE;
	xmmsc_coll_type_t type;
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
	type = xmmsc_coll_get_type (coll);
	switch (type) {
	case XMMS_COLLECTION_TYPE_REFERENCE:
		/* zero or one (bound in DAG) operand */
		if (num_operands > 1) {
			return FALSE;
		}

		/* check if referenced collection exists */
		xmmsc_coll_attribute_get (coll, "reference", &attr);
		if (attr == NULL) {
			return FALSE;
		} else if (strcmp (attr, "All Media") != 0) {
			xmmsc_coll_attribute_get (coll, "namespace", &attr2);

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
			xmmsc_coll_operand_list_save (coll);
			xmmsc_coll_operand_list_first (coll);
			xmmsc_coll_operand_list_entry (coll, &op);
			xmmsc_coll_operand_list_restore (coll);

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

	case XMMS_COLLECTION_TYPE_HAS:
		/* one operand */
		if (num_operands != 1) {
			return FALSE;
		}

		/* "field" attribute */
		/* with valid value */
		if (!xmmsc_coll_attribute_get (coll, "field", &attr)) {
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_EQUALS:
	case XMMS_COLLECTION_TYPE_MATCH:
	case XMMS_COLLECTION_TYPE_SMALLER:
	case XMMS_COLLECTION_TYPE_GREATER:
		/* one operand */
		if (num_operands != 1) {
			return FALSE;
		}

		/* "field"/"value" attributes */
		/* with valid values */
		if (!xmmsc_coll_attribute_get (coll, "field", &attr)) {
			return FALSE;
		}
		/* FIXME: valid fields?
		else if (...) {
			return FALSE;
		}
		*/

		if (!xmmsc_coll_attribute_get (coll, "value", &attr)) {
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_IDLIST:
	case XMMS_COLLECTION_TYPE_QUEUE:
		/* no operand */
		if (num_operands > 0) {
			return FALSE;
		}
		break;

	case XMMS_COLLECTION_TYPE_PARTYSHUFFLE:
		/* one operand */
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
		xmmsc_coll_operand_list_save (coll);

		xmmsc_coll_operand_list_first (coll);
		while (xmmsc_coll_operand_list_entry (coll, &op) && valid) {
			if (!xmms_collection_validate_recurs (dag, op, save_name,
			                                      save_namespace)) {
				valid = FALSE;
			}
			xmmsc_coll_operand_list_next (coll);
		}

		xmmsc_coll_operand_list_restore (coll);
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
xmms_collection_unreference (xmms_coll_dag_t *dag, gchar *name, guint nsid)
{
	xmmsc_coll_t *existing, *active_pl;
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
xmms_collection_get_namespace_id (gchar *namespace)
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

	/* Apply the function to the operator. */
	f (dag, coll, parent, udata);

	/* Recurse into the parents (if not a reference) */
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
	val = xmms_object_cmd_value_str_new (key);
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
void
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
		xmmsc_coll_add_operand (coll, infos->newtarget);
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
	xmmsc_coll_t *op;
	coll_rebind_infos_t *infos;
	gchar *target_name = NULL;
	gchar *target_namespace = NULL;

	infos = (coll_rebind_infos_t*)udata;

	xmmsc_coll_operand_list_save (coll);
	xmmsc_coll_operand_list_first (coll);
	while (xmmsc_coll_operand_list_entry (coll, &op)) {
		/* Skip if not potential reference */
		if (xmmsc_coll_get_type (op) != XMMS_COLLECTION_TYPE_REFERENCE) {
			xmmsc_coll_operand_list_next (coll);
			continue;
		}

		xmmsc_coll_attribute_get (op, "reference", &target_name);
		xmmsc_coll_attribute_get (op, "namespace", &target_namespace);
		if (strcmp (infos->name, target_name) != 0 ||
		    strcmp (infos->namespace, target_namespace) != 0) {
			xmmsc_coll_operand_list_next (coll);
			continue;
		}

		/* Rebind coll to ref'd operand directly, effectively strip reference */
		xmmsc_coll_remove_operand (op, infos->oldtarget);

		xmmsc_coll_remove_operand (coll, op);
		xmmsc_coll_add_operand (coll, infos->oldtarget);

		xmmsc_coll_operand_list_first (coll); /* Restart if oplist changed */
	}
	xmmsc_coll_operand_list_restore (coll);
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
		} else {
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
		*list = g_list_prepend (*list, xmms_object_cmd_value_str_new (coll_name));
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
		if (xmmsc_coll_attribute_get (coll, "reference", &attr1)) {
			if (strcmp (attr1, "All Media") == 0) {
				match = TRUE;
			} else if (xmmsc_coll_attribute_get (coll, "namespace", &attr2)) {
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
			match = xmms_collection_media_match (dag, mediainfo, op,
			                                     nsid, match_table);
			xmmsc_coll_operand_list_next (coll);
		}
		xmmsc_coll_operand_list_restore (coll);
		break;

	case XMMS_COLLECTION_TYPE_INTERSECTION:
		/* if ALL match */
		match = TRUE;
		xmmsc_coll_operand_list_save (coll);
		xmmsc_coll_operand_list_first (coll);
		while (match && xmmsc_coll_operand_list_entry (coll, &op)) {
			match = xmms_collection_media_match (dag, mediainfo, op,
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

	case XMMS_COLLECTION_TYPE_EQUALS:
		match = xmms_collection_media_filter_equals (dag, mediainfo, coll,
		                                            nsid, match_table);
		break;

	case XMMS_COLLECTION_TYPE_MATCH:
		match = xmms_collection_media_filter_match (dag, mediainfo, coll,
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

/** Get all the properties for the given media.
 *
 * @param mid  The id of the media.
 * @return  A HashTable with all the properties.
 */
static GHashTable *
xmms_collection_media_info (guint mid, xmms_error_t *err)
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
	                               g_free, (GDestroyNotify)xmms_object_cmd_value_unref);
	for (state = 0, n = res; n; state = (state + 1) % 3, n = n->next) {
		switch (state) {
		case 0:  /* source */
			break;

		case 1:  /* prop name */
			cmdval = n->data;
			name = g_strdup (cmdval->value.string);
			break;

		case 2:  /* prop value */
			value = xmms_object_cmd_value_ref (n->data);

			/* Only insert the first source */
			if (g_hash_table_lookup (infos, name) == NULL) {
				g_hash_table_replace (infos, name, value);
			}
			break;
		}

		xmms_object_cmd_value_unref (n->data);
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
filter_get_mediainfo_field_string (xmmsc_coll_t *coll, GHashTable *mediainfo, gchar **val)
{
	gboolean retval = FALSE;
	gchar *attr;
	xmms_object_cmd_value_t *cmdval;

	if (xmmsc_coll_attribute_get (coll, "field", &attr)) {
		cmdval = g_hash_table_lookup (mediainfo, attr);
		if (cmdval != NULL) {
			switch (cmdval->type) {
			case XMMS_OBJECT_CMD_ARG_STRING:
				*val = g_strdup (cmdval->value.string);
				retval = TRUE;
				break;

			case XMMS_OBJECT_CMD_ARG_UINT32:
				*val = g_strdup_printf ("%u", cmdval->value.uint32);
				retval = TRUE;
				break;

			case XMMS_OBJECT_CMD_ARG_INT32:
				*val = g_strdup_printf ("%d", cmdval->value.int32);
				retval = TRUE;
				break;

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
filter_get_mediainfo_field_int (xmmsc_coll_t *coll, GHashTable *mediainfo, gint *val)
{
	gboolean retval = FALSE;
	gchar *attr;
	xmms_object_cmd_value_t *cmdval;

	if (xmmsc_coll_attribute_get (coll, "field", &attr)) {
		cmdval = g_hash_table_lookup (mediainfo, attr);
		if (cmdval != NULL && cmdval->type == XMMS_OBJECT_CMD_ARG_INT32) {
			*val = cmdval->value.int32;
			retval = TRUE;
		}
	}

	return retval;
}

/* Get the string value of the "value" attribute of the collection. */
static gboolean
filter_get_operator_value_string (xmmsc_coll_t *coll, gchar **val)
{
	gchar *attr;
	gboolean valid;

	valid = xmmsc_coll_attribute_get (coll, "value", &attr);
	if (valid) {
		*val = attr;
	}

	return valid;
}

/* Get the integer value of the "value" attribute of the collection. */
static gboolean
filter_get_operator_value_int (xmmsc_coll_t *coll, gint *val)
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
filter_get_operator_case (xmmsc_coll_t *coll, gboolean *val)
{
	gchar *attr;

	if (xmmsc_coll_attribute_get (coll, "case-sensitive", &attr)) {
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
                                  xmmsc_coll_t *coll, guint nsid,
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
                                    xmmsc_coll_t *coll, guint nsid,
                                    GHashTable *match_table)
{
	gboolean match = FALSE;
	gchar *mediaval = NULL;
	gchar *opval;
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
                                       xmmsc_coll_t *coll, guint nsid,
                                       GHashTable *match_table)
{
	gboolean match = FALSE;
	gchar *buf;
	gchar *mediaval;
	gchar *opval;
	gboolean case_sens;

	if (filter_get_mediainfo_field_string (coll, mediainfo, &buf) &&
	    filter_get_operator_value_string (coll, &opval) &&
	    filter_get_operator_case (coll, &case_sens)) {

		/* Prepare values */
		if (case_sens) {
			mediaval = buf;
		} else {
			opval = g_utf8_strdown (opval, -1);
			mediaval = g_utf8_strdown (buf, -1);
			g_free (buf);
		}

		match = g_pattern_match_simple (opval, mediaval);

		if (!case_sens) {
			g_free (opval);
		}
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
                                      xmmsc_coll_t *coll, guint nsid,
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
                                      xmmsc_coll_t *coll, guint nsid,
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
