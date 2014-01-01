/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
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
#include <sqlite3.h>
#include <string.h>
#include <glib.h>

#include <xmms/xmms_config.h>
#include <xmmspriv/xmms_statfs.h>
#include <xmmspriv/xmms_utils.h>
#include <xmmspriv/xmms_collection.h>
#include <xmmsc/xmmsc_idnumbers.h>

/* increment this whenever there are incompatible db structure changes */
#define DB_VERSION 36

#define _STRINGIFY(x) #x

const char set_version_stm[] = "PRAGMA user_version=" _STRINGIFY(DB_VERSION);

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

/**
 * A query that can't retrieve results
 */
static gboolean
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
			printf ("BUSY EVENT!");
			g_assert_not_reached ();
	}
	if (ret != SQLITE_OK) {
			printf ("Error in query! \"%s\" (%d) - %s", q, ret, err);
			sqlite3_free (q);
			va_end (ap);
			return FALSE;
	}

	sqlite3_free (q);
	va_end (ap);

	return TRUE;
}

static void
upgrade_v26_to_v27 (sqlite3 *sql)
{
	printf ("Upgrade v26->v27");
	sqlite3_exec (sql,
	              "drop view albums;"
	              "drop view artists;"
	              "drop view compilations;"
	              "drop view songs;",
	              NULL, NULL, NULL);

	printf ("done");
}

static void
upgrade_v27_to_v28 (sqlite3 *sql)
{
	printf ("Upgrade v27->v28");

	sqlite3_exec (sql,
	              "drop table Log;",
	              NULL, NULL, NULL);

	printf ("done");
}

static void
upgrade_v28_to_v29 (sqlite3 *sql)
{
	printf ("Upgrade v28->v29");

	sqlite3_exec (sql, "delete from Media where source in"
	              "(select id from Sources where source like 'plugin%')",
	              NULL, NULL, NULL);
	sqlite3_exec (sql, "delete from Sources where source like 'plugin%'",
	              NULL, NULL, NULL);
	sqlite3_exec (sql, "update Media set value=0 where key='resolved'",
	              NULL, NULL, NULL);

	printf ("done");
}

static void
upgrade_v29_to_v30 (sqlite3 *sql)
{
	printf ("Upgrade v29->v30");
	sqlite3_exec (sql, "insert into Media (id, key, value, source) select distinct id, 'available', 1, (select id from Sources where source='server') from Media", NULL, NULL, NULL);
	printf ("done");
}

static void
upgrade_v30_to_v31 (sqlite3 *sql)
{
	printf ("Upgrade v30->v31");

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

	printf ("done");
}

static void
upgrade_v31_to_v32 (sqlite3 *sql)
{
	printf ("upgrade v31->v32");
	sqlite3_exec (sql, "delete from Media where id = (select id from Media where key='available' and value=0)", NULL, NULL, NULL);
	sqlite3_exec (sql, "delete from Media where key='available' and source = 1", NULL, NULL, NULL);
	sqlite3_exec (sql, "update media set key='status' where key='resolved' and source = 1", NULL, NULL, NULL);
	printf ("done");
}

static void
upgrade_v32_to_v33 (sqlite3 *sql)
{
	/* Decrement collection type id, as we removed ERROR from the enum. */
	printf ("upgrade v32->v33");
	sqlite3_exec (sql, "update CollectionOperators set type=type - 1", NULL, NULL, NULL);
	printf ("done");
}

static void
upgrade_v33_to_v34 (sqlite3 *sql)
{
	printf ("upgrade v33->v34");
	sqlite3_exec (sql, "update CollectionAttributes set value=replace(replace(value, '%', '*'), '_', '?') WHERE collid IN (SELECT id FROM CollectionOperators WHERE type='6')", NULL, NULL, NULL);
	printf ("done");
}


static void
upgrade_v34_to_v35 (sqlite3 *sql)
{
	printf ("upgrade v34->v35");
	sqlite3_exec (sql, "DROP INDEX prop_idx;"
	                   "CREATE INDEX id_key_value_1x ON Media (id, key, value COLLATE BINARY);"
	                   "CREATE INDEX id_key_value_2x ON Media (id, key, value COLLATE NOCASE);"
	                   "CREATE INDEX key_value_1x ON Media (key, value COLLATE BINARY);"
	                   "CREATE INDEX key_value_2x ON Media (key, value COLLATE NOCASE);"
	                   "UPDATE CollectionAttributes SET value=replace(replace(value, '%', '*'), '_', '?') WHERE collid IN (SELECT id FROM CollectionOperators WHERE type='6');", NULL, NULL, NULL);
	printf ("done");
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
	printf ("upgrade v35->v36 (save integers as strings also)");

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

	printf ("done");
}

/* XXX Prevent "missing-prototype" warning */
gboolean try_upgrade (sqlite3 *sql);

gboolean
try_upgrade (sqlite3 *sql)
{
	gint version;
	gboolean can_upgrade = TRUE;
	sqlite3_exec (sql, "PRAGMA user_version",
			xmms_sqlite_version_cb, &version, NULL);

	if (version == DB_VERSION)
		return TRUE;

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

