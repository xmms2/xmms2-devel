/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include "xmms/util.h"
#include "xmms/playlist.h"
#include "xmms/config.h"
#include "xmms/medialib.h"
#include "xmms/transport.h"
#include "xmms/decoder.h"
#include "xmms/plugin.h"
#include "xmms/signal_xmms.h"
#include "xmms/ipc.h"
#include "internal/plugin_int.h"
#include "internal/decoder_int.h"
#include "internal/output_int.h"
#include "internal/transport_int.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <time.h>

#include <sqlite.h>

struct xmms_medialib_St {
	xmms_object_t object;
	struct sqlite *sql;
	GMutex *mutex;
	guint32 nextid;
};


/** 
  * Ok, so the functions are written with reentrency in mind, but
  * we choose to have a global medialib object here. It will be
  * much easier, and I don't see the real use of multiple medialibs
  * right now. This could be changed by removing this global one
  * and altering the function callers...
  */

static xmms_medialib_t *medialib;

static GHashTable *xmms_medialib_info (xmms_medialib_t *playlist, guint32 id, xmms_error_t *err);
/** Methods */
XMMS_CMD_DEFINE (info, xmms_medialib_info, xmms_medialib_t *, HASHTABLE, UINT32, NONE);

static void 
xmms_medialib_destroy (xmms_object_t *object)
{
	xmms_medialib_t *mlib = (xmms_medialib_t *)object;

	g_mutex_free (mlib->mutex);
	xmms_sqlite_close (mlib->sql);
}
 
gboolean
xmms_medialib_init (xmms_playlist_t *playlist)
{
	medialib = xmms_object_new (xmms_medialib_t, xmms_medialib_destroy);
	medialib->sql = xmms_sqlite_open (&medialib->nextid);
	medialib->mutex = g_mutex_new ();

	xmms_ipc_object_register (XMMS_IPC_OBJECT_MEDIALIB, XMMS_OBJECT (medialib));
	xmms_ipc_broadcast_register (XMMS_OBJECT (medialib), XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);
	xmms_object_cmd_add (XMMS_OBJECT (medialib), 
			     XMMS_IPC_CMD_INFO, 
			     XMMS_CMD_FUNC (info));
	
	return TRUE;
}

static int
xmms_medialib_int_cb (void *pArg, int argc, char **argv, char **columnName) 
{
	guint *i = pArg;

	if (argv[0])
		*i = atoi (argv[0]);

	return 0;
}



static int
xmms_medialib_string_cb (void *pArg, int argc, char **argv, char **columnName) 
{
	gchar **str = pArg;

	if (argv[0])
		*str = g_strdup (argv[0]);

	return 0;
}


gchar *
xmms_medialib_entry_property_get (xmms_medialib_entry_t entry, const gchar *property)
{
	gchar *ret = NULL;

	g_return_val_if_fail (property, NULL);

	g_mutex_lock (medialib->mutex);
	xmms_sqlite_query (medialib->sql, xmms_medialib_string_cb, &ret,
			   "select value from Media where key=%Q and id=%d", property, entry);
	g_mutex_unlock (medialib->mutex);

	return ret;
}

guint
xmms_medialib_entry_property_get_int (xmms_medialib_entry_t entry, const gchar *property)
{
	gchar *tmp = NULL;
	guint ret;

	tmp = xmms_medialib_entry_property_get (entry, property);
	if (tmp) {
		ret = atoi (tmp);
		g_free (tmp);
	} else {
		ret = 0;
	}
	return ret;
}

gboolean
xmms_medialib_entry_property_set (xmms_medialib_entry_t entry, const gchar *property, const gchar *value)
{
	gboolean ret;
	g_return_val_if_fail (property, FALSE);

	g_mutex_lock (medialib->mutex);
	ret = xmms_sqlite_query (medialib->sql, NULL, NULL,
				 "insert or replace into Media (id, value, key) values (%d, %Q, LOWER(%Q))", entry, value, property);
	g_mutex_unlock (medialib->mutex);

	return ret;

}

gboolean
xmms_medialib_entry_is_resolved (xmms_medialib_entry_t entry)
{
	return xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED);
}

/** Just a convient function wrapper */
guint
xmms_medialib_entry_id_get (xmms_medialib_entry_t entry)
{
	return xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_ID);
}

void
xmms_medialib_entry_send_update (xmms_medialib_entry_t entry)
{
	g_mutex_lock (medialib->mutex);
	xmms_object_emit_f (XMMS_OBJECT (medialib), XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE, 
			    XMMS_OBJECT_CMD_ARG_UINT32, entry);
	g_mutex_unlock (medialib->mutex);
}

xmms_medialib_entry_t
xmms_medialib_entry_new (const char *url)
{
	guint id = 0;
	guint ret;

	g_return_val_if_fail (url, 0);
	g_mutex_lock (medialib->mutex);

	if (g_strncasecmp (url, "mlib://", 7) == 0) {
		const gchar *p = url+9;
		id = strtol (p, NULL, 10);
		/* Hmmm, maybe verify that this entry exists? */
	} else {
		xmms_sqlite_query (medialib->sql, xmms_medialib_int_cb, &id, 
			     	"select id from Media where key='url' and value=%Q", url);
	}

	if (id) {
		ret = id;
	} else {
		ret = ++medialib->nextid;
		if (!xmms_sqlite_query (medialib->sql, NULL, NULL,
				       "insert into Media (id, key, value) values (%d, 'url', %Q)",
				       ret, url)) {
			ret = 0;
		}
		if (!xmms_sqlite_query (medialib->sql, NULL, NULL,
				       "insert or replace into Media (id, key, value) values (%d, 'resolved', '0')",
				       ret)) {
			ret = 0;
		}

	}
	g_mutex_unlock (medialib->mutex);

	return ret;

}

static int
xmms_medialib_hashtable_cb (void *pArg, int argc, char **argv, char **columnName) 
{
	GHashTable *hash = pArg;

	g_hash_table_insert (hash, g_strdup (argv[1]), g_strdup (argv[2]));

	return 0;
}

GHashTable *
xmms_medialib_entry_to_hashtable (xmms_medialib_entry_t entry)
{
	GHashTable *ret;
	
	ret = g_hash_table_new (g_str_hash, g_str_equal);

	g_mutex_lock (medialib->mutex);

	xmms_sqlite_query (medialib->sql, xmms_medialib_hashtable_cb, ret, 
			   "select * from Media where id=%d", entry);

	g_mutex_unlock (medialib->mutex);

	return ret;
}


static GHashTable *
xmms_medialib_info (xmms_medialib_t *playlist, guint32 id, xmms_error_t *err)
{
	GHashTable *ret = xmms_medialib_entry_to_hashtable (id);
	if (!ret) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Could not retrive info for that entry!");
		return NULL;
	}
	return ret;
}

/*
GList *
xmms_medialib_query (const char *query)
{
}

gboolean
xmms_medialib_playlist_save (const char *name)
{
}

gboolean
xmms_medialib_playlist_load (const char *name)
{
}
*/
