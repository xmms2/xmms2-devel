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




/** @file
 *  This controls the medialib.
 */

#include "xmms/util.h"
#include "xmms/playlist.h"
#include "xmms/dbus.h"
#include "xmms/config.h"
#include "xmms/medialib.h"
#include "xmms/transport.h"
#include "xmms/decoder.h"
#include "xmms/plugin.h"
#include "xmms/signal_xmms.h"
#include "internal/plugin_int.h"
#include "internal/decoder_int.h"
#include "internal/output_int.h"
#include "internal/transport_int.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <time.h>

#ifdef HAVE_SQLITE
#include <sqlite.h>
#endif

/** @defgroup MediaLibrary MediaLibrary
  * @ingroup XMMSServer
  * @brief The library.
  *
  * The medialibrary is responsible for holding information about your whole
  * music collection. 
  * @{
  */

struct xmms_medialib_St {
	xmms_object_t object;

	GMutex *mutex;
	guint id;

#ifdef HAVE_SQLITE
	sqlite *sql;
#endif
};


/** Global medialib */
static xmms_medialib_t *medialib;


static void
xmms_medialib_destroy (xmms_object_t *medialib)
{
	xmms_medialib_t *m = (xmms_medialib_t*) medialib;

	g_mutex_free (m->mutex);

	if (m->sql) 
		xmms_medialib_close (m);

	g_free (m);
}

/** Initialize the medialib */
gboolean
xmms_medialib_init ()
{

	medialib = xmms_object_new (xmms_medialib_t, xmms_medialib_destroy);


#ifdef HAVE_SQLITE
	if (!xmms_sqlite_open ()) {
		xmms_object_unref (medialib);
		medialib = NULL;
		return FALSE;
	}
#endif /*HAVE_SQLITE*/
	medialib->mutex = g_mutex_new ();

	xmms_config_value_register ("medialib.dologging",
				    "1",
				    NULL, NULL);

	return TRUE;
}

static void
statuschange (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
        gchar *mid;
        gboolean ret;
	xmms_playlist_entry_t *entry;
	xmms_config_value_t *cv;
	guint32 status = ((xmms_object_method_arg_t *) data)->retval.uint32;

	cv = xmms_config_lookup ("medialib.dologging");
	g_return_if_fail (cv);
	
	if (!xmms_config_value_int_get (cv))
		return;

	entry = xmms_output_playing_entry_get ((xmms_output_t *)object, NULL);
	if (!entry)
		return;

	mid = xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID);
	if (!mid) 
		return;

	if (status == XMMS_OUTPUT_STATUS_STOP) {
		char *tmp;
		gint value = 0.0;
		gchar *sek;

		tmp = xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION);
		if (tmp) {
			value = (gint) (100.0 * xmms_output_playtime ((xmms_output_t *)object, NULL) / (gdouble)atoi (tmp));
		}
		
		XMMS_DBG ("ENTRY STOPPED value = %d", value);
		sek = xmms_playlist_entry_property_get (entry, "laststarted");
		g_return_if_fail (sek);

		XMMS_MTX_LOCK (medialib->mutex);
		ret = xmms_sqlite_query (NULL, NULL, 
					 "update Log set value=%d where id=%s and starttime=%s", 
					 value, mid, sek);
		
		XMMS_MTX_UNLOCK (medialib->mutex);
	} else if (status == XMMS_OUTPUT_STATUS_PLAY) {
		char tmp[16];
		time_t stime = time (NULL);
		
		XMMS_MTX_LOCK (medialib->mutex);
		ret = xmms_sqlite_query (NULL, NULL, 
					 "insert into Log (id, starttime) values (%s, %u)", 
					 mid, (guint)stime);

		XMMS_MTX_UNLOCK (medialib->mutex);
		if (!ret) {
			return;
		}

		snprintf (tmp, 16, "%u", (guint)stime);
		xmms_playlist_entry_property_set (entry, "laststarted", tmp);
	}
}


void
xmms_medialib_output_register (xmms_output_t *output)
{
	xmms_object_connect (XMMS_OBJECT (output),
			     XMMS_SIGNAL_OUTPUT_STATUS,
			     (xmms_object_handler_t) statuschange, NULL);
}


static guint
xmms_medialib_next_id (xmms_medialib_t *medialib)
{
	guint id=0;
	g_return_val_if_fail (medialib, 0);

	id = medialib->id ++;

	return id;
}

static gchar *_e[] = { 	
	"id", "url",
	XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST,
	XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM,
	XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE,
	XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE,
	XMMS_PLAYLIST_ENTRY_PROPERTY_LMOD,
	NULL
};

static void
insert_foreach (gpointer key, gpointer value, gpointer userdata)
{
	gchar *k = key;
	gchar *v = value;
	xmms_medialib_t *medialib = userdata;
	gint i = 0;

	while (_e[i]) {
		if (g_strcasecmp (_e[i], k) == 0) {
		    return;
		}
		i++;
	}

	xmms_sqlite_query (NULL, NULL, "insert into Property values (%d, '%q', '%q')", medialib->id - 1, k, v);
}

/** 
  * Takes an #xmms_playlist_entry_t and stores it
  * to the library
  *
  * @todo what if the entry already exists?
  */

gboolean
xmms_medialib_entry_store (xmms_playlist_entry_t *entry)
{
	gboolean ret;
	guint id;
	gchar *tmp;

	g_return_val_if_fail (medialib, FALSE);
	XMMS_DBG ("Storing entry to medialib!");
	XMMS_MTX_LOCK (medialib->mutex);

	id = xmms_medialib_next_id (medialib);
	ret = xmms_sqlite_query (NULL, NULL,
				 "insert into Media values (%d, '%q', '%q', '%q', '%q', '%q', '%q')",
				 id, xmms_playlist_entry_url_get (entry),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_LMOD));

	if (!ret) {
		return FALSE;
	}

	xmms_playlist_entry_property_foreach (entry, insert_foreach, medialib);

	tmp = g_strdup_printf ("%d", id);
	xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID, tmp);
	g_free (tmp);
				 
	
	XMMS_MTX_UNLOCK (medialib->mutex);

	return TRUE;
}

static int
select_callback (void *pArg, int argc, char **argv, char **cName)
{
	gint i=0;
	GList **l = (GList **) pArg;

	while (cName[i]) {
		if (g_strcasecmp (cName[i], "url") == 0) {
			xmms_playlist_entry_t *e;
			e = xmms_playlist_entry_new (argv[i]);
			xmms_medialib_entry_get (e);
			*l = g_list_prepend (*l, e);
		}

		i++;
	}

	return 0;
}

/**
 * Get a list of #xmms_playlist_entry_t 's that matches the query.
 * To use this function you'll need to select the URL column from the
 * database.
 *
 * Make sure that query are correctly escaped before entering here!
 */
GList *
xmms_medialib_select (gchar *query, xmms_error_t *error)
{
	GList *res = NULL;
	gint ret;

	g_return_val_if_fail (query, 0);

	ret = xmms_sqlite_query (select_callback, (void *)&res, query);

	if (!ret)
		return NULL;

	res = g_list_reverse (res);
	return res;

}

static int
mediarow_callback (void *pArg, int argc, char **argv, char **columnName) 
{
	gint i;
	xmms_playlist_entry_t *entry = pArg;

	for (i = 0; i < argc; i ++) {
		XMMS_DBG ("Setting %s to %s", columnName[i], argv[i]);
		if (g_strcasecmp (columnName[i], "id") == 0) {
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID, argv[i]);
		} else if (g_strcasecmp (columnName[i], "url") == 0) {
			continue;
		} else {
			xmms_playlist_entry_property_set (entry, columnName[i], argv[i]);
		}
	}

	return 0;
	
}

static int
proprow_callback (void *pArg, int argc, char **argv, char **columnName) 
{
	xmms_playlist_entry_t *entry = pArg;

	XMMS_DBG ("Setting %s to %s", argv[1], argv[0]);
	xmms_playlist_entry_property_set (entry, argv[1], argv[0]);

	return 0;
}


/**
  * Takes a #xmms_playlist_entry_t and fills in the blanks as
  * they are in the database.
  */ 
gboolean
xmms_medialib_entry_get (xmms_playlist_entry_t *entry)
{
	gboolean ret;

	g_return_val_if_fail (medialib, FALSE);
	g_return_val_if_fail (entry, FALSE);

	XMMS_MTX_LOCK (medialib->mutex);

	ret = xmms_sqlite_query (mediarow_callback, (void *)entry, 
				 "select * from Media where url = '%q' order by id limit 1", 
				 xmms_playlist_entry_url_get (entry));

	if (!ret) {
		XMMS_MTX_UNLOCK (medialib->mutex);
		return FALSE;
	}

	if (!xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID)) {
		XMMS_MTX_UNLOCK (medialib->mutex);
		return FALSE;
	}

	ret = xmms_sqlite_query (proprow_callback, (void *) entry, 
				 "select key, value from Property where id = %q", 
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID));

	if (!ret) {
		XMMS_MTX_UNLOCK (medialib->mutex);
		return FALSE;
	}

	XMMS_MTX_UNLOCK (medialib->mutex);


	return TRUE;
}

/** Shutdown the medialib. */
void
xmms_medialib_close ()
{
	g_return_if_fail (medialib);

#ifdef HAVE_SQLITE
	xmms_sqlite_close (medialib);
	medialib->sql = NULL;
#endif
}

#ifdef HAVE_SQLITE
void
xmms_medialib_sql_set (sqlite *sql)
{
	medialib->sql = sql;
}

sqlite *
xmms_medialib_sql_get ()
{
	return medialib->sql;
}

#endif

/** @internal */
void
xmms_medialib_id_set (guint id)
{
	medialib->id = id;
}

/** @} */

