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

static void xmms_collection_destroy (xmms_object_t *object);

static xmms_collection_namespace_id_t xmms_collection_get_namespace (xmms_coll_dag_t *dag, gchar *namespace, xmms_error_t *err);
static xmmsc_coll_t * xmms_collection_get_pointer (xmms_coll_dag_t *dag, gchar *collname, guint namespace);

static void free_value_coll (gpointer key, gpointer value, gpointer udata);
static void prepend_key_string (gpointer key, gpointer value, gpointer udata);


XMMS_CMD_DEFINE (collection_get, xmms_collection_get, xmms_coll_dag_t *, COLL, STRING, STRING);
XMMS_CMD_DEFINE (collection_list, xmms_collection_list, xmms_coll_dag_t *, LIST, STRING, NONE);
XMMS_CMD_DEFINE3(collection_save, xmms_collection_save, xmms_coll_dag_t *, NONE, STRING, STRING, COLL);
XMMS_CMD_DEFINE (collection_remove, xmms_collection_remove, xmms_coll_dag_t *, NONE, STRING, STRING);
XMMS_CMD_DEFINE (collection_find, xmms_collection_find, xmms_coll_dag_t *, LIST, UINT32, STRING);

/* FIXME: Arrays, num args, etc?
XMMS_CMD_DEFINE (query_ids, xmms_collection_query_ids, xmms_coll_dag_t *, LIST, COLL, );
XMMS_CMD_DEFINE (query_infos, xmms_collection_query_infos, xmms_coll_dag_t *, LIST, ...);
*/


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


/**
 * Initializes a new xmms_coll_dag_t.
 */
xmms_coll_dag_t *
xmms_collection_init (void)
{
	gint i;
	xmms_coll_dag_t *ret;
	
	ret = xmms_object_new (xmms_coll_dag_t, xmms_collection_destroy);
	ret->mutex = g_mutex_new ();

	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		ret->collrefs[i] = g_hash_table_new (g_str_hash, g_str_equal);
	}

	xmms_ipc_object_register (XMMS_IPC_OBJECT_COLLECTION, XMMS_OBJECT (ret));

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

/*
	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_QUERY_IDS, 
			     XMMS_CMD_FUNC (query_ids));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_QUERY_INFOS, 
			     XMMS_CMD_FUNC (query_infos));
*/
	return ret;
}





static xmms_collection_namespace_id_t
xmms_collection_get_namespace (xmms_coll_dag_t *dag, gchar *namespace, xmms_error_t *err)
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
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid collection namespace");
		nsid = XMMS_COLLECTION_NSID_INVALID;
	}

	return nsid;
}

static xmmsc_coll_t *
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


gboolean
xmms_collection_remove (xmms_coll_dag_t *dag, gchar *name, gchar *namespace, xmms_error_t *err)
{
	xmmsc_coll_t *existing = NULL;
	guint32 nsid;
	gboolean retval;
	gint i;

	XMMS_DBG("COLLECTIONS: Entering xmms_collection_remove");

	nsid = xmms_collection_get_namespace (dag, namespace, err);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		return FALSE;
	}

	g_mutex_lock (dag->mutex);

	/* FIXME: What if referenced by others? free coll and key (string) ! */

	/* FIXME: cleaner modularisation */
	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES && existing == NULL; ++i) {
			existing = g_hash_table_lookup (dag->collrefs[i], name);
			if (existing != NULL) {
				g_hash_table_remove (dag->collrefs[i], name);
			}
		}
	}
	else {
		existing = g_hash_table_lookup (dag->collrefs[nsid], name);
		if (existing != NULL) {
			g_hash_table_remove (dag->collrefs[nsid], name);
		}
	}

	if (existing == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "No such collection!");
		retval = FALSE;
	}
	else {
		xmmsc_coll_unref (existing);
		retval = TRUE;
	}
/*
	existing = xmms_collection_get_pointer (dag, name, nsid);
	if (existing != NULL) {
		xmmsc_coll_unref (existing);
		retval = TRUE;
	}
	else {
		xmms_error_set (err, XMMS_ERROR_NOENT, "No such collection!");
		retval = FALSE;
	}
*/

	g_mutex_unlock (dag->mutex);

	return retval;
}


gboolean
xmms_collection_save (xmms_coll_dag_t *dag, gchar *name, gchar *namespace, xmmsc_coll_t *coll, xmms_error_t *err)
{
	xmmsc_coll_t *existing;
	guint32 nsid;

	XMMS_DBG("COLLECTIONS: Entering xmms_collection_save");

	nsid = xmms_collection_get_namespace (dag, namespace, err);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		return FALSE;
	}

	g_mutex_lock (dag->mutex);


	/* FIXME: What if referenced by others? -- see _remove */

	/* Unreference previously saved collection */
	existing = xmms_collection_get_pointer (dag, name, nsid);
	if (existing != NULL) {
		xmmsc_coll_unref (existing);
	}

	XMMS_DBG("COLLECTIONS: xmms_collection_save, done checking for removal, save %s", name);

	/* Save new collection */
	g_hash_table_insert (dag->collrefs[nsid], g_strdup(name), coll);
	xmmsc_coll_ref (coll);

	g_mutex_unlock (dag->mutex);

	XMMS_DBG("COLLECTIONS: xmms_collection_save, end");

	return TRUE;
}


xmmsc_coll_t *
xmms_collection_get (xmms_coll_dag_t *dag, gchar *collname, gchar *namespace, xmms_error_t *err)
{
	xmmsc_coll_t *coll = NULL;
	guint32 nsid;

	nsid = xmms_collection_get_namespace (dag, namespace, err);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		return NULL;
	}

	g_mutex_lock (dag->mutex);

	coll = xmms_collection_get_pointer (dag, collname, nsid);

	/* New reference, will be freed after being put in the return message */
	xmmsc_coll_ref (coll);
	
	g_mutex_unlock (dag->mutex);

	return coll;
}


/** Lists the collections in the given namespace.
 *
 * @returns A newly allocated GList with the list of collection names.
 * Remeber that it is only the LIST that is copied. Not the entries.
 * The entries are however referenced, and must be unreffed!
 */
GList *
xmms_collection_list (xmms_coll_dag_t *dag, gchar *namespace, xmms_error_t *err)
{
	GList *r = NULL;
	guint32 nsid;
	gint i;

	XMMS_DBG("COLLECTIONS: Entering xmms_collection_list");

	nsid = xmms_collection_get_namespace (dag, namespace, err);
	if (nsid == XMMS_COLLECTION_NSID_INVALID) {
		return NULL;
	}

	g_mutex_lock (dag->mutex);

	/* Get the list of collections in the given namespace */
	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
			g_hash_table_foreach (dag->collrefs[i], prepend_key_string, &r);
		}
	}
	else if (nsid != XMMS_COLLECTION_NSID_INVALID) {
		XMMS_DBG("COLLECTIONS: xmms_collection_list, in NS=%d", nsid);
		g_hash_table_foreach (dag->collrefs[nsid], prepend_key_string, &r);
	}

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
		g_hash_table_foreach (dag->collrefs[i], free_value_coll, NULL);
		g_hash_table_destroy (dag->collrefs[i]);
	}

	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_COLLECTION);
}


static void
free_value_coll (gpointer key, gpointer value, gpointer udata)
{
	/* FIXME: free key too ? */
	xmmsc_coll_t *coll = (xmmsc_coll_t*)value;
	xmmsc_coll_unref (coll);
}

static void
prepend_key_string (gpointer key, gpointer value, gpointer udata)
{
	xmms_object_cmd_value_t *val;
	GList **list = (GList**)udata;
	val = xmms_object_cmd_value_str_new (key);
	*list = g_list_prepend (*list, val);

	/* FIXME: duplicate strings ? */
}
