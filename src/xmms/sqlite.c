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
 * Sqlite Backend.
 */

#include <stdio.h>
#include <stdlib.h>

#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"
#include "xmmspriv/xmms_sqlite.h"
#include "xmmspriv/xmms_statfs.h"
#include "xmmspriv/xmms_utils.h"
#include "xmmspriv/xmms_collection.h"
#include "xmmsc/xmmsc_idnumbers.h"

#include <sqlite3.h>
#include <string.h>
#include <glib.h>

/* increment this whenever there are incompatible db structure changes */
#define DB_VERSION 36

const char set_version_stm[] = "PRAGMA user_version=" XMMS_STRINGIFY (DB_VERSION);

/* Tables and unique constraints */
const char *tables[] = {
	/* Media */
	"CREATE TABLE Media (id INTEGER, key, value, source INTEGER, "
	                    "intval INTEGER DEFAULT NULL)",
	/* Media unique constraint */
	"CREATE UNIQUE INDEX key_idx ON Media (id, key, source)",

	/* Sources */
	"CREATE TABLE Sources (id INTEGER PRIMARY KEY AUTOINCREMENT, source)",

	/* CollectionAttributes */
	"CREATE TABLE CollectionAttributes (collid INTEGER, key TEXT, value TEXT)",
	/* CollectionAttributes unique constraint */
	"CREATE UNIQUE INDEX collectionattributes_idx "
	       "ON CollectionAttributes (collid, key)",

	/* CollectionConnections */
	"CREATE TABLE CollectionConnections (from_id INTEGER, to_id INTEGER)",
	/* CollectionConnections unique constraint */
	"CREATE UNIQUE INDEX collectionconnections_idx "
	       "ON CollectionConnections (from_id, to_id)",

	/* CollectionIdlists */
	"CREATE TABLE CollectionIdlists (collid INTEGER, position INTEGER, "
	                                "mid INTEGER)",
	/* CollectionIdlists unique constraint */
	"CREATE UNIQUE INDEX collectionidlists_idx "
	       "ON CollectionIdlists (collid, position)",

	/* CollectionLabels */
	"CREATE TABLE CollectionLabels (collid INTEGER, namespace INTEGER, "
	                               "name TEXT)",

	/* CollectionOperators */
	"CREATE TABLE CollectionOperators (id INTEGER PRIMARY KEY AUTOINCREMENT, "
	                                  "type INTEGER)",
	NULL
};

const char *views[] = {
	NULL
};

const char *triggers[] = {
	NULL
};

const char *indices[] = {
	/* Media idices */
	"CREATE INDEX id_key_value_1x ON Media (id, key, value COLLATE BINARY)",
	"CREATE INDEX id_key_value_2x ON Media (id, key, value COLLATE NOCASE)",
	"CREATE INDEX key_value_1x ON Media (key, value COLLATE BINARY)",
	"CREATE INDEX key_value_2x ON Media (key, value COLLATE NOCASE)",

	/* Collections DAG index */
	"CREATE INDEX collectionlabels_idx ON CollectionLabels (collid)",

	NULL
};

const char create_CollectionAttributes_stm[] = "create table CollectionAttributes (collid integer, key text, value text)";
const char create_CollectionConnections_stm[] = "create table CollectionConnections (from_id integer, to_id integer)";
const char create_CollectionIdlists_stm[] = "create table CollectionIdlists (collid integer, position integer, mid integer)";
const char create_CollectionLabels_stm[] = "create table CollectionLabels (collid integer, namespace integer, name text)";
const char create_CollectionOperators_stm[] = "create table CollectionOperators (id integer primary key AUTOINCREMENT, type integer)";

/**
 * This magic numbers are taken from ANALYZE on a big database, if we change the db
 * layout drasticly we need to redo them!
 */
const char fill_stats[] = "INSERT INTO sqlite_stat1 VALUES('Media', 'key_idx', '199568 14 1 1');"
                          "INSERT INTO sqlite_stat1 VALUES('Media', 'prop_idx', '199568 6653 3');"
                          "INSERT INTO sqlite_stat1 VALUES('PlaylistEntries', 'playlistentries_idx', '12784 12784 1');"
                          "INSERT INTO sqlite_stat1 VALUES('Playlist', 'playlist_idx', '2 1');"
                          "INSERT INTO sqlite_stat1 VALUES('Playlist', 'sqlite_autoindex_Playlist_1', '2 1');"
                          "INSERT INTO sqlite_stat1 VALUES('CollectionLabels', 'collectionlabels_idx', '2 2');"
                          "INSERT INTO sqlite_stat1 VALUES('CollectionIdlists', 'collectionidlists_idx', '15 15 1');"
                          "INSERT INTO sqlite_stat1 VALUES('CollectionAttributes', 'collectionattributes_idx', '2 2 1');";

const char fill_init_playlist_stm[] = "INSERT INTO CollectionOperators VALUES(1, %d);"
                                      "INSERT INTO CollectionLabels VALUES(1, %d, 'Default');"
                                      "INSERT INTO CollectionLabels VALUES(1, %d, '" XMMS_ACTIVE_PLAYLIST "');"
                                      "INSERT INTO CollectionIdlists VALUES(1, 1, 1);";

const char create_collidx_stm[] = "create unique index collectionconnections_idx on CollectionConnections (from_id, to_id);"
                                  "create unique index collectionattributes_idx on CollectionAttributes (collid, key);"
                                  "create unique index collectionidlists_idx on CollectionIdlists (collid, position);"
                                  "create index collectionlabels_idx on CollectionLabels (collid);";

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
	gint32 a, b;
	a = strtol (str1, NULL, 10);
	b = strtol (str2, NULL, 10);
	if (a < b) return -1;
	if (a == b) return 0;
	return 1;
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

static void
upgrade_v27_to_v28 (sqlite3 *sql)
{
	XMMS_DBG ("Upgrade v27->v28");

	sqlite3_exec (sql,
	              "drop table Log;",
	              NULL, NULL, NULL);

	XMMS_DBG ("done");
}

static void
upgrade_v28_to_v29 (sqlite3 *sql)
{
	XMMS_DBG ("Upgrade v28->v29");

	sqlite3_exec (sql, "delete from Media where source in"
	              "(select id from Sources where source like 'plugin%')",
	              NULL, NULL, NULL);
	sqlite3_exec (sql, "delete from Sources where source like 'plugin%'",
	              NULL, NULL, NULL);
	sqlite3_exec (sql, "update Media set value=0 where key='resolved'",
	              NULL, NULL, NULL);

	XMMS_DBG ("done");
}

static void
upgrade_v29_to_v30 (sqlite3 *sql)
{
	XMMS_DBG ("Upgrade v29->v30");
	sqlite3_exec (sql, "insert into Media (id, key, value, source) select distinct id, 'available', 1, (select id from Sources where source='server') from Media", NULL, NULL, NULL);
	XMMS_DBG ("done");
}

static void
upgrade_v30_to_v31 (sqlite3 *sql)
{
	XMMS_DBG ("Upgrade v30->v31");

	sqlite3_exec (sql, create_CollectionAttributes_stm, NULL, NULL, NULL);
	sqlite3_exec (sql, create_CollectionConnections_stm, NULL, NULL, NULL);
	sqlite3_exec (sql, create_CollectionIdlists_stm, NULL, NULL, NULL);
	sqlite3_exec (sql, create_CollectionLabels_stm, NULL, NULL, NULL);
	sqlite3_exec (sql, create_CollectionOperators_stm, NULL, NULL, NULL);
	sqlite3_exec (sql, create_collidx_stm, NULL, NULL, NULL);

	/* Create a default playlist */
	xmms_sqlite_exec (sql, fill_init_playlist_stm, XMMS_COLLECTION_TYPE_IDLIST,
	                                               XMMS_COLLECTION_NSID_PLAYLISTS,
	                                               XMMS_COLLECTION_NSID_PLAYLISTS);

	XMMS_DBG ("done");
}

static void
upgrade_v31_to_v32 (sqlite3 *sql)
{
	XMMS_DBG ("upgrade v31->v32");
	sqlite3_exec (sql, "delete from Media where id = (select id from Media where key='available' and value=0)", NULL, NULL, NULL);
	sqlite3_exec (sql, "delete from Media where key='available' and source = 1", NULL, NULL, NULL);
	sqlite3_exec (sql, "update media set key='status' where key='resolved' and source = 1", NULL, NULL, NULL);
	XMMS_DBG ("done");
}

static void
upgrade_v32_to_v33 (sqlite3 *sql)
{
	/* Decrement collection type id, as we removed ERROR from the enum. */
	XMMS_DBG ("upgrade v32->v33");
	sqlite3_exec (sql, "update CollectionOperators set type=type - 1", NULL, NULL, NULL);
	XMMS_DBG ("done");
}

static void
upgrade_v33_to_v34 (sqlite3 *sql)
{
	XMMS_DBG ("upgrade v33->v34");
	sqlite3_exec (sql, "update CollectionAttributes set value=replace(replace(value, '%', '*'), '_', '?') WHERE collid IN (SELECT id FROM CollectionOperators WHERE type='6')", NULL, NULL, NULL);
	XMMS_DBG ("done");
}


static void
upgrade_v34_to_v35 (sqlite3 *sql)
{
	XMMS_DBG ("upgrade v34->v35");
	sqlite3_exec (sql, "DROP INDEX prop_idx;"
	                   "CREATE INDEX id_key_value_1x ON Media (id, key, value COLLATE BINARY);"
	                   "CREATE INDEX id_key_value_2x ON Media (id, key, value COLLATE NOCASE);"
	                   "CREATE INDEX key_value_1x ON Media (key, value COLLATE BINARY);"
	                   "CREATE INDEX key_value_2x ON Media (key, value COLLATE NOCASE);"
	                   "UPDATE CollectionAttributes SET value=replace(replace(value, '%', '*'), '_', '?') WHERE collid IN (SELECT id FROM CollectionOperators WHERE type='6');", NULL, NULL, NULL);
	XMMS_DBG ("done");
}

static void
xmms_sqlite_stringify (sqlite3_context *context, int args, sqlite3_value **val)
{
	gint i;
	gchar buffer[32];

	if (sqlite3_value_type (val[0]) == SQLITE_INTEGER) {
		i = sqlite3_value_int (val[0]);
		sprintf (buffer, "%d", i);
		sqlite3_result_text (context, buffer, -1, SQLITE_TRANSIENT);
	} else {
		sqlite3_result_value (context, val[0]);
	}
}

static void
upgrade_v35_to_v36 (sqlite3 *sql)
{
	XMMS_DBG ("upgrade v35->v36 (save integers as strings also)");

	xmms_sqlite_exec (sql, "ALTER TABLE Media "
	                       "ADD COLUMN intval INTEGER DEFAULT NULL");

	sqlite3_create_function (sql, "xmms_stringify", 1, SQLITE_UTF8, NULL,
	                         xmms_sqlite_stringify, NULL, NULL);
	xmms_sqlite_exec (sql, "UPDATE Media "
	                       "SET intval = value, value = xmms_stringify (value) "
	                       "WHERE value < ''",
	                  NULL, NULL, NULL);
	sqlite3_create_function (sql, "xmms_stringify", 1, SQLITE_UTF8, NULL, NULL,
	                         NULL, NULL);

	XMMS_DBG ("done");
}

static gboolean
try_upgrade (sqlite3 *sql, gint version)
{
	gboolean can_upgrade = TRUE;

	switch (version) {
		case 26:
			upgrade_v26_to_v27 (sql);
		case 27:
			upgrade_v27_to_v28 (sql);
		case 28:
			upgrade_v28_to_v29 (sql);
		case 29:
			upgrade_v29_to_v30 (sql);
		case 30:
			upgrade_v30_to_v31 (sql);
		case 31:
			upgrade_v31_to_v32 (sql);
		case 32:
			upgrade_v32_to_v33 (sql);
		case 33:
			upgrade_v33_to_v34 (sql);
		case 34:
			upgrade_v34_to_v35 (sql);
		case 35:
			upgrade_v35_to_v36 (sql);
			break; /* remember to (re)move this! We want fallthrough */
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

static void
xmms_sqlite_set_common_properties (sqlite3 *sql)
{
	sqlite3_exec (sql, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
	sqlite3_exec (sql, "PRAGMA auto_vacuum = 1", NULL, NULL, NULL);
	sqlite3_exec (sql, "PRAGMA cache_size = 8000", NULL, NULL, NULL);
	sqlite3_exec (sql, "PRAGMA temp_store = MEMORY", NULL, NULL, NULL);

	/* One minute */
	sqlite3_busy_timeout (sql, 60000);

	sqlite3_create_collation (sql, "INTCOLL", SQLITE_UTF8, NULL,
	                          xmms_sqlite_integer_coll);
}

gboolean
xmms_sqlite_create (gboolean *create)
{
	xmms_config_property_t *cv;
	gchar *tmp;
	gboolean analyze = FALSE;
	const gchar *dbpath;
	gint version = 0;
	sqlite3 *sql;
	guint i;

	*create = FALSE;

	cv = xmms_config_lookup ("medialib.path");
	dbpath = xmms_config_property_get_string (cv);

	if (!g_file_test (dbpath, G_FILE_TEST_EXISTS)) {
		*create = TRUE;
	}

	if (sqlite3_open (dbpath, &sql)) {
		xmms_log_fatal ("Error opening sqlite db: %s", sqlite3_errmsg (sql));
		return FALSE;
	}

	xmms_sqlite_set_common_properties (sql);

	if (!*create) {
		sqlite3_exec (sql, "PRAGMA user_version",
		              xmms_sqlite_version_cb, &version, NULL);

		if (version != DB_VERSION && !try_upgrade (sql, version)) {
			gchar *old;

			sqlite3_close (sql);

			old = XMMS_BUILD_PATH ("medialib.db.old");
			rename (dbpath, old);
			if (sqlite3_open (dbpath, &sql)) {
				xmms_log_fatal ("Error creating sqlite db: %s",
				                sqlite3_errmsg (sql));
				g_free (old);
				return FALSE;
			}
			g_free (old);

			xmms_sqlite_set_common_properties (sql);
			*create = TRUE;
		}

		cv = xmms_config_lookup ("medialib.analyze_on_startup");
		analyze = xmms_config_property_get_int (cv);
		if (analyze) {
			xmms_log_info ("Analyzing db, please wait a few seconds");
			sqlite3_exec (sql, "ANALYZE", NULL, NULL, NULL);
			xmms_log_info ("Done with analyze");
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
		 * Create the tables and unique constraints
		 */
		for (i = 0; tables[i]; i++) {
			sqlite3_exec (sql, tables[i], NULL, NULL, NULL);
		}
		/**
		 * Create the views
		 */
		for (i = 0; views[i]; i++) {
			sqlite3_exec (sql, views[i], NULL, NULL, NULL);
		}
		/**
		 * Create the triggers
		 */
		for (i = 0; triggers[i]; i++) {
			sqlite3_exec (sql, triggers[i], NULL, NULL, NULL);
		}
		/**
		 * Create indices
		 */
		for (i = 0; indices[i]; i++) {
			sqlite3_exec (sql, indices[i], NULL, NULL, NULL);
		}
		/**
		 * Add the server source
		 */
		sqlite3_exec (sql, "INSERT INTO Sources (source) VALUES ('server')",
		              NULL, NULL, NULL);
		/**
		 * Create a default playlist
		 */
		xmms_sqlite_exec (sql, fill_init_playlist_stm,
		                  XMMS_COLLECTION_TYPE_IDLIST,
		                  XMMS_COLLECTION_NSID_PLAYLISTS,
		                  XMMS_COLLECTION_NSID_PLAYLISTS);
		/**
		 * Set database version
		 */
		sqlite3_exec (sql, set_version_stm, NULL, NULL, NULL);
	}

	sqlite3_close (sql);

	XMMS_DBG ("xmms_sqlite_create done!");
	return TRUE;
}

/**
 * Open a database or create a new one
 */
sqlite3 *
xmms_sqlite_open ()
{
	sqlite3 *sql;
	const gchar *dbpath;
	xmms_config_property_t *cv;

	cv = xmms_config_lookup ("medialib.path");
	dbpath = xmms_config_property_get_string (cv);

	if (sqlite3_open (dbpath, &sql)) {
		xmms_log_fatal ("Error opening sqlite db: %s", sqlite3_errmsg (sql));
		return NULL;
	}

	g_return_val_if_fail (sql, NULL);

	xmms_sqlite_set_common_properties (sql);

	return sql;
}

static xmmsv_t *
xmms_sqlite_column_to_val (sqlite3_stmt *stm, gint column)
{
	xmmsv_t *val = NULL;

	switch (sqlite3_column_type (stm, column)) {
		case SQLITE_INTEGER:
		case SQLITE_FLOAT:
			val = xmmsv_new_int (sqlite3_column_int (stm, column));
			break;
		case SQLITE_TEXT:
		case SQLITE_BLOB:
			val = xmmsv_new_string ((gchar *)sqlite3_column_text (stm, column));
			break;
		case SQLITE_NULL:
			val = xmmsv_new_none ();
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
		g_assert_not_reached ();
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
		g_assert_not_reached ();
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
		gint num, i;
		xmmsv_t *dict;

		dict = xmmsv_new_dict ();
		num = sqlite3_data_count (stm);

		for (i = 0; i < num; i++) {
			const char *key;
			xmmsv_t *val;

			/* We don't need to strdup the key because xmmsv_dict_set
			 * will create its own copy.
			 */
			key = sqlite3_column_name (stm, i);
			val = xmms_sqlite_column_to_val (stm, i);

			xmmsv_dict_set (dict, key, val);

			/* The dictionary owns the value. */
			xmmsv_unref (val);
		}

		if (!method (dict, udata)) {
			break;
		}

	}

	if (ret == SQLITE_ERROR) {
		xmms_log_error ("SQLite Error code %d (%s) on query '%s'", ret, sqlite3_errmsg (sql), q);
	} else if (ret == SQLITE_MISUSE) {
		xmms_log_error ("SQLite api misuse on query '%s'", q);
	} else if (ret == SQLITE_BUSY) {
		xmms_log_error ("SQLite busy on query '%s'", q);
		g_assert_not_reached ();
	}

	sqlite3_free (q);
	sqlite3_finalize (stm);

	return (ret == SQLITE_DONE);
}

/**
 * Execute a query to the database.
 */
static gboolean
xmms_sqlite_query_array_va (sqlite3 *sql, xmms_medialib_row_array_method_t method, gpointer udata, const gchar *query, va_list ap)
{
	gchar *q;
	gint ret, num_cols;
	xmmsv_t **row;
	sqlite3_stmt *stm = NULL;

	g_return_val_if_fail (query, FALSE);
	g_return_val_if_fail (sql, FALSE);

	q = sqlite3_vmprintf (query, ap);

	ret = sqlite3_prepare (sql, q, -1, &stm, NULL);

	if (ret == SQLITE_BUSY) {
		xmms_log_fatal ("BUSY EVENT!");
		g_assert_not_reached ();
	}

	if (ret != SQLITE_OK) {
		xmms_log_error ("Error %d (%s) in query '%s'", ret, sqlite3_errmsg (sql), q);
		sqlite3_free (q);
		return FALSE;
	}

	num_cols = sqlite3_column_count (stm);

	row = g_new (xmmsv_t *, num_cols + 1);
	row[num_cols] = NULL;

	while ((ret = sqlite3_step (stm)) == SQLITE_ROW) {
		gint i;
		gboolean b;

		/* I'm a bit paranoid */
		g_assert (num_cols == sqlite3_data_count (stm));

		for (i = 0; i < num_cols; i++) {
			row[i] = xmms_sqlite_column_to_val (stm, i);
		}

		b = method (row, udata);

		for (i = 0; i < num_cols; i++) {
			xmmsv_unref (row[i]);
		}

		if (!b) {
			break;
		}
	}

	g_free (row);

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

gboolean
xmms_sqlite_query_array (sqlite3 *sql, xmms_medialib_row_array_method_t method, gpointer udata, const gchar *query, ...)
{
	va_list ap;
	gboolean r;

	va_start (ap, query);
	r = xmms_sqlite_query_array_va (sql, method, udata, query, ap);
	va_end (ap);

	return r;
}

static gboolean
xmms_sqlite_int_cb (xmmsv_t **row, gpointer udata)
{
	gint *i = udata;

	if (row && row[0] && xmmsv_get_type (row[0]) == XMMSV_TYPE_INT32)
		xmmsv_get_int (row[0], i);
	else
		XMMS_DBG ("Expected int32 but got something else!");

	return TRUE;
}

gboolean
xmms_sqlite_query_int (sqlite3 *sql, gint32 *out, const gchar *query, ...)
{
	va_list ap;
	gboolean r;

	g_return_val_if_fail (query, FALSE);
	g_return_val_if_fail (sql, FALSE);

	va_start (ap, query);
	r = xmms_sqlite_query_array_va (sql, xmms_sqlite_int_cb, out, query, ap);
	va_end (ap);

	return r;
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
	        sqlite3_libversion_number ());
}

/* Return an escaped string */
gchar *
sqlite_prepare_string (const gchar *input) {
	gchar *q, *str;
	q = sqlite3_mprintf ("%Q", input);
	str = g_strdup (q);
	sqlite3_free (q);
	return str;
}

/** @} */
