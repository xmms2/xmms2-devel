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

#include <stdio.h>
#include <stdlib.h>

#include "xmms/xmms_defs.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"
#include "xmmspriv/xmms_sqlite.h"

#include <sqlite3.h>
#include <glib.h>

/* increment this whenever there are incompatible db structure changes */
#define DB_VERSION 22

const char set_version_stm[] = "PRAGMA user_version=" XMMS_STRINGIFY (DB_VERSION);
const char create_Media_stm[] = "create table Media (id integer, key, value, source integer)";
const char create_Sources_stm[] = "create table Sources (id integer primary key AUTOINCREMENT, source)";
const char create_Log_stm[] = "create table Log (id, starttime, value)";
const char create_Playlist_stm[] = "create table Playlist (id primary key, name, pos integer)";
const char create_PlaylistEntries_stm[] = "create table PlaylistEntries (playlist_id int, entry, pos integer primary key AUTOINCREMENT)";
const char create_idx_stm[] = "create unique index key_idx on Media (id, key, source);"
						      "create index prop_idx on Media (key,value);"
                              "create index log_id on Log (id);"
                              "create index playlistentries_idx on PlaylistEntries (playlist_id, entry);"
                              "create index playlist_idx on Playlist (name);";

const char create_views[] = "CREATE VIEW artists as select distinct m1.value as artist from Media m1 left join Media m2 on m1.id = m2.id and m2.key='compilation' where m1.key='artist' and m2.value is null;"
			    "CREATE VIEW albums as select distinct m1.value as artist, ifnull(m2.value,'[unknown]') as album from Media m1 left join Media m2 on m1.id = m2.id and m2.key='album' left join Media m3 on m1.id = m3.id and m3.key='compilation' where m1.key='artist' and m3.value is null;"
			    "CREATE VIEW songs as select distinct m1.value as artist, ifnull(m2.value,'[unknown]') as album, ifnull(m3.value, m4.value) as title, ifnull(m5.value, -1) as tracknr, m1.id as id from Media m1 left join Media m2 on m1.id = m2.id and m2.key='album' left join Media m3 on m1.id = m3.id and m3.key='title' join Media m4 on m1.id = m4.id and m4.key='url' left join Media m5 on m1.id = m5.id and m5.key='tracknr' where m1.key='artist';"
			    "CREATE VIEW compilations as select distinct m1.value as compilation from Media m1 left join Media m2 on m1.id = m2.id and m2.key='compilation' where m1.key='album' and m2.value='1';"
			    "CREATE VIEW topsongs as select m.value as artist, m2.value as song, sum(l.value) as playsum, m.id as id, count(l.id) as times from Log l left join Media m on l.id=m.id and m.key='artist' left join Media m2 on m2.id = l.id and m2.key='title' group by l.id order by playsum desc;";

static void upgrade_v21_to_v22 (sqlite3 *sql);

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

static gboolean
try_upgrade (sqlite3 *sql, gint version)
{
	gboolean can_upgrade = TRUE;

	switch (version) {
		case 21:
			upgrade_v21_to_v22 (sql);
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
xmms_sqlite_open (gboolean *c)
{
	sqlite3 *sql;
	gboolean create = TRUE;
	const gchar *dbpath;
	gint version = 0;
	xmms_config_property_t *cv;

	cv = xmms_config_lookup ("medialib.path");
	dbpath = xmms_config_property_get_string (cv);

	if (g_file_test (dbpath, G_FILE_TEST_EXISTS)) {
		create = FALSE;
	}

	if (sqlite3_open (dbpath, &sql)) {
		xmms_log_fatal ("Error creating sqlite db: %s", sqlite3_errmsg(sql));
		return FALSE; 
	}
	
	sqlite3_busy_timeout (sql, 1000);

	sqlite3_exec (sql, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
/*	sqlite3_exec (sql, "PRAGMA cache_size = 4000", NULL, NULL, NULL);*/

	/* if the database already exists, check whether there have been
	 * any incompatible changes. if so, we need to recreate the db.
	 */
	if (!create) {
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
			create = TRUE;
		}
	}

	if (create) {
		XMMS_DBG ("Creating the database...");
		sqlite3_exec (sql, set_version_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, create_Media_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, create_Sources_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, "insert into Sources (source) values ('server')", NULL, NULL, NULL);
		sqlite3_exec (sql, create_Log_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, create_PlaylistEntries_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, create_Playlist_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, create_idx_stm, NULL, NULL, NULL);
		sqlite3_exec (sql, create_views, NULL, NULL, NULL);
	} 

	sqlite3_create_collation (sql, "INTCOLL", SQLITE_UTF8, NULL, xmms_sqlite_integer_coll);

	*c = create;

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
	if (ret != SQLITE_OK) {
		xmms_log_error ("Error in query! (%d) - %s", ret, err);
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
xmms_sqlite_query_table (sqlite3 *sql, xmms_medialib_row_table_method_t method, gpointer udata, const gchar *query, ...)
{
	gchar *q;
	va_list ap;
	gint ret;
	sqlite3_stmt *stm;

	g_return_val_if_fail (query, FALSE);
	g_return_val_if_fail (sql, FALSE);

	va_start (ap, query);

	q = sqlite3_vmprintf (query, ap);

	ret = sqlite3_prepare (sql, q, 0, &stm, NULL);


	if (ret != SQLITE_OK) {
		xmms_log_error ("Error in query! (%d) - %s", ret, q);
	}
	
	sqlite3_free (q);
	va_end (ap);

	while (sqlite3_step (stm) == SQLITE_ROW) {
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
			/*g_hash_table_destroy (ret);*/
			break;
		}

		/*
		g_hash_table_destroy (ret);
		*/
	}

	sqlite3_finalize (stm);

	return TRUE;
	
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
	sqlite3_stmt *stm;
	gboolean retval = FALSE;

	g_return_val_if_fail (query, FALSE);
	g_return_val_if_fail (sql, FALSE);

	va_start (ap, query);

	q = sqlite3_vmprintf (query, ap);

	ret = sqlite3_prepare (sql, q, 0, &stm, NULL);

	if (ret != SQLITE_OK) {
		xmms_log_error ("Error in query! (%d) - %s", ret, q);
	}

	while ((ret = sqlite3_step (stm)) == SQLITE_ROW) {		
		gint i;
		xmms_object_cmd_value_t **row;
		gint num = sqlite3_data_count (stm);

		row = g_new0 (xmms_object_cmd_value_t*, num+1);

		for (i = 0; i < num; i++) {
			row[i] = xmms_sqlite_column_to_val (stm, i);
		}

		retval = method (row, udata);

		if (!retval)
			break;
	}

	if (ret == SQLITE_DONE) {
		retval = TRUE;
	} else if (ret == SQLITE_ERROR || 
			   ret == SQLITE_MISUSE || 
			   ret == SQLITE_BUSY) {
		xmms_log_error ("SQLite Error code %d on query '%s'", ret, q);
		retval = FALSE;
	}

	sqlite3_finalize (stm);
	sqlite3_free (q);
	va_end (ap);

	return retval;
	
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

/** @} */
