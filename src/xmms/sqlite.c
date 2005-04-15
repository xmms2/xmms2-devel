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
 * Sqlite Backend.
 */

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

/* increment this whenever there are incompatible db structure changes */
#define DB_VERSION 6

const char create_Control_stm[] = "create table Control (version)";
const char create_Media_stm[] = "create table Media (id primary_key, key, value)";
const char create_Log_stm[] = "create table Log (id, starttime, value)";
const char create_Playlist_stm[] = "create table Playlist (id primary_key, name)";
const char create_PlaylistEntries_stm[] = "create table PlaylistEntries (playlist_id, entry, primary key (playlist_id, entry))";
const char create_idx_stm[] = "create unique index key_idx on Media (id, key);"
			      "create index prop_idx on Media (value);"
                              "create index log_id on Log (id);"
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

static int
xmms_sqlite_version_cb (void *pArg, int argc, char **argv, char **columnName)
{
	guint *id = pArg;

	if (argv[0]) {
		*id = atoi (argv[0]);
	} else {
		*id = 0;
	}

	return 0;
}

sqlite *
xmms_sqlite_open (guint *id)
{
	sqlite *sql;
	gchar *err;
	const gchar *hdir;
	gboolean create = TRUE;
	gchar *dbpath;
	gint version = 0;
	xmms_config_value_t *cv;

	cv = xmms_config_lookup ("medialib.path");
	if (cv) {
		dbpath = xmms_config_value_string_get (cv);
	} else {
		hdir = g_get_home_dir ();

		dbpath = g_malloc0 (XMMS_PATH_MAX+1);
		g_snprintf (dbpath, XMMS_PATH_MAX, "%s/.xmms2/medialib.db", hdir);
	}

	if (g_file_test (dbpath, G_FILE_TEST_EXISTS)) {
		create = FALSE;
	}

	XMMS_DBG ("opening database: %s", dbpath);

	sql = sqlite_open (dbpath, 0644, &err);
	if (!sql) {
		xmms_log_fatal ("Error creating sqlite db: %s", err);
		free (err);
		return FALSE; 
	}

	/* if the database already exists, check whether there have been
	 * any incompatible changes. if so, we need to recreate the db.
	 */
	if (!create) {
		sqlite_exec (sql, "select version from Control",
		             xmms_sqlite_version_cb, &version, NULL);
		if (version != DB_VERSION) {
			gchar old[XMMS_PATH_MAX];

			sqlite_close (sql);
			g_snprintf (old, XMMS_PATH_MAX, "%s/.xmms2/medialib.db.old", hdir);
			rename (dbpath, old);

			sql = sqlite_open (dbpath, 0644, &err);
			if (!sql) {
				xmms_log_fatal ("Error creating sqlite db: %s", err);
				free (err);
				return NULL;
			}

			create = TRUE;
		}
	}

	if (create) {
		gchar buf[128];

		g_snprintf (buf, sizeof (buf),
		            "insert into Control (version) VALUES (%i)",
		            DB_VERSION);

		XMMS_DBG ("Creating the database...");
		sqlite_exec (sql, create_Control_stm, NULL, NULL, NULL);
		sqlite_exec (sql, buf, NULL, NULL, NULL);
		sqlite_exec (sql, create_Media_stm, NULL, NULL, NULL);
		sqlite_exec (sql, create_Log_stm, NULL, NULL, NULL);
		sqlite_exec (sql, create_PlaylistEntries_stm, NULL, NULL, NULL);
		sqlite_exec (sql, create_Playlist_stm, NULL, NULL, NULL);
		sqlite_exec (sql, create_idx_stm, NULL, NULL, NULL);
		*id = 1;
	} else {
		sqlite_exec (sql, "select MAX (id) from Media", xmms_sqlite_id_cb, id, NULL);
	}

	return sql;
}

gboolean
xmms_sqlite_query (sqlite *sql, xmms_medialib_row_method_t method, void *udata, char *query, ...)
{
	gchar *err;
	va_list ap;
	gint ret;

	g_return_val_if_fail (query, FALSE);
	g_return_val_if_fail (sql, FALSE);

	va_start (ap, query);

	XMMS_DBG ("Running query: %s", query);

	ret = sqlite_exec_vprintf (sql, query, (sqlite_callback) method, udata, &err, ap);
	if (ret != SQLITE_OK) {
		xmms_log_error ("Error in query! (%d) - %s", ret, err);
		va_end (ap);
		return FALSE;
	}

	va_end (ap);

	return TRUE;
	
}

void
xmms_sqlite_close (sqlite *sql)
{
	g_return_if_fail (sql);
	sqlite_close (sql);
}
