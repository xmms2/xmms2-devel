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
 * Sqlite Backend.
 */

#include <stdio.h>
#include <stdlib.h>

#include "xmms/xmms_defs.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"
#include "xmmspriv/xmms_sqlite.h"
#include "xmmspriv/xmms_statfs.h"

#include <sqlite3.h>
#include <glib.h>

/* increment this whenever there are incompatible db structure changes */
#define DB_VERSION 27

const char set_version_stm[] = "PRAGMA user_version=" XMMS_STRINGIFY (DB_VERSION);
const char create_Media_stm[] = "create table Media (id integer, key, value, source integer)";
const char create_Sources_stm[] = "create table Sources (id integer primary key AUTOINCREMENT, source)";
const char create_Log_stm[] = "create table Log (id, starttime, percent)";
const char create_Playlist_stm[] = "create table Playlist (id primary key, name, pos integer)";
const char create_PlaylistEntries_stm[] = "create table PlaylistEntries (playlist_id int, entry, pos integer primary key AUTOINCREMENT)";

/** 
 * This magic numbers are taken from ANALYZE on a big database, if we change the db
 * layout drasticly we need to redo them!
 */
const char fill_stats[] = "INSERT INTO sqlite_stat1 VALUES('Media', 'key_idx', '199568 14 1 1');"
                          "INSERT INTO sqlite_stat1 VALUES('Media', 'prop_idx', '199568 6653 3');"
                          "INSERT INTO sqlite_stat1 VALUES('Log', 'log_id', '12 2');"
                          "INSERT INTO sqlite_stat1 VALUES('PlaylistEntries', 'playlistentries_idx', '12784 12784 1');"
                          "INSERT INTO sqlite_stat1 VALUES('Playlist', 'playlist_idx', '2 1');"
                          "INSERT INTO sqlite_stat1 VALUES('Playlist', 'sqlite_autoindex_Playlist_1', '2 1');";

const char create_idx_stm[] = "create unique index key_idx on Media (id,key,source);"
						      "create index prop_idx on Media (key,value);"
                              "create index log_id on Log (id);"
                              "create index playlistentries_idx on PlaylistEntries (playlist_id, entry);"
                              "create index playlist_idx on Playlist (name);";

static void upgrade_v21_to_v22 (sqlite3 *sql);
static void upgrade_v26_to_v27 (sqlite3 *sql);

/**
 * @defgroup SQLite SQLite
 * @ingroup XMMSServer
 * @brief The SQLite backend of medialib
 * @{
 */

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

static int
xmms_sqlite_integer_coll (void *udata, int len1, const void *str1, int len2, const void *str2)
{
	guint32 a, b;
	a = strtol(str1, NULL, 10);
	b = strtol(str2, NULL, 10);
	if (a < b) return -1;
	if (a == b) return 0;
	return 1;
}

static void
upgrade_v21_to_v22 (sqlite3 *sql)
{
	XMMS_DBG ("Performing upgrade v21 to v22");

	sqlite3_exec (sql,
	              "update Playlist "
	              "set name = '_autosaved' where name = 'autosaved';",
	              NULL, NULL, NULL);

	XMMS_DBG ("done");
}

static void
upgrade_v26_to_v27 (sqlite3 *sql)
{
	XMMS_DBG ("Upgrade v26->v27");
	sqlite3_exec (sql,
	              "drop view albums;"
	              "drop view artists;"
	              "drop view compilations;"
	              "drop view songs;",
	              NULL, NULL, NULL);

	XMMS_DBG ("done");
}

static gboolean
try_upgrade (sqlite3 *sql, gint version)
{
	gboolean can_upgrade = TRUE;

	switch (version) {
		case 21:
			upgrade_v21_to_v22 (sql);
			break;
		case 26:
			upgrade_v26_to_v27 (sql);
			break;
		default:
			can_upgrade = FALSE;
			break;
	}

	if (can_upgrade) {
		/* store the new version in the database */
		sqlite3_exec (sql, set_version_stm, NULL, NULL, NULL);
	}

	return can_upgrade;
}

/**
 * Open a database or create a new one
 */
sqlite3 *
xmms_sqlite_open (gboolean *create)
{
	sqlite3 *sql;
	const gchar *dbpath;
	gint version = 0;
	xmms_config_property_t *cv;
	gchar *tmp;

	cv = xmms_config_lookup ("medialib.path");
	dbpath = xmms_config_property_get_string (cv);

	*create = FALSE;
	if (!g_file_test (dbpath, G_FILE_TEST_EXISTS)) {
		*create = TRUE;
	}

	if (sqlite3_open (dbpath, &sql)) {
		xmms_log_fatal ("Error creating sqlite db: %s", sqlite3_errmsg(sql));
		return NULL;
	}

	g_return_val_if_fail (sql, NULL);

	sqlite3_exec (sql, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
	sqlite3_exec (sql, "PRAGMA auto_vacuum = 1", NULL, NULL, NULL);
	sqlite3_exec (sql, "PRAGMA cache_size = 8000", NULL, NULL, NULL);
	sqlite3_exec (sql, "PRAGMA temp_store = MEMORY", NULL, NULL, NULL);

	/* One minute */
	sqlite3_busy_timeout (sql, 60000);

	/* if the database already exists, check whether there have been
	 * any incompatible changes. if so, we need to recreate the db.
	 */
	if (!*create) {
		sqlite3_exec (sql, "PRAGMA user_version",
		              xmms_sqlite_version_cb, &version, NULL);

		if (version != DB_VERSION && !try_upgrade (sql, version)) {
			gchar old[XMMS_PATH_MAX];

			sqlite3_close (sql);
			g_snprintf (old, XMMS_PATH_MAX, "%s/.xmms2/medialib.db.old",
			            g_get_home_dir ());
			rename (dbpath, old);
			if (sqlite3_open (dbpath, &sql)) {
				xmms_log_fatal ("Error creating sqlite db: %s",
				                sqlite3_errmsg (sql));
				return NULL;
			}
			sqlite3_exec (sql, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
			*create = TRUE;
		}
	}

	if (*create) {
		/* Check if we are about to put the medialib on a
		 * remote filesystem. They are known to work less
		 * well with sqlite and therefore we should refuse
		 * to do so. The user has to know that he is doing
		 * something stupid
		 */

		tmp = g_path_get_dirname (dbpath);
		if (xmms_statfs_is_remote (tmp)) {
			cv = xmms_config_lookup ("medialib.allow_remote_fs");
			if (xmms_config_property_get_int (cv) == 1) {
				xmms_log_info ("Allowing database on remote system against best judgement.");
			} else {
				xmms_log_fatal ("Remote filesystem detected!\n"
				                "* It looks like you are putting your database: %s\n"
				                "* on a remote filesystem, this is a bad idea since there are many known bugs\n"
				                "* with SQLite on some remote filesystems. We recomend that you put the db\n"
				                "* somewhere else. You can do this by editing the xmms2.conf and find the\n"
				                "* property for medialib.path. If you however still want to try to run the\n"
				                "* db on a remote filesystem please set medialib.allow_remote_fs=1 in your\n"
				                "* config and restart xmms2d.", dbpath);
			}
		}

		g_free (tmp);

		XMMS_DBG ("Creating the database...");
		/**
		 * This will create the sqlite_stats1 table which we
		 * fill out with good information about our indexes.
		 * Thanks to drh for these pointers!
		 */
		sqlite3_exec (sql, "ANALYZE", NULL, NULL, NULL);
		/**
		 * Fill out sqlite_stats1
		 */
		sqlite3_exec (sql, fill_stats, NULL, NULL, NULL);
		/**
		 * Create the rest of our tables
		 */
		sqlite3_exec (sql, create_Media_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, create_Sources_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, "insert into Sources (source) values ('server')", NULL, NULL, NULL);
		sqlite3_exec (sql, create_Log_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, create_PlaylistEntries_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, create_Playlist_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, create_idx_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, set_version_stm, NULL, NULL, NULL);
	} 

	sqlite3_create_collation (sql, "INTCOLL", SQLITE_UTF8, NULL, xmms_sqlite_integer_coll);

	return sql;
}

static xmms_object_cmd_value_t *
xmms_sqlite_column_to_val (sqlite3_stmt *stm, gint column)
{
	xmms_object_cmd_value_t *val = NULL;

	switch (sqlite3_column_type (stm, column)) {
		case SQLITE_INTEGER:
		case SQLITE_FLOAT:
			val = xmms_object_cmd_value_int_new (sqlite3_column_int (stm, column));
			break;
		case SQLITE_TEXT:
		case SQLITE_BLOB:
			val = xmms_object_cmd_value_str_new ((gchar *)sqlite3_column_text (stm, column));
			break;
		case SQLITE_NULL:
			val = xmms_object_cmd_value_none_new ();
			break;
		default:
			XMMS_DBG ("Unhandled SQLite type!");
			break;
	}

	return val;

}

/**
 * A query that can't retrieve results
 */
gboolean
xmms_sqlite_exec (sqlite3 *sql, const char *query, ...)
{
	gchar *q, *err;
	va_list ap;
	gint ret;

	g_return_val_if_fail (query, FALSE);
	g_return_val_if_fail (sql, FALSE);

	va_start (ap, query);

	q = sqlite3_vmprintf (query, ap);

	ret = sqlite3_exec (sql, q, NULL, NULL, &err);
	if (ret == SQLITE_BUSY) {
		xmms_log_fatal ("BUSY EVENT!");
		g_assert_not_reached();
	}
	if (ret != SQLITE_OK) {
		xmms_log_error ("Error in query! \"%s\" (%d) - %s", q, ret, err);
		sqlite3_free (q);
		va_end (ap);
		return FALSE;
	}

	sqlite3_free (q);
	va_end (ap);

	return TRUE;
}

/**
 * Execute a query to the database.
 */
gboolean
xmms_sqlite_query_table (sqlite3 *sql, xmms_medialib_row_table_method_t method, gpointer udata, xmms_error_t *error, const gchar *query, ...)
{
	gchar *q;
	va_list ap;
	gint ret;
	sqlite3_stmt *stm;

	g_return_val_if_fail (query, FALSE);
	g_return_val_if_fail (sql, FALSE);

	va_start (ap, query);
	q = sqlite3_vmprintf (query, ap);
	va_end (ap);

	ret = sqlite3_prepare (sql, q, -1, &stm, NULL);

	if (ret == SQLITE_BUSY) {
		xmms_log_fatal ("BUSY EVENT!");
	}

	if (ret != SQLITE_OK) {
		gchar err[256];
		g_snprintf (err, sizeof (err),
		            "Error in query: %s", sqlite3_errmsg (sql));
		xmms_error_set (error, XMMS_ERROR_GENERIC, err);
		xmms_log_error ("Error %d (%s) in query '%s'", ret, sqlite3_errmsg (sql), q);
		sqlite3_free (q);
		return FALSE;
	}

	while ((ret = sqlite3_step (stm)) == SQLITE_ROW) {
		gint i;
		xmms_object_cmd_value_t *val;
		GHashTable *ret = g_hash_table_new_full (g_str_hash, g_str_equal,
							 g_free, xmms_object_cmd_value_free);
		gint num = sqlite3_data_count (stm);

		for (i = 0; i < num; i++) {
			val = xmms_sqlite_column_to_val (stm, i);
			g_hash_table_insert (ret, g_strdup (sqlite3_column_name (stm, i)), val);
		}

		if (!method (ret, udata)) {
			break;
		}

	}

	if (ret == SQLITE_ERROR) {
		xmms_log_error ("SQLite Error code %d (%s) on query '%s'", ret, sqlite3_errmsg (sql), q);
	} else if (ret == SQLITE_MISUSE) {
		xmms_log_error ("SQLite api misuse on query '%s'", q);
	} else if (ret == SQLITE_BUSY) {
		xmms_log_error ("SQLite busy on query '%s'", q);
	}

	sqlite3_free (q);
	sqlite3_finalize (stm);
	
	return (ret == SQLITE_DONE);
}

/**
 * Execute a query to the database.
 */
gboolean
xmms_sqlite_query_array (sqlite3 *sql, xmms_medialib_row_array_method_t method, gpointer udata, const gchar *query, ...)
{
	gchar *q;
	va_list ap;
	gint ret;
	sqlite3_stmt *stm = NULL;

	g_return_val_if_fail (query, FALSE);
	g_return_val_if_fail (sql, FALSE);

	va_start (ap, query);
	q = sqlite3_vmprintf (query, ap);
	va_end (ap);

	ret = sqlite3_prepare (sql, q, -1, &stm, NULL);

	if (ret == SQLITE_BUSY) {
		xmms_log_fatal ("BUSY EVENT!");
		g_assert_not_reached();
	}

	if (ret != SQLITE_OK) {
		xmms_log_error ("Error %d (%s) in query '%s'", ret, sqlite3_errmsg (sql), q);
		sqlite3_free (q);
		return FALSE;
	}

	while ((ret = sqlite3_step (stm)) == SQLITE_ROW) {		
		gint i;
		xmms_object_cmd_value_t **row;
		gint num = sqlite3_data_count (stm);

		row = g_new0 (xmms_object_cmd_value_t*, num+1);

		for (i = 0; i < num; i++) {
			row[i] = xmms_sqlite_column_to_val (stm, i);
		}

		if (!method (row, udata))
			break;
	}

	if (ret == SQLITE_ERROR) {
		xmms_log_error ("SQLite Error code %d (%s) on query '%s'", ret, sqlite3_errmsg (sql), q);
	} else if (ret == SQLITE_MISUSE) {
		xmms_log_error ("SQLite api misuse on query '%s'", q);
	} else if (ret == SQLITE_BUSY) {
		xmms_log_error ("SQLite busy on query '%s'", q);
	}

	sqlite3_free (q);
	sqlite3_finalize (stm);

	return (ret == SQLITE_DONE);
}

/**
 * Close database and free all resources used.
 */
void
xmms_sqlite_close (sqlite3 *sql)
{
	g_return_if_fail (sql);
	sqlite3_close (sql);
}

void
xmms_sqlite_print_version (void)
{
	printf (" Using sqlite version %d (compiled against "
		XMMS_STRINGIFY (SQLITE_VERSION_NUMBER) ")\n",
		sqlite3_libversion_number());
}

/** @} */
