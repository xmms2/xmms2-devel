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

/** @file Sqlite Backend. */

#include <stdlib.h>

#include "xmms/xmms.h"
#include "xmms/transport.h"
#include "xmms/medialib.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/playlist.h"
#include "xmms/mediainfo.h"
#include "xmms/plsplugins.h"
#include "xmms/signal_xmms.h"

#include <sqlite.h>
#include <glib.h>

const char create_Media_stm[] = "create table Media (id primary_key, url, artist, album, title, genre, lmod)";
const char create_Property_stm[] = "create table Property (id primary_key, value, key)";
const char create_Log_stm[] = "create table Log (id primary_key, starttime, value)";
const char create_Playlist_stm[] = "create table Playlist (id primary_key, name)";
const char create_PlaylistEntries_stm[] = "create table PlaylistEntries (playlist_id, entry, primary key (playlist_id, entry))";
const char create_idx_stm[] = "create index url_idx on Media (url);"
                              "create index prop_key_idx on Property (key);"
                              "create index log_starttime_idx on Log (starttime);"
                              "create index playlist_idx on Playlist (name);";


static int
xmms_sqlite_id_cb (void *pArg, int argc, char **argv, char **columnName) 
{
	guint *id = pArg;

	if (argv[0]) {
		*id = atoi (argv[0]) + 1;
	} else {
		*id = 1;
	}

	return 0;
}

gboolean
xmms_sqlite_open ()
{
	sqlite *sql;
	gchar *err;
	const gchar *hdir;
	gboolean create = TRUE;
	gchar dbpath[XMMS_PATH_MAX];
	guint id;

	hdir = g_get_home_dir ();

	g_snprintf (dbpath, XMMS_PATH_MAX, "%s/.xmms2/medialib.db", hdir);

	if (g_file_test (dbpath, G_FILE_TEST_EXISTS)) {
		create = FALSE;
	}

	sql = sqlite_open (dbpath, 0644, &err);
	if (!sql) {
		xmms_log_fatal ("Error creating sqlite db: %s", err);
		free (err);
		return FALSE; 
	}

	if (create) {
		XMMS_DBG ("Creating the database...");
		sqlite_exec (sql, create_Media_stm, NULL, NULL, NULL);
		sqlite_exec (sql, create_Property_stm, NULL, NULL, NULL);
		sqlite_exec (sql, create_Log_stm, NULL, NULL, NULL);
		sqlite_exec (sql, create_PlaylistEntries_stm, NULL, NULL, NULL);
		sqlite_exec (sql, create_Playlist_stm, NULL, NULL, NULL);
		sqlite_exec (sql, create_idx_stm, NULL, NULL, NULL);
		id = 1;
	} else {
		sqlite_exec (sql, "select MAX (id) from Media", xmms_sqlite_id_cb, &id, NULL);
	}

	xmms_medialib_sql_set (sql);
	xmms_medialib_id_set (id);

	return TRUE;
}

gboolean
xmms_sqlite_query (xmms_medialib_row_method_t method, void *udata, char *query, ...)
{
	gchar *err;
	sqlite *sql;
	va_list ap;

	g_return_val_if_fail (query, FALSE);

	va_start (ap, query);

	sql = xmms_medialib_sql_get ();

	XMMS_DBG ("Running query: %s", query);

	if (sqlite_exec_vprintf (sql, query, (sqlite_callback) method, udata, &err, ap) != SQLITE_OK) {
		xmms_log_error ("Error in query! %s", err);
		va_end (ap);
		return FALSE;
	}

	va_end (ap);

	return TRUE;
	
}

void
xmms_sqlite_close ()
{
	sqlite *sql;
	sql = xmms_medialib_sql_get ();
	sqlite_close (sql);
}
