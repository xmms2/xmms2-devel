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

#include "xmms_configuration.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmmspriv/xmms_xform.h"
#include "xmmspriv/xmms_utils.h"
#include "xmms/xmms_error.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_object.h"
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_log.h"


#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <time.h>

#include "xmmspriv/xmms_fetch_info.h"
#include "xmmspriv/xmms_fetch_spec.h"
#include "s4.h"


/**
 * @file
 * Medialib is a metainfo cache that is searchable.
 */


static void xmms_medialib_client_remove_entry (xmms_medialib_t *medialib, gint32 entry, xmms_error_t *error);
gchar *xmms_medialib_url_encode (const gchar *path);

static void xmms_medialib_client_add_entry (xmms_medialib_t *, const gchar *, xmms_error_t *);
static void xmms_medialib_client_move_entry (xmms_medialib_t *, gint32 entry, const gchar *, xmms_error_t *);
static void xmms_medialib_client_import_path (xmms_medialib_t *medialib, const gchar *path, xmms_error_t *error);
static void xmms_medialib_client_rehash (xmms_medialib_t *medialib, gint32 id, xmms_error_t *error);
static void xmms_medialib_client_set_property_string (xmms_medialib_t *medialib, gint32 entry, const gchar *source, const gchar *key, const gchar *value, xmms_error_t *error);
static void xmms_medialib_client_set_property_int (xmms_medialib_t *medialib, gint32 entry, const gchar *source, const gchar *key, gint32 value, xmms_error_t *error);
static void xmms_medialib_client_remove_property (xmms_medialib_t *medialib, gint32 entry, const gchar *source, const gchar *key, xmms_error_t *error);
static GTree *xmms_medialib_client_get_info (xmms_medialib_t *medialib, gint32 id, xmms_error_t *err);
static gint32 xmms_medialib_client_get_id (xmms_medialib_t *medialib, const gchar *url, xmms_error_t *error);

static s4_t *xmms_medialib_database_open (const gchar *config_path, const gchar *indices[]);
static xmms_medialib_entry_t xmms_medialib_entry_new_insert (xmms_medialib_session_t *session, guint32 id, const gchar *url, xmms_error_t *error);

#include "medialib_ipc.c"

#define SESSION(x) MEDIALIB_SESSION(medialib, x)

/**
 *
 * @defgroup Medialib Medialib
 * @ingroup XMMSServer
 * @brief Medialib caches metadata
 *
 * Controls metadata storage.
 *
 * @{
 */

/**
 * Medialib structure
 */
struct xmms_medialib_St {
	xmms_object_t object;

	s4_t *s4;
	/** The current playlist */
	xmms_playlist_t *playlist;
	s4_sourcepref_t *default_sp;
};

struct xmms_medialib_session_St {
	xmms_medialib_t *medialib;
	s4_transaction_t *trans;
	xmmsv_t *changed;
	xmmsv_t *added;
	xmmsv_t *vals;
};


/**
  * Ok, so the functions are written with reentrency in mind, but
  * we choose to have a global medialib object here. It will be
  * much easier, and I don't see the real use of multiple medialibs
  * right now. This could be changed by removing this global one
  * and altering the function callers...
  */
static xmms_medialib_t *medialib;

static const gchar *source_pref[] = {
	"server",
	"client/*",
	"plugin/playlist",
	"plugin/id3v2",
	"plugin/segment",
	"plugin/*",
	"*",
	NULL
};


/**
 * Trigger a update signal to the client. This should be called
 * when important information in the entry has been changed and
 * should be visible to the user.
 *
 * @param entry Entry to signal a update for.
 */

static void
xmms_medialib_entry_send_update (xmms_medialib_t *medialib, xmms_medialib_entry_t entry)
{
	xmms_object_emit_f (XMMS_OBJECT (medialib),
	                    XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE,
	                    XMMSV_TYPE_INT32, entry);
}

/**
 * Trigger an added siginal to the client. This should be
 * called when a new entry has been added to the medialib
 *
 * @param entry Entry to signal an add for.
 */
static void
xmms_medialib_entry_send_added (xmms_medialib_t *medialib, xmms_medialib_entry_t entry)
{
	xmms_object_emit_f (XMMS_OBJECT (medialib),
	                    XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_ADDED,
	                    XMMSV_TYPE_INT32, entry);
}

xmms_medialib_session_t *
xmms_medialib_begin (xmms_medialib_t *mlib)
{
	xmms_medialib_session_t *ret = malloc (sizeof (xmms_medialib_session_t));

	ret->medialib = medialib;
	ret->trans = s4_begin (medialib->s4, 0);
	ret->changed = xmmsv_new_list ();
	ret->added = xmmsv_new_list ();
	ret->vals = xmmsv_new_list ();

	return ret;
}

static void
xmms_medialib_session_free (xmms_medialib_session_t *session)
{
	xmmsv_unref (session->changed);
	xmmsv_unref (session->added);
	xmmsv_unref (session->vals);

	g_free (session);
}

static void
xmms_medialib_session_free_full (xmms_medialib_session_t *session)
{
	xmmsv_t *val;
	gint i;

	for (i = 0; xmmsv_list_get (session->vals, i, &val); i++)
		xmmsv_unref (val);

	xmms_medialib_session_free (session);
}


gboolean
xmms_medialib_commit (xmms_medialib_session_t *session)
{
	gboolean ret;
	gint32 i, ival;

	ret = s4_commit (session->trans);
	if (!ret) {
		xmms_medialib_session_free_full (session);
		return FALSE;
	}

	for (i = 0; xmmsv_list_get_int (session->changed, i, &ival); i++) {
		xmms_medialib_entry_send_update (session->medialib, ival);
	}

	for (i = 0; xmmsv_list_get_int (session->added, i, &ival); i++) {
		xmms_medialib_entry_send_added (session->medialib, ival);
	}

	xmms_medialib_session_free (session);

	return TRUE;
}

void
xmms_medialib_abort (xmms_medialib_session_t *session)
{
	s4_abort (session->trans);
	xmms_medialib_session_free_full (session);
}

static void
xmms_medialib_destroy (xmms_object_t *object)
{
	xmms_medialib_t *mlib = (xmms_medialib_t *) object;

	s4_sourcepref_unref (mlib->default_sp);
	s4_close (mlib->s4);

	xmms_medialib_unregister_ipc_commands ();
}

#define XMMS_MEDIALIB_SOURCE_SERVER "server"

/**
 * Initialize the medialib and open the database file.
 *
 * @param playlist the current playlist pointer
 * @returns TRUE if successful and FALSE if there was a problem
 */

xmms_medialib_t *
xmms_medialib_init (xmms_playlist_t *playlist)
{
	xmms_config_property_t *cfg;
	const gchar *indices[] = {
		XMMS_MEDIALIB_ENTRY_PROPERTY_URL,
		XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS,
		NULL
	};
	const gchar *medialib_path;
	gchar *path;

	medialib = xmms_object_new (xmms_medialib_t, xmms_medialib_destroy);
	medialib->playlist = playlist;

	xmms_medialib_register_ipc_commands (XMMS_OBJECT (medialib));

	path = XMMS_BUILD_PATH ("medialib.s4");
	cfg = xmms_config_property_register ("medialib.path", path, NULL, NULL);
	g_free (path);

	path = XMMS_BUILD_PATH ("collections", "${uuid}");
	xmms_config_property_register ("collection.directory", path, NULL, NULL);
	g_free (path);

	xmms_config_property_register ("sqlite2s4.path", "sqlite2s4", NULL, NULL);

	medialib_path = xmms_config_property_get_string (cfg);
	medialib->s4 = xmms_medialib_database_open (medialib_path, indices);
	medialib->default_sp = s4_sourcepref_create (source_pref);

	return medialib;
}

/**
 * Extracts the file name of the old media library
 * and replaces its suffix with .s4
 */
static gchar *
xmms_medialib_database_converted_name (const gchar *conf_path)
{
	gchar *filename, *dirname, *dot, *converted_name, *fullpath;

	dirname = g_path_get_dirname (conf_path);
	filename = g_path_get_basename (conf_path);

	/* nuke the suffix */
	dot = g_strrstr (filename, ".");
	if (dot != NULL) {
		*dot = '\0';
	}

	converted_name = g_strconcat (filename, ".s4", NULL);

	fullpath = g_build_path (G_DIR_SEPARATOR_S, dirname,
	                         converted_name, NULL);

	g_free (converted_name);
	g_free (filename);
	g_free (dirname);

	return fullpath;
}

static s4_t *
xmms_medialib_database_convert (const gchar *database_name,
                                const gchar *indices[])
{
	const gchar *coll_conf, *conv_conf;
	gchar *cmdline, *new_name, *obsolete_name;
	xmms_config_property_t *cfg;
	gint exit_status;
	s4_t *s4;

	cfg = xmms_config_lookup ("collection.directory");
	coll_conf = xmms_config_property_get_string (cfg);

	cfg = xmms_config_lookup ("sqlite2s4.path");
	conv_conf = xmms_config_property_get_string (cfg);

	new_name = xmms_medialib_database_converted_name (database_name);

	cmdline = g_strjoin (" ", conv_conf, database_name,
	                     new_name, coll_conf, NULL);

	xmms_log_info ("Attempting to migrate database to new format.");

	if (!g_spawn_command_line_sync (cmdline, NULL, NULL, &exit_status, NULL) || exit_status) {
		xmms_log_fatal ("Could not run \"%s\", try to run it manually", cmdline);
	}

	g_free (cmdline);

	s4 = s4_open (new_name, indices, 0);
	/* Now we give up */
	if (s4 == NULL) {
		xmms_log_fatal ("Could not open the S4 database");
	}

	xmms_log_info ("Migration successful.");

	/* Move the sqlite database */
	obsolete_name = g_strconcat (database_name, ".obsolete", NULL);
	g_rename (database_name, obsolete_name);
	g_free (obsolete_name);

	/* Update the config path */
	cfg = xmms_config_lookup ("medialib.path");
	xmms_config_property_set_data (cfg, new_name);

	g_free (new_name);

	return s4;
}

static s4_t *
xmms_medialib_database_open (const gchar *database_name,
                             const gchar *indices[])
{
	s4_t *s4;

	s4 = s4_open (database_name, indices, 0);
	if (s4 != NULL) {
		return s4;
	}

	if (s4_errno () != S4E_MAGIC) {
		/* The database was a S4 database, but still couldn't be opened. */
		xmms_log_fatal ("Could not open the S4 database");
	}

	/* Seems like we've found a SQLite database, lets convert it. */
	return xmms_medialib_database_convert (database_name, indices);
}

static s4_sourcepref_t *
xmms_medialib_get_source_preference (xmms_medialib_session_t *session)
{
	return session->medialib->default_sp;
}

char *
xmms_medialib_uuid (xmms_medialib_t *mlib)
{
	return s4_get_uuid_string (medialib->s4);
}

static s4_resultset_t *
xmms_medialib_filter (xmms_medialib_session_t *session,
                      const gchar *filter_key, const s4_val_t *filter_val,
                      gint filter_flags, s4_sourcepref_t *sourcepref,
                      const gchar *fetch_key, gint fetch_flags)
{
	s4_condition_t *cond;
	s4_fetchspec_t *spec;
	s4_resultset_t *ret;

	cond = s4_cond_new_filter (S4_FILTER_EQUAL, filter_key, filter_val,
	                           sourcepref, S4_CMP_CASELESS, filter_flags);

	spec = s4_fetchspec_create ();
	s4_fetchspec_add (spec, fetch_key, sourcepref, fetch_flags);
	ret = s4_query (NULL, session->trans, spec, cond);

	s4_cond_free (cond);
	s4_fetchspec_free (spec);

	return ret;
}

static s4_val_t *
xmms_medialib_entry_property_get (xmms_medialib_session_t *session,
                                  xmms_medialib_entry_t entry,
                                  const gchar *property)
{
	s4_sourcepref_t *sourcepref;
	const s4_result_t *res;
	s4_resultset_t *set;
	s4_val_t *ret = NULL;
	s4_val_t *song_id;

	g_return_val_if_fail (property, NULL);

	song_id = s4_val_new_int (entry);

	if (strcmp (property, XMMS_MEDIALIB_ENTRY_PROPERTY_ID) == 0) {
		/* only resolving attributes other than 'id' */
		return song_id;
	}

	sourcepref = xmms_medialib_get_source_preference (session);

	set = xmms_medialib_filter (session, "song_id", song_id, S4_COND_PARENT,
	                            sourcepref, property, S4_FETCH_DATA);

	res = s4_resultset_get_result (set, 0, 0);
	if (res != NULL) {
		ret = s4_val_copy (s4_result_get_val (res));
	}

	s4_resultset_free (set);
	s4_val_free (song_id);

	return ret;
}


/**
 * Retrieve a property from an entry
 *
 * @see xmms_medialib_entry_property_get_str
 */

xmmsv_t *
xmms_medialib_entry_property_get_value (xmms_medialib_session_t *session,
                                        xmms_medialib_entry_t id_num,
                                        const gchar *property)
{
	xmmsv_t *ret = NULL;
	s4_val_t *prop;
	const gchar *s;
	gint32 i;

	prop = xmms_medialib_entry_property_get (session, id_num, property);
	if (prop == NULL)
		return NULL;

	if (s4_val_get_str (prop, &s)) {
		ret = xmmsv_new_string (s);
	} else if (s4_val_get_int (prop, &i)) {
		ret = xmmsv_new_int (i);
	}

	s4_val_free (prop);

	return ret;
}

/**
 * Retrieve a property from an entry.
 *
 * @param id_num Entry to query.
 * @param property The property to extract. Strings passed should
 * be defined in medialib.h
 *
 * @returns Newly allocated gchar that needs to be freed with g_free
 */

gchar *
xmms_medialib_entry_property_get_str (xmms_medialib_session_t *session,
                                      xmms_medialib_entry_t id_num,
                                      const gchar *property)
{
	gchar *ret = NULL;
	s4_val_t *prop;
	const gchar *s;
	gint32 i;

	prop = xmms_medialib_entry_property_get (session, id_num, property);
	if (prop == NULL)
		return NULL;

	if (s4_val_get_int (prop, &i)) {
		ret = g_strdup_printf ("%i", i);
	} else if (s4_val_get_str (prop, &s)) {
		ret = g_strdup (s);
	}

	s4_val_free (prop);

	return ret;
}

/**
 * Retrieve a property as a int from a entry.
 *
 * @param id_num Entry to query.
 * @param property The property to extract. Strings passed should
 * be defined in medialib.h
 *
 * @returns Property as integer, or -1 if it doesn't exist.
 */
gint
xmms_medialib_entry_property_get_int (xmms_medialib_session_t *session,
                                      xmms_medialib_entry_t id_num,
                                      const gchar *property)
{
	gint32 ret;
	s4_val_t *prop;

	prop = xmms_medialib_entry_property_get (session, id_num, property);
	if (prop == NULL) {
		return -1;
	}

	if (!s4_val_get_int (prop, &ret)) {
		ret = -1;
	}

	s4_val_free (prop);

	return ret;
}

static gboolean
xmms_medialib_entry_property_set_source (xmms_medialib_session_t *session,
                                         xmms_medialib_entry_t entry,
                                         const gchar *key, s4_val_t *new_prop,
                                         const gchar *source)
{
	const gchar *sources[2] = { source, NULL };
	s4_sourcepref_t *sp;
	s4_resultset_t *set;
	s4_val_t *song_id;
	const s4_result_t *res;

	song_id = s4_val_new_int (entry);
	sp = s4_sourcepref_create (sources);

	set = xmms_medialib_filter (session, "song_id", song_id,
	                            S4_COND_PARENT, sp, key, S4_FETCH_DATA);

	res = s4_resultset_get_result (set, 0, 0);
	if (res != NULL) {
		s4_del (NULL, session->trans, "song_id", song_id,
		        key, s4_result_get_val (res), source);
	}

	s4_resultset_free (set);
	s4_sourcepref_unref (sp);

	s4_add (NULL, session->trans, "song_id", song_id, key, new_prop, source);
	s4_val_free (song_id);

	xmmsv_list_append_int (session->changed, entry);

	return TRUE;
}

/**
 * Set a entry property to a new value, overwriting the old value.
 *
 * @param entry Entry to alter.
 * @param property The property to extract. Strings passed should
 * be defined in medialib.h
 * @param value gint with the new value, will be copied in to the medialib
 *
 * @returns TRUE on success and FALSE on failure.
 */
gboolean
xmms_medialib_entry_property_set_int (xmms_medialib_session_t *session,
                                      xmms_medialib_entry_t entry,
                                      const gchar *property, gint value)
{
	return xmms_medialib_entry_property_set_int_source (session,
	                                                    entry,
	                                                    property, value,
	                                                    "server");
}


gboolean
xmms_medialib_entry_property_set_int_source (xmms_medialib_session_t *session,
                                             xmms_medialib_entry_t id_num,
                                             const gchar *property, gint value,
                                             const gchar *source)
{
	gboolean ret;
	s4_val_t *prop;

	g_return_val_if_fail (property, FALSE);

	prop = s4_val_new_int (value);
	ret = xmms_medialib_entry_property_set_source (session, id_num, property, prop, source);

	s4_val_free (prop);
	return ret;
}

/**
 * Set a entry property to a new value, overwriting the old value.
 *
 * @param entry Entry to alter.
 * @param property The property to extract. Strings passed should
 * be defined in medialib.h
 * @param value gchar with the new value, will be copied in to the medialib
 *
 * @returns TRUE on success and FALSE on failure.
 */
gboolean
xmms_medialib_entry_property_set_str (xmms_medialib_session_t *session,
                                      xmms_medialib_entry_t entry,
                                      const gchar *property, const gchar *value)
{
	return xmms_medialib_entry_property_set_str_source (session,
	                                                    entry,
	                                                    property, value,
	                                                    "server");
}


gboolean
xmms_medialib_entry_property_set_str_source (xmms_medialib_session_t *session,
                                             xmms_medialib_entry_t id_num,
                                             const gchar *property, const gchar *value,
                                             const gchar *source)
{
	gboolean ret;
	s4_val_t *prop;
	gint ival;

	g_return_val_if_fail (property, FALSE);

	if (value && !g_utf8_validate (value, -1, NULL)) {
		XMMS_DBG ("OOOOOPS! Trying to set property %s to a NON UTF-8 string (%s) I will deny that!", property, value);
		return FALSE;
	}

	if (xmms_is_int (value, &ival)) {
		prop = s4_val_new_int (ival);
	} else {
		prop = s4_val_new_string (value);
	}

	ret = xmms_medialib_entry_property_set_source (session, id_num, property, prop, source);

	s4_val_free (prop);

	return ret;

}


/* A filter to find the highest id. It always returns -1, thus the
 * search algorithm will ultimately end up passing it the highest value.
 */
static gint
highest_id_filter (const s4_val_t *value, s4_condition_t *cond)
{
	gint32 *i = s4_cond_get_funcdata (cond);
	gint32 ival;

	if (s4_val_get_int (value, &ival)) {
		*i = ival;
	}

	return -1;
}

/**
 * Return a fresh unused medialib id.
 */
static int32_t
xmms_medialib_get_new_id (xmms_medialib_session_t *session)
{
	gint32 highest = -1;
	s4_fetchspec_t *fs;
	s4_condition_t *cond;
	s4_resultset_t *set;
	s4_sourcepref_t *sourcepref;

	sourcepref = xmms_medialib_get_source_preference (session);

	fs = s4_fetchspec_create ();
	cond = s4_cond_new_custom_filter (highest_id_filter, &highest, NULL,
	                                  "song_id", sourcepref, 0, 1, S4_COND_PARENT);
	set = s4_query (NULL, session->trans, fs, cond);

	s4_resultset_free (set);
	s4_cond_free (cond);
	s4_fetchspec_free (fs);

	return highest + 1;
}


static void
xmms_medialib_client_remove_entry (xmms_medialib_t *medialib,
                                   gint32 entry, xmms_error_t *error)
{
	SESSION (xmms_medialib_entry_remove (session, entry));
}

/**
 * Remove a medialib entry from the database
 *
 * @param id_num Entry to remove
 */
void
xmms_medialib_entry_remove (xmms_medialib_session_t *session,
                            xmms_medialib_entry_t entry)
{
	s4_resultset_t *set;
	s4_val_t *song_id;
	gint i;

	song_id = s4_val_new_int (entry);

	set = xmms_medialib_filter (session, "song_id", song_id,
	                            S4_COND_PARENT, NULL, NULL, S4_FETCH_DATA);

	for (i = 0; i < s4_resultset_get_rowcount (set); i++) {
		const s4_result_t *res;

		res = s4_resultset_get_result (set, i, 0);
		while (res != NULL) {
			s4_del (NULL, session->trans, "song_id", song_id,
			        s4_result_get_key (res),
			        s4_result_get_val (res),
			        s4_result_get_src (res));

			res = s4_result_next (res);
		}
	}

	s4_resultset_free (set);
	s4_val_free (song_id);

	/** @todo safe ? */
	xmms_playlist_remove_by_entry (session->medialib->playlist, entry);
}


static void
process_file (xmms_medialib_session_t *session,
              const gchar *playlist,
              gint32 pos,
              const gchar *path,
              xmms_error_t *error)
{
	xmms_medialib_entry_t entry;

	entry = xmms_medialib_entry_new_encoded (session, path, error);

	if (entry && playlist != NULL) {
		if (pos >= 0) {
			xmms_playlist_insert_entry (medialib->playlist,
			                            playlist, pos, entry, error);
		} else {
			xmms_playlist_add_entry (medialib->playlist,
			                         playlist, entry, error);
		}
	}
}

static gint
cmp_val (gconstpointer a, gconstpointer b)
{
	xmmsv_t *v1, *v2;
	const gchar *s1, *s2;
	v1 = (xmmsv_t *) a;
	v2 = (xmmsv_t *) b;
	if (xmmsv_get_type (v1) != XMMSV_TYPE_DICT)
		return 0;
	if (xmmsv_get_type (v2) != XMMSV_TYPE_DICT)
		return 0;

	xmmsv_dict_entry_get_string (v1, "path", &s1);
	xmmsv_dict_entry_get_string (v2, "path", &s2);

	return strcmp (s1, s2);
}

/* code ported over from CLI's "radd" command. */
/* note that the returned file list is reverse-sorted! */
static gboolean
process_dir (const gchar *directory,
             GList **ret,
             xmms_error_t *error)
{
	GList *list;

	list = xmms_xform_browse (directory, error);
	if (!list) {
		return FALSE;
	}

	list = g_list_sort (list, cmp_val);

	while (list) {
		xmmsv_t *val = list->data;
		const gchar *str;
		gint isdir;

		xmmsv_dict_entry_get_string (val, "path", &str);
		xmmsv_dict_entry_get_int (val, "isdir", &isdir);

		if (isdir == 1) {
			process_dir (str, ret, error);
		} else {
			*ret = g_list_prepend (*ret, g_strdup (str));
		}

		xmmsv_unref (val);
		list = g_list_delete_link (list, list);
	}

	return TRUE;
}

void
xmms_medialib_entry_cleanup (xmms_medialib_session_t *session,
                             xmms_medialib_entry_t entry)
{
	s4_resultset_t *set;
	s4_val_t *song_id;
	gint i;

	song_id = s4_val_new_int (entry);

	set = xmms_medialib_filter (session, "song_id", song_id,
	                            S4_COND_PARENT, NULL, NULL, S4_FETCH_DATA);


	for (i = 0; i < s4_resultset_get_rowcount (set); i++) {
		const s4_result_t *res;

		res = s4_resultset_get_result (set, i, 0);
		while (res != NULL) {
			const gchar *src = s4_result_get_src (res);
			const gchar *key = s4_result_get_key (res);

			if (strcmp (XMMS_MEDIALIB_SOURCE_SERVER, src) == 0) {
				if (strcmp (key, XMMS_MEDIALIB_ENTRY_PROPERTY_URL) &&
				    strcmp (key, XMMS_MEDIALIB_ENTRY_PROPERTY_ADDED) &&
				    strcmp (key, XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS) &&
				    strcmp (key, XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD) &&
				    strcmp (key, XMMS_MEDIALIB_ENTRY_PROPERTY_LASTSTARTED)) {
					s4_del (NULL, session->trans, "song_id", song_id,
					        key, s4_result_get_val (res), src);
				}
			} else if (strncmp (src, "plugin/", 7) == 0 &&
			           strcmp (src, "plugin/playlist") != 0) {
				s4_del (NULL, session->trans, "song_id", song_id,
				        key, s4_result_get_val (res), src);
			}

			res = s4_result_next (res);
		}
	}

	s4_resultset_free (set);
	s4_val_free (song_id);
}

static void
xmms_medialib_client_rehash (xmms_medialib_t *medialib, gint32 id, xmms_error_t *error)
{
	xmms_mediainfo_reader_t *mr;

	MEDIALIB_BEGIN (medialib);
	if (id) {
		xmms_medialib_entry_status_set (session, id, XMMS_MEDIALIB_ENTRY_STATUS_REHASH);
	} else {
		s4_sourcepref_t *sourcepref;
		s4_resultset_t *set;
		s4_val_t *status;
		gint i;

		sourcepref = xmms_medialib_get_source_preference (session);

		status = s4_val_new_int (XMMS_MEDIALIB_ENTRY_STATUS_OK);
		set = xmms_medialib_filter (session, XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS,
		                            status, 0, sourcepref, "song_id", S4_FETCH_PARENT);
		s4_val_free (status);

		for (i = 0; i < s4_resultset_get_rowcount (set); i++) {
			const s4_result_t *res;

			res = s4_resultset_get_result (set, i, 0);
			for (; res != NULL; res = s4_result_next (res)) {
				s4_val_get_int (s4_result_get_val (res), &id);
				xmms_medialib_entry_status_set (session, id, XMMS_MEDIALIB_ENTRY_STATUS_REHASH);
			}
		}

		s4_resultset_free (set);
	}
	MEDIALIB_COMMIT ();

	mr = xmms_playlist_mediainfo_reader_get (medialib->playlist);
	xmms_mediainfo_reader_wakeup (mr);

}

/* Recursively add entries under the given path to the medialib,
 * optionally adding them to a playlist if the playlist argument is
 * not NULL.
 */
void
xmms_medialib_add_recursive (xmms_medialib_t *medialib, const gchar *playlist,
                             const gchar *path, xmms_error_t *error)
{
	/* Just called insert with negative pos to append */
	xmms_medialib_insert_recursive (medialib, playlist, -1, path, error);
}

/* Recursively adding entries under the given path to the medialib,
 * optionally insert them into a playlist at a given position if the
 * playlist argument is not NULL. If the position is negative, entries
 * are appended to the playlist.
 */
void
xmms_medialib_insert_recursive (xmms_medialib_t *medialib, const gchar *playlist,
                                gint32 pos, const gchar *path,
                                xmms_error_t *error)
{
	GList *first, *list = NULL, *n;

	g_return_if_fail (medialib);
	g_return_if_fail (path);

	/* Allocate our first list node manually here. The following call
	 * to process_dir() will prepend all other nodes, so afterwards
	 * "first" will point to the last node of the list... see below.
	 */
	first = list = g_list_alloc ();

	process_dir (path, &list, error);

	XMMS_DBG ("taking the transaction!");

	/* We now want to iterate the list in the order in which the nodes
	 * were added, ie in reverse order. Thankfully we stored a pointer
	 * to the last node in the list before, which saves us an expensive
	 * g_list_last() call now. Increase pos each time to retain order.
	 */
	for (n = first->prev; n; n = g_list_previous (n)) {
		SESSION (process_file (session, playlist, pos, n->data, error));
		if (pos >= 0)
			pos++;
		g_free (n->data);
	}

	g_list_free (list);

	XMMS_DBG ("and we are done!");
}

static void
xmms_medialib_client_import_path (xmms_medialib_t *medialib, const gchar *path,
                                  xmms_error_t *error)
{
	xmms_medialib_add_recursive (medialib, NULL, path, error);
}

static xmms_medialib_entry_t
xmms_medialib_entry_new_insert (xmms_medialib_session_t *session,
                                guint32 id,
                                const char *url,
                                xmms_error_t *error)
{
	xmms_mediainfo_reader_t *mr;

	if (!xmms_medialib_entry_property_set_str (session, id, XMMS_MEDIALIB_ENTRY_PROPERTY_URL, url)) {
		return 0;
	}

	xmms_medialib_entry_status_set (session, id, XMMS_MEDIALIB_ENTRY_STATUS_NEW);
	mr = xmms_playlist_mediainfo_reader_get (session->medialib->playlist);
	xmms_mediainfo_reader_wakeup (mr);

	return 1;

}

/**
 * @internal
 */
static xmms_medialib_entry_t
xmms_medialib_get_id (xmms_medialib_session_t *session, const char *url, xmms_error_t *error)
{
	s4_sourcepref_t *sourcepref;
	s4_resultset_t *set;
	s4_val_t *value;
	const s4_result_t *res;
	gint32 id = 0;

	sourcepref = xmms_medialib_get_source_preference (session);

	value = s4_val_new_string (url);
	set = xmms_medialib_filter (session, XMMS_MEDIALIB_ENTRY_PROPERTY_URL,
	                            value, 0, sourcepref, "song_id", S4_FETCH_PARENT);
	s4_val_free (value);

	res = s4_resultset_get_result (set, 0, 0);
	if (res != NULL) {
		s4_val_get_int (s4_result_get_val (res), &id);
	}

	s4_resultset_free (set);

	return id;
}

xmms_medialib_entry_t
xmms_medialib_entry_new_encoded (xmms_medialib_session_t *session,
                                 const gchar *url, xmms_error_t *error)
{
	guint ret = 0;

	g_return_val_if_fail (url, 0);

	ret = xmms_medialib_get_id (session, url, error);

	if (ret == 0) {
		ret = xmms_medialib_get_new_id (session);

		if (!xmms_medialib_entry_new_insert (session, ret, url, error)) {
			return 0;
		}

		xmmsv_list_append_int (session->added, ret);
	}

	return ret;

}

/**
 * Welcome to a function that should be called something else.
 * Returns a entry for a URL, if the URL is already in the medialib
 * the current entry will be returned otherwise a new one will be
 * created and returned.
 *
 * @todo rename to something better?
 *
 * @param url URL to add/retrieve from the medialib
 * @param error If an error occurs, it will be stored in there.
 *
 * @returns Entry mapped to the URL
 */
xmms_medialib_entry_t
xmms_medialib_entry_new (xmms_medialib_session_t *session,
                         const char *url, xmms_error_t *error)
{
	gchar *enc_url;
	xmms_medialib_entry_t res;

	enc_url = xmms_medialib_url_encode (url);
	if (!enc_url) {
		return 0;
	}

	res = xmms_medialib_entry_new_encoded (session, enc_url, error);

	g_free (enc_url);

	return res;
}

gint32
xmms_medialib_client_get_id (xmms_medialib_t *medialib, const gchar *url,
                             xmms_error_t *error)
{
	gint32 ret;

	SESSION (ret = xmms_medialib_get_id (session, url, error));

	return ret;
}

static void
xmms_medialib_tree_add_tuple (GTree *tree, const char *key,
                              const char *source, xmmsv_t *value)
{
	xmmsv_t *keytreeval;

	if (key == NULL || source == NULL || value == NULL) {
		return;
	}

	/* Find (or insert) subtree matching the prop key */
	keytreeval = (xmmsv_t *) g_tree_lookup (tree, key);
	if (!keytreeval) {
		keytreeval = xmmsv_new_dict ();
		g_tree_insert (tree, g_strdup (key), keytreeval);
	}

	/* Replace (or insert) value matching the prop source */
	xmmsv_dict_set (keytreeval, source, value);
}

/**
 * Convert a entry and all properties to a key-source-value tree that
 * could be feed to the client or somewhere else in the daemon.
 *
 * @param session The medialib session to be used for the transaction.
 * @param entry Entry to convert.
 *
 * @returns Newly allocated tree with newly allocated strings
 * make sure to free them all.
 */

static GTree *
xmms_medialib_entry_to_tree (xmms_medialib_session_t *session,
                             xmms_medialib_entry_t entry)
{
	s4_resultset_t *set;
	s4_val_t *song_id;
	xmmsv_t *v_entry;
	GTree *ret;
	gint i;

	g_return_val_if_fail (entry, NULL);

	if (!xmms_medialib_check_id (session, entry)) {
		return NULL;
	}

	song_id = s4_val_new_int (entry);
	set = xmms_medialib_filter (session, "song_id", song_id, S4_COND_PARENT,
	                            NULL, NULL, S4_FETCH_PARENT | S4_FETCH_DATA);
	s4_val_free (song_id);

	ret = g_tree_new_full ((GCompareDataFunc) strcmp, NULL, g_free,
	                       (GDestroyNotify) xmmsv_unref);

	for (i = 0; i < s4_resultset_get_rowcount (set); i++) {
		const s4_result_t *res;

		res = s4_resultset_get_result (set, 0, 0);
		while (res != NULL) {
			const s4_val_t *val;
			const char *s;
			gint32 i;

			val = s4_result_get_val (res);
			if (s4_val_get_str (val, &s)) {
				v_entry = xmmsv_new_string (s);
			} else if (s4_val_get_int (val, &i)) {
				v_entry = xmmsv_new_int (i);
			}

			xmms_medialib_tree_add_tuple (ret, s4_result_get_key (res),
			                              s4_result_get_src (res), v_entry);
			xmmsv_unref (v_entry);

			res = s4_result_next (res);
		}
	}

	s4_resultset_free (set);
	v_entry = xmmsv_new_int (entry);
	xmms_medialib_tree_add_tuple (ret, "id", "server", v_entry);
	xmmsv_unref (v_entry);

	return ret;
}

static GTree *
xmms_medialib_client_get_info (xmms_medialib_t *medialib, gint32 id,
                               xmms_error_t *err)
{
	GTree *ret = NULL;

	if (!id) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "No such entry, 0");
	} else {
		SESSION (ret = xmms_medialib_entry_to_tree (session, id));

		if (!ret) {
			xmms_error_set (err, XMMS_ERROR_NOENT,
			                "Could not retrieve info for that entry!");
		}
	}

	return ret;
}

/**
 * Add a entry to the medialib. Calls #xmms_medialib_entry_new and then
 * wakes up the mediainfo_reader in order to resolve the metadata.
 *
 * @param medialib Medialib pointer
 * @param url URL to add
 * @param error In case of error this will be filled.
 */

static void
xmms_medialib_client_add_entry (xmms_medialib_t *medialib, const gchar *url,
                                xmms_error_t *error)
{
	g_return_if_fail (medialib);
	g_return_if_fail (url);

	SESSION (xmms_medialib_entry_new_encoded (session, url, error));
}

/**
 * Changes the URL of an entry in the medialib.
 *
 * @param medialib Medialib pointer
 * @param entry entry to modify
 * @param url URL to change to
 * @param error In case of error this will be filled.
 */
static void
xmms_medialib_client_move_entry (xmms_medialib_t *medialib, gint32 entry,
                                 const gchar *url, xmms_error_t *error)
{
	const gchar *key = XMMS_MEDIALIB_ENTRY_PROPERTY_URL;
	gchar *enc_url;

	enc_url = xmms_medialib_url_encode (url);

	SESSION (xmms_medialib_entry_property_set_str_source (session, entry, key,
	                                                      enc_url, "server"));

	g_free (enc_url);
}

static void
xmms_medialib_client_set_property_string (xmms_medialib_t *medialib,
                                          gint32 entry, const gchar *source,
                                          const gchar *key, const gchar *value,
                                          xmms_error_t *error)
{
	if (g_ascii_strcasecmp (source, "server") == 0) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Can't write to source server!");
		return;
	}

	SESSION (xmms_medialib_entry_property_set_str_source (session, entry, key,
	                                                      value, source));
}

static void
xmms_medialib_client_set_property_int (xmms_medialib_t *medialib, gint32 entry,
                                       const gchar *source, const gchar *key,
                                       gint32 value, xmms_error_t *error)
{
	if (g_ascii_strcasecmp (source, "server") == 0) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Can't write to source server!");
		return;
	}

	SESSION (xmms_medialib_entry_property_set_int_source (session, entry, key,
	                                                      value, source));
}

static void
xmms_medialib_property_remove (xmms_medialib_session_t *session, guint32 entry,
                               const gchar *source, const gchar *key,
                               xmms_error_t *error)
{
	const char *sources[2] = { source, NULL };
	s4_sourcepref_t *sp;
	s4_resultset_t *set;
	const s4_result_t *res;
	s4_val_t *song_id;

	sp = s4_sourcepref_create (sources);

	song_id = s4_val_new_int (entry);
	set = xmms_medialib_filter (session, "song_id", song_id,
	                            S4_COND_PARENT, sp, key, S4_FETCH_DATA);

	res = s4_resultset_get_result (set, 0, 0);
	if (res != NULL) {
		s4_del (NULL, session->trans, "song_id", song_id,
		        key,  s4_result_get_val (res), source);
	}

	s4_resultset_free (set);
	s4_sourcepref_unref (sp);
	s4_val_free (song_id);
}

static void
xmms_medialib_client_remove_property (xmms_medialib_t *medialib, gint32 entry,
                                      const gchar *source, const gchar *key,
                                      xmms_error_t *error)
{
	if (g_ascii_strcasecmp (source, "server") == 0) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Can't remove properties set by the server!");
		return;
	}

	SESSION (xmms_medialib_property_remove (session, entry, source, key, error));
}


/** @} */

/**
 * @internal
 */

gboolean
xmms_medialib_check_id (xmms_medialib_session_t *session,
                        xmms_medialib_entry_t id)
{
	xmmsv_t *val;

	val = xmms_medialib_entry_property_get_value (session, id, XMMS_MEDIALIB_ENTRY_PROPERTY_URL);
	if (val == NULL) {
		return FALSE;
	}

	xmmsv_unref (val);

	return TRUE;
}


/**
 * @internal
 * Get the next unresolved entry. Used by the mediainfo reader..
 */

static s4_resultset_t *
not_resolved_set (xmms_medialib_session_t *session)
{
	s4_condition_t *cond1, *cond2, *cond;
	s4_sourcepref_t *sourcepref;
	s4_fetchspec_t *spec;
	s4_resultset_t *ret;
	s4_val_t *v1, *v2;

	sourcepref = xmms_medialib_get_source_preference (session);

	v1 = s4_val_new_int (XMMS_MEDIALIB_ENTRY_STATUS_NEW);
	v2 = s4_val_new_int (XMMS_MEDIALIB_ENTRY_STATUS_REHASH);

	cond1 = s4_cond_new_filter (S4_FILTER_EQUAL,
	                            XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS,
	                            v1, sourcepref, S4_CMP_CASELESS, 0);
	cond2 = s4_cond_new_filter (S4_FILTER_EQUAL,
	                            XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS,
	                            v2, sourcepref, S4_CMP_CASELESS, 0);

	cond = s4_cond_new_combiner (S4_COMBINE_OR);
	s4_cond_add_operand (cond, cond1);
	s4_cond_add_operand (cond, cond2);

	spec = s4_fetchspec_create ();
	s4_fetchspec_add (spec, "song_id", sourcepref, S4_FETCH_PARENT);

	ret = s4_query (NULL, session->trans, spec, cond);

	s4_fetchspec_free (spec);
	s4_cond_free (cond);
	s4_cond_free (cond1);
	s4_cond_free (cond2);
	s4_val_free (v1);
	s4_val_free (v2);

	return ret;
}

xmms_medialib_entry_t
xmms_medialib_entry_not_resolved_get (xmms_medialib_session_t *session)
{
	const s4_result_t *res;
	s4_resultset_t *set;
	gint32 ret = 0;

	set = not_resolved_set (session);
	res = s4_resultset_get_result (set, 0, 0);

	if (res != NULL) {
		s4_val_get_int (s4_result_get_val (res), &ret);
	}

	s4_resultset_free (set);

	return ret;
}

guint
xmms_medialib_num_not_resolved (xmms_medialib_session_t *session)
{
	s4_resultset_t *set;
	gint ret = 0;

	set = not_resolved_set (session);
	ret = s4_resultset_get_rowcount (set);
	s4_resultset_free (set);

	return ret;
}


gboolean
xmms_medialib_decode_url (gchar *url)
{
	gint i = 0, j = 0;

	g_return_val_if_fail (url, TRUE);

	while (url[i]) {
		guchar chr = url[i++];

		if (chr == '+') {
			url[j++] = ' ';
		} else if (chr == '%') {
			char ts[3];
			char *t;

			ts[0] = url[i++];
			if (!ts[0])
				return FALSE;
			ts[1] = url[i++];
			if (!ts[1])
				return FALSE;
			ts[2] = '\0';

			url[j++] = strtoul (ts, &t, 16);
			if (t != &ts[2])
				return FALSE;
		} else {
			url[j++] = chr;
		}
	}

	url[j] = '\0';

	return TRUE;
}


#define GOODCHAR(a) ((((a) >= 'a') && ((a) <= 'z')) || \
                     (((a) >= 'A') && ((a) <= 'Z')) || \
                     (((a) >= '0') && ((a) <= '9')) || \
                     ((a) == ':') || \
                     ((a) == '/') || \
                     ((a) == '-') || \
                     ((a) == '.') || \
                     ((a) == '_'))

/* we don't share code here with medialib because we want to use g_malloc :( */
gchar *
xmms_medialib_url_encode (const gchar *path)
{
	static gchar hex[16] = "0123456789abcdef";
	gchar *res;
	gint i = 0, j = 0;

	res = g_malloc (strlen (path) * 3 + 1);
	if (!res) {
		return NULL;
	}

	while (path[i]) {
		guchar chr = path[i++];
		if (GOODCHAR (chr)) {
			res[j++] = chr;
		} else if (chr == ' ') {
			res[j++] = '+';
		} else {
			res[j++] = '%';
			res[j++] = hex[((chr & 0xf0) >> 4)];
			res[j++] = hex[(chr & 0x0f)];
		}
	}

	res[j] = '\0';

	return res;
}


/* A filter matching everything */
static gint
universe_filter (void)
{
	return 0;
}

/* A filter for idlists. Checks if the value given (id number)
 * is in the hash table
 */
static gint
idlist_filter (const s4_val_t *value, s4_condition_t *cond)
{
	GHashTable *id_table;
	gint32 ival;

	if (!s4_val_get_int (value, &ival)) {
		return 1;
	}

	id_table = s4_cond_get_funcdata (cond);

	return g_hash_table_lookup (id_table, GINT_TO_POINTER (ival)) == NULL;
}

/**
 * Creates a new resultset where the order is the same as in the idlist
 *
 * @param set The resultset to sort. It will be freed by this function
 * @param id_pos The position of the "id" column
 * @param idlist The idlist to order by
 * @return A new set with the same order as the idlist
 */
static s4_resultset_t *
xmms_medialib_result_sort_idlist (s4_resultset_t *set, xmmsv_t *idlist)
{
	const s4_resultrow_t *row;
	const s4_result_t *result;
	GHashTable *row_table;
	s4_resultset_t *ret;
	gint32 ival, i;

	row_table = g_hash_table_new (NULL, NULL);

	for (i = 0; s4_resultset_get_row (set, i, &row); i++) {
		if (s4_resultrow_get_col (row, 0, &result)
		    && s4_val_get_int (s4_result_get_val (result), &ival)) {
			g_hash_table_insert (row_table, GINT_TO_POINTER (ival), (void *) row);
		}
	}

	ret = s4_resultset_create (s4_resultset_get_colcount (set));

	for (i = 0; xmmsv_list_get_int (idlist, i, &ival); i++) {
		row = g_hash_table_lookup (row_table, GINT_TO_POINTER (ival));
		if (row != NULL) {
			s4_resultset_add_row (ret, row);
		}
	}

	g_hash_table_destroy (row_table);
	s4_resultset_free (set);

	return ret;
}

/**
 * Sorts a resultset
 *
 * @param set The resultset to sort
 * @param fetch The fetch-list used when set was created
 * @param order A list with orderings. An ordering can be a string
 * telling which column to sort by (prefixed by '-' to sort ascending)
 * or a list of integers (an idlist).
 * @return The set (or a new set) with the correct ordering
 */
static s4_resultset_t *
xmms_medialib_result_sort (s4_resultset_t *set, xmms_fetch_info_t *fetch_info, xmmsv_t *order)
{
	const gchar *str;
	gint i, j, stop;
	gint *s4_order;
	xmmsv_t *val;

	s4_order = g_new0 (int, xmmsv_list_get_size (order) + 1);

	/* Find the first idlist-order operand */
	for (i = 0; xmmsv_list_get (order, i, &val); i++) {
		if (xmmsv_is_type (val, XMMSV_TYPE_LIST)) {
			set = xmms_medialib_result_sort_idlist (set, val);
			break;
		}
	}
	/* We will only order by the operands before the idlist */
	stop = i;

	for (i = 0, j = 0; i < stop; i++) {
		if (xmmsv_list_get_string (order, i, &str)) {
			gint neg = (*str == '-')?1:0;
			str += neg;

			if (strcmp (str, "__ RANDOM __") == 0) {
				if (i == 0) {
					s4_resultset_shuffle (set);
				}
				break;
			} else if (strcmp (str, "__ ID __") == 0) {
				s4_order[j] = 1;
			} else {
				s4_order[j] = xmms_fetch_info_get_index (fetch_info, NULL, str) + 1;
			}

			if (neg) {
				s4_order[j] = -s4_order[j];
			}
			j++;
		}
	}
	s4_order[j] = 0;

	if (j > 0) {
		s4_resultset_sort (set, s4_order);
	}

	g_free (s4_order);

	return set;
}

/* Check if a collection is the universe
 * TODO: Move it to the xmmstypes lib?
 */
static gboolean
is_universe (xmmsv_coll_t *coll)
{
	gchar *target_name;
	gboolean ret = FALSE;

	switch (xmmsv_coll_get_type (coll)) {
		case XMMS_COLLECTION_TYPE_UNIVERSE:
			ret = TRUE;
			break;
		case XMMS_COLLECTION_TYPE_REFERENCE:
			if (xmmsv_coll_attribute_get (coll, "reference", &target_name)
			    && strcmp (target_name, "All Media") == 0)
				ret = TRUE;
			break;
		default:
			break;
	}

	return ret;
}

/* Returns non-zero if the collection has an ordering, 0 otherwise */
static gboolean
has_order (xmmsv_coll_t *coll)
{
	xmmsv_t *operands = xmmsv_coll_operands_get (coll);
	xmmsv_coll_t *c;
	gint i;

	switch (xmmsv_coll_get_type (coll)) {
		/* Filter keeps the ordering of the operand */
		case XMMS_COLLECTION_TYPE_HAS:
		case XMMS_COLLECTION_TYPE_MATCH:
		case XMMS_COLLECTION_TYPE_TOKEN:
		case XMMS_COLLECTION_TYPE_EQUALS:
		case XMMS_COLLECTION_TYPE_NOTEQUAL:
		case XMMS_COLLECTION_TYPE_SMALLER:
		case XMMS_COLLECTION_TYPE_SMALLEREQ:
		case XMMS_COLLECTION_TYPE_GREATER:
		case XMMS_COLLECTION_TYPE_GREATEREQ:
			/* Intersection is orderded if the first operand is ordeed */
		case XMMS_COLLECTION_TYPE_INTERSECTION:
			xmmsv_list_get_coll (operands, 0, &c);
			return has_order (c);

			/* Union is ordered if all operands are ordered (concat) */
		case XMMS_COLLECTION_TYPE_UNION:
			for (i = 0; xmmsv_list_get_coll (operands, i, &c); i++) {
				if (!has_order (c))
					return FALSE;
			}

			/* These are always ordered */
		case XMMS_COLLECTION_TYPE_IDLIST:
		case XMMS_COLLECTION_TYPE_ORDER:
		case XMMS_COLLECTION_TYPE_LIMIT:
			return TRUE;

		case XMMS_COLLECTION_TYPE_REFERENCE:
				if (!is_universe (coll)) {
				xmmsv_list_get_coll (operands, 0, &c);
				return has_order (c);
			}
		case XMMS_COLLECTION_TYPE_COMPLEMENT:
		case XMMS_COLLECTION_TYPE_UNIVERSE:
		case XMMS_COLLECTION_TYPE_MEDIASET:
			break;
	}

	return FALSE;
}

static s4_condition_t *collection_to_condition (xmms_medialib_session_t *session,
                                                xmmsv_coll_t *coll,
                                                xmms_fetch_info_t *fetch,
                                                xmmsv_t *order);
static s4_resultset_t *xmms_medialib_query_recurs (xmms_medialib_session_t *session,
                                                   xmmsv_coll_t *coll,
                                                   xmms_fetch_info_t *fetch);

static s4_condition_t *
create_idlist_filter (xmms_medialib_session_t *session, GHashTable *id_table)
{
	s4_sourcepref_t *sourcepref;

	sourcepref = xmms_medialib_get_source_preference (session);

	return s4_cond_new_custom_filter (idlist_filter, id_table,
	                                  (free_func_t)g_hash_table_destroy,
	                                  "song_id", sourcepref, 0, 0, S4_COND_PARENT);
}

static s4_condition_t *
complement_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                      xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	xmmsv_coll_t *operand;
	xmmsv_t *operands;
	s4_condition_t *cond;

	cond = s4_cond_new_combiner (S4_COMBINE_NOT);

	operands = xmmsv_coll_operands_get (coll);
	if (xmmsv_list_get_coll (operands, 0, &operand)) {
		s4_condition_t *operand_cond;
		operand_cond = collection_to_condition (session, operand, fetch, order);
		s4_cond_add_operand (cond, operand_cond);
		s4_cond_unref (operand_cond);
	}

	return cond;
}

static s4_condition_t *
filter_condition (xmms_medialib_session_t *session,
                  xmmsv_coll_t *coll, xmms_fetch_info_t *fetch,
                  xmmsv_t *order)
{
	s4_filter_type_t type;
	s4_sourcepref_t *sp, *_default_sp;
	s4_cmp_mode_t cmp_mode;
	gint32 ival, flags = 0;
	gchar *tmp, *key, *val;
	xmmsv_t *operands;
	xmmsv_coll_t *operand;
	s4_condition_t *cond;
	s4_val_t *sval;

	_default_sp = sp = xmms_medialib_get_source_preference (session);

	if (!xmmsv_coll_attribute_get (coll, "type", &tmp)
	    || strcmp (tmp, "value") == 0) {
		/* If 'field' is not set, match against every key */
		if (!xmmsv_coll_attribute_get (coll, "field", &key)) {
			key = NULL;
		}
	} else if (strcmp (tmp, "id") == 0) {
		key = (gchar *) "song_id";
		flags = S4_COND_PARENT;
	} else {
		xmms_log_error ("FILTER with invalid \"type\"-attribute."
		                "This is probably a bug.");
		/* set key to something safe */
		key = NULL;
	}

	switch (xmmsv_coll_get_type (coll)) {
		case XMMS_COLLECTION_TYPE_HAS:
			type = S4_FILTER_EXISTS; break;
		case XMMS_COLLECTION_TYPE_MATCH:
			type = S4_FILTER_MATCH; break;
		case XMMS_COLLECTION_TYPE_TOKEN:
			type = S4_FILTER_TOKEN; break;
		case XMMS_COLLECTION_TYPE_EQUALS:
			type = S4_FILTER_EQUAL; break;
		case XMMS_COLLECTION_TYPE_NOTEQUAL:
			type = S4_FILTER_NOTEQUAL; break;
		case XMMS_COLLECTION_TYPE_SMALLER:
			type = S4_FILTER_SMALLER; break;
		case XMMS_COLLECTION_TYPE_SMALLEREQ:
			type = S4_FILTER_SMALLEREQ; break;
		case XMMS_COLLECTION_TYPE_GREATER:
			type = S4_FILTER_GREATER; break;
		case XMMS_COLLECTION_TYPE_GREATEREQ:
			type = S4_FILTER_GREATEREQ; break;
		default:
			g_assert_not_reached ();
	}

	if (xmmsv_coll_attribute_get (coll, "value", &val)) {
		if (xmms_is_int (val, &ival)) {
			sval = s4_val_new_int (ival);
		} else {
			sval = s4_val_new_string (val);
		}
	}

	if (!xmmsv_coll_attribute_get (coll, "collation", &val)) {
		/* For <, <=, >= and > we default to natcoll,
		 * so that strings will order correctly
		 * */
		switch (type) {
			case S4_FILTER_SMALLER:
			case S4_FILTER_GREATER:
			case S4_FILTER_SMALLEREQ:
			case S4_FILTER_GREATEREQ:
				cmp_mode = S4_CMP_COLLATE;
				break;
			default:
				cmp_mode = S4_CMP_CASELESS;
		}
	} else if (strcmp (val, "NOCASE") == 0) {
		cmp_mode = S4_CMP_CASELESS;
	} else if (strcmp (val, "BINARY") == 0) {
		cmp_mode = S4_CMP_BINARY;
	} else if (strcmp (val, "NATCOLL") == 0) {
		cmp_mode = S4_CMP_COLLATE;
	} else { /* Unknown collation, fall back to caseless matching */
		cmp_mode = S4_CMP_CASELESS;
	}

	if (xmmsv_coll_attribute_get (coll, "source-preference", &val)) {
		gchar **prefs;

		prefs = g_strsplit (val, ":", -1);
		sp = s4_sourcepref_create ((const gchar **) prefs);
		g_strfreev (prefs);
	}

	cond = s4_cond_new_filter (type, key, sval, sp, cmp_mode, flags);

	if (sval != NULL) {
		s4_val_free (sval);
	}

	if (sp != _default_sp) {
		s4_sourcepref_unref (sp);
	}

	operands = xmmsv_coll_operands_get (coll);
	xmmsv_list_get_coll (operands, 0, &operand);
	if (!is_universe (operand)) {
		s4_condition_t *op_cond = cond;
		cond = s4_cond_new_combiner (S4_COMBINE_AND);
		s4_cond_add_operand (cond, op_cond);
		s4_cond_unref (op_cond);
		op_cond = collection_to_condition (session, operand, fetch, order);
		s4_cond_add_operand (cond, op_cond);
		s4_cond_unref (op_cond);
	}

	return cond;
}

static s4_condition_t *
idlist_condition (xmms_medialib_session_t *session,
                  xmmsv_coll_t *coll, xmms_fetch_info_t *fetch,
                  xmmsv_t *order)
{
	GHashTable *id_table;
	gint32 i, ival;

	xmmsv_list_append (order, xmmsv_coll_idlist_get (coll));

	id_table = g_hash_table_new (NULL, NULL);

	for (i = 0; xmmsv_coll_idlist_get_index (coll, i, &ival); i++) {
		g_hash_table_insert (id_table, GINT_TO_POINTER (ival), GINT_TO_POINTER (1));
	}

	return create_idlist_filter (session, id_table);
}

static s4_condition_t *
intersection_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                        xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	s4_condition_t *cond;
	xmmsv_coll_t *operand;
	xmmsv_t *operands;
	gint i;

	operands = xmmsv_coll_operands_get (coll);
	cond = s4_cond_new_combiner (S4_COMBINE_AND);

	for (i = 0; xmmsv_list_get_coll (operands, i, &operand); i++) {
		s4_condition_t *op_cond;
		if (i == 0) {
			/* We keep the ordering of the first operand */
			op_cond = collection_to_condition (session, operand, fetch, order);
		} else {
			op_cond = collection_to_condition (session, operand, fetch, NULL);
		}
		s4_cond_add_operand (cond, op_cond);
		s4_cond_unref (op_cond);
	}

	return cond;
}

static s4_condition_t *
limit_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                 xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	const s4_resultrow_t *row;
	const s4_result_t *result;
	s4_resultset_t *set;
	xmmsv_coll_t *operand;
	xmmsv_t *operands, *child_order;
	GHashTable *id_table;
	gint32 ival;
	guint start, stop;
	gchar *key;

	if (xmmsv_coll_attribute_get (coll, "start", &key)) {
		start = atoi (key);
	} else {
		start = 0;
	}

	if (xmmsv_coll_attribute_get (coll, "length", &key)) {
		stop = atoi (key) + start;
	} else {
		stop = UINT_MAX;
	}

	operands = xmmsv_coll_operands_get (coll);
	xmmsv_list_get_coll (operands, 0, &operand);

	set = xmms_medialib_query_recurs (session, operand, fetch);

	child_order = xmmsv_new_list ();
	id_table = g_hash_table_new (NULL, NULL);

	for (; start < stop && s4_resultset_get_row (set, start, &row); start++) {
		s4_resultrow_get_col (row, 0, &result);
		if (result != NULL && s4_val_get_int (s4_result_get_val (result), &ival)) {
			g_hash_table_insert (id_table, GINT_TO_POINTER (ival), GINT_TO_POINTER (1));
			xmmsv_list_append_int (child_order, ival);
		}
	}

	s4_resultset_free (set);

	xmmsv_list_append (order, child_order);

	xmmsv_unref (child_order);

	return create_idlist_filter (session, id_table);
}

static s4_condition_t *
mediaset_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                    xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	xmmsv_t *operands = xmmsv_coll_operands_get (coll);
	xmmsv_coll_t *operand;

	xmmsv_list_get_coll (operands, 0, &operand);
	return collection_to_condition (session, operand, fetch, NULL);
}

enum xmms_sort_type_t {
	SORT_TYPE_ID,
	SORT_TYPE_VALUE,
	SORT_TYPE_RANDOM,
	SORT_TYPE_LIST
};

static s4_condition_t *
order_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                 xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	xmmsv_coll_t *operand;
	xmmsv_t *operands;
	gchar *key, *val;

	if (!xmmsv_coll_attribute_get (coll, "type", &key)) {
		key = (gchar *) "value";
	}
	if (strcmp (key, "random") == 0) {
		xmmsv_list_append_string (order, "__ RANDOM __");
	} else {
		if (strcmp (key, "id") == 0) {
			val = (gchar *) "__ ID __";
		} else {
			s4_sourcepref_t *sourcepref;

			sourcepref = xmms_medialib_get_source_preference (session);

			xmmsv_coll_attribute_get (coll, "field", &val);
			xmms_fetch_info_add_key (fetch, NULL, val, sourcepref);
		}

		if (!xmmsv_coll_attribute_get (coll, "direction", &key)
		    || strcmp (key, "ASC") == 0) {
			xmmsv_list_append_string (order, val);
		} else if (strcmp (key, "DESC") == 0) {
			val = g_strconcat ("-", val, NULL);
			xmmsv_list_append_string (order, val);
			g_free (val);
		}
	}

	operands = xmmsv_coll_operands_get (coll);
	xmmsv_list_get_coll (operands, 0, &operand);

	return collection_to_condition (session, operand, fetch, order);
}

static s4_condition_t *
union_ordered_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                         xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *operands, *id_list;
	GHashTable *id_table;

	id_list = xmmsv_new_list ();
	id_table = g_hash_table_new (NULL, NULL);
	operands = xmmsv_coll_operands_get (coll);

	xmmsv_get_list_iter (operands, &it);
	while (xmmsv_list_iter_valid (it)) {
		const s4_resultrow_t *row;
		s4_resultset_t *set;
		xmmsv_coll_t *operand;
		gint j;

		xmmsv_list_iter_entry_coll (it, &operand);

		/* Query the operand */
		set = xmms_medialib_query_recurs (session, operand, fetch);

		/* Append the IDs to the id_list */
		for (j = 0; s4_resultset_get_row (set, j, &row); j++) {
			const s4_result_t *result;
			gint32 value;

			if (!s4_resultrow_get_col (row, 0, &result))
			    continue;

			if (!s4_val_get_int (s4_result_get_val (result), &value))
				continue;

			xmmsv_list_append_int (id_list, value);

			g_hash_table_insert (id_table,
			                     GINT_TO_POINTER (value),
			                     GINT_TO_POINTER (1));
		}

		s4_resultset_free (set);

		xmmsv_list_iter_next (it);
	}

	xmmsv_list_append (order, id_list);
	xmmsv_unref (id_list);

	return create_idlist_filter (session, id_table);
}

static s4_condition_t *
union_unordered_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                           xmms_fetch_info_t *fetch)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *operands;
	s4_condition_t *cond;

	cond = s4_cond_new_combiner (S4_COMBINE_OR);
	operands = xmmsv_coll_operands_get (coll);

	xmmsv_get_list_iter (operands, &it);
	while (xmmsv_list_iter_valid (it)) {
		s4_condition_t *op_cond;
		xmmsv_coll_t *operand;

		xmmsv_list_iter_entry_coll (it, &operand);

		op_cond = collection_to_condition (session, operand, fetch, NULL);
		s4_cond_add_operand (cond, op_cond);
		s4_cond_unref (op_cond);

		xmmsv_list_iter_next (it);
	}

	return cond;
}


static s4_condition_t *
union_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                 xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	if (has_order (coll)) {
		return union_ordered_condition (session, coll, fetch, order);
	}
	return union_unordered_condition (session, coll, fetch);
}

static s4_condition_t *
universe_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                    xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	return s4_cond_new_custom_filter ((filter_function_t) universe_filter, NULL, NULL,
	                                  "song_id", NULL, S4_CMP_BINARY, 0, S4_COND_PARENT);
}

static s4_condition_t *
reference_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                     xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	xmmsv_t *operands;

	if (is_universe (coll)) {
		return universe_condition (session, coll, fetch, order);
	}

	operands = xmmsv_coll_operands_get (coll);
	xmmsv_list_get_coll (operands, 0, &coll);

	return collection_to_condition  (session, coll, fetch, order);
}

/**
 * Convert an xmms2 collection to an S4 condition.
 *
 * @param coll The collection to convert
 * @param fetch Information on what S4 fetches
 * @param order xmmsv_t list that will be filled in with order information
 * as the function recurses.
 * @return A new S4 condition. Must be freed with s4_cond_free
 */
static s4_condition_t *
collection_to_condition (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                         xmms_fetch_info_t *fetch, xmmsv_t *order)
{
	switch (xmmsv_coll_get_type (coll)) {
		case XMMS_COLLECTION_TYPE_COMPLEMENT:
			return complement_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_HAS:
		case XMMS_COLLECTION_TYPE_MATCH:
		case XMMS_COLLECTION_TYPE_TOKEN:
		case XMMS_COLLECTION_TYPE_EQUALS:
		case XMMS_COLLECTION_TYPE_NOTEQUAL:
		case XMMS_COLLECTION_TYPE_SMALLER:
		case XMMS_COLLECTION_TYPE_SMALLEREQ:
		case XMMS_COLLECTION_TYPE_GREATER:
		case XMMS_COLLECTION_TYPE_GREATEREQ:
			return filter_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_IDLIST:
			return idlist_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_INTERSECTION:
			return intersection_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_LIMIT:
			return limit_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_MEDIASET:
			return mediaset_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_ORDER:
			return order_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_REFERENCE:
			return reference_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_UNION:
			return union_condition (session, coll, fetch, order);
		case XMMS_COLLECTION_TYPE_UNIVERSE:
			return universe_condition (session, coll, fetch, order);
		default:
			/* ??? */
			return NULL;
	}
}

/**
 * Internal function that does the actual querying.
 *
 * @param coll The collection to use when querying
 * @param fetch Information on what is being fetched
 * @return An S4 resultset correspoding to the entires in the
 * medialib matching the collection.
 * Must be free with s4_resultset_free
 */
static s4_resultset_t *
xmms_medialib_query_recurs (xmms_medialib_session_t *session,
                            xmmsv_coll_t *coll, xmms_fetch_info_t *fetch)
{
	s4_condition_t *cond;
	s4_resultset_t *ret;
	xmmsv_t *order;

	order = xmmsv_new_list ();

	cond = collection_to_condition (session, coll, fetch, order);
	ret = s4_query (NULL, session->trans, fetch->fs, cond);
	s4_cond_free (cond);

	ret = xmms_medialib_result_sort (ret, fetch, order);

	xmmsv_unref (order);

	return ret;
}


/**
 * Returns a random entry from a collection
 *
 * @param coll The collection to find a random entry in
 * @return A random entry from the collection, 0 if the collection is empty
 */
xmms_medialib_entry_t
xmms_medialib_query_random_id (xmms_medialib_session_t *session,
                               xmmsv_coll_t *coll)
{
	xmmsv_t *fetch_spec, *get_list, *res;
	xmms_medialib_entry_t ret;
	xmms_error_t err;

	get_list = xmmsv_new_list ();
	xmmsv_list_append_string (get_list, "id");

	fetch_spec = xmmsv_new_dict ();
	xmmsv_dict_set_string (fetch_spec, "type", "metadata");
	xmmsv_dict_set_string (fetch_spec, "aggregate", "random");
	xmmsv_dict_set (fetch_spec, "get", get_list);

	res = xmms_medialib_query (session, coll, fetch_spec, &err);
	xmmsv_get_int (res, &ret);

	xmmsv_unref (get_list);
	xmmsv_unref (fetch_spec);
	xmmsv_unref (res);

	return ret;
}

typedef xmmsv_t *(*void_to_xmmsv_t)(void *value, void *userdata);

static xmmsv_t *
convert_ghashtable_to_xmmsv (GHashTable *table, gint depth,
                             void_to_xmmsv_t func, void *userdata)
{
	GHashTableIter iter;
	GHashTable *value;
	const gchar *key;
	xmmsv_t *ret;

	if (depth == 0) {
		return func ((void *) table, userdata);
	}

	g_hash_table_iter_init (&iter, table);

	ret = xmmsv_new_dict ();

	while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &value)) {
		xmmsv_t *xvalue;

		if (value == NULL) {
			continue;
		}

		xvalue = convert_ghashtable_to_xmmsv (value, depth - 1, func, userdata);
		xmmsv_dict_set (ret, key, xvalue);
		xmmsv_unref (xvalue);
	}

	if (xmmsv_dict_get_size (ret) == 0) {
		xmmsv_unref (ret);
		ret = NULL;
	}

	return ret;
}


typedef struct {
	gint64 sum;
	gint n;
} avg_data_t;

typedef struct {
	xmmsv_t *data;
	gint n;
} random_data_t;

typedef struct {
	GHashTable *ht;
	xmmsv_t *list;
} set_data_t;

/* Converts an S4 result (a column) into an xmmsv values */
static void *
result_to_xmmsv (xmmsv_t *ret, gint32 id, const s4_result_t *res,
                 xmms_fetch_spec_t *spec)
{
	const s4_val_t *val;
	xmmsv_t *dict, *cur;
	const gchar *strval, *key = NULL;
	gint32 i, ival, oldval, new_val;
	guint len;

	/* Big enough to hold 2^32 with minus sign */
	gchar buf[12];

	/* Loop through all the values the column has */
	while (res != NULL) {
		dict = ret;
		cur = ret;

		/* Loop through the list of what to get ("key", "source", ..) */
		for (i = 0; i < spec->data.metadata.get_size; i++) {
			strval = NULL;
			/* Fill strval with the correct value if it is a string
			 * or ival if it is an integer
			 */
			switch (spec->data.metadata.get[i]) {
				case METADATA_KEY:
					strval = s4_result_get_key (res);
					break;
				case METADATA_SOURCE:
					strval = s4_result_get_src (res);
					if (strval == NULL)
						strval = "server";
					break;
				case METADATA_ID:
					ival = id;
					break;
				case METADATA_VALUE:
					val = s4_result_get_val (res);

					if (!s4_val_get_int (val, &ival)) {
						s4_val_get_str (val, &strval);
					}
					break;
			}

			/* If this is not the last property to get we use this property
			 * as a key in a dict
			 */
			if (i < (spec->data.metadata.get_size - 1)) {
				/* Convert integers to strings */
				if (strval == NULL) {
					sprintf (buf, "%i", ival);
					key = buf;
				} else {
					key = strval;
				}

				/* Make sure the root dict exists */
				if (dict == NULL) {
					ret = dict = xmmsv_new_dict ();
				}

				/* If this dict contains dicts we have to create a new
				 * dict if one does not exists for the key yet
				 */
				if (!xmmsv_dict_get (dict, key, &cur))
					cur = NULL;

				if (i < (spec->data.metadata.get_size - 2)) {
					if (cur == NULL) {
						cur = xmmsv_new_dict ();
						xmmsv_dict_set (dict, key, cur);
						xmmsv_unref (cur);
					}
					dict = cur;
				}
			}
		}

		new_val = 0;

		switch (spec->data.metadata.aggr_func) {
			case AGGREGATE_FIRST: {
				if (cur == NULL) {
					if (strval != NULL) {
						cur = xmmsv_new_string (strval);
					} else {
						cur = xmmsv_new_int (ival);
					}
					new_val = 1;
				}
				break;
			}
			case AGGREGATE_LIST: {
				if (cur == NULL) {
					cur = xmmsv_new_list ();
					new_val = 1;
				}
				if (strval != NULL) {
					xmmsv_list_append_string (cur, strval);
				} else {
					xmmsv_list_append_int (cur, ival);
				}
				break;
			}
			case AGGREGATE_SET: {
				set_data_t *data;
				xmmsv_t *val;
				void *key;

				if (cur == NULL) {
					set_data_t init = {
						.ht = g_hash_table_new (NULL, NULL),
						.list = xmmsv_new_list ()
					};
					cur = xmmsv_new_bin ((guchar *) &init, sizeof (set_data_t));
					new_val = 1;
				}

				xmmsv_get_bin (cur, (const guchar **) &data, &len);

				if (strval != NULL) {
					val = xmmsv_new_string (strval);
					key = (void *) strval;
				} else {
					val = xmmsv_new_int (ival);
					key = GINT_TO_POINTER (ival);
				}

				if (g_hash_table_lookup (data->ht, key) == NULL) {
					g_hash_table_insert (data->ht, key, val);
					xmmsv_list_append (data->list, val);
				}
				xmmsv_unref (val);
				break;
			}
			case AGGREGATE_SUM: {
				if (strval != NULL) {
					ival = 0;
				}

				if (cur != NULL) {
					xmmsv_get_int (cur, &oldval);
					xmmsv_unref (cur);
				} else {
					oldval = 0;
				}

				ival += oldval;
				cur = xmmsv_new_int (ival);
				new_val = 1;
				break;
			}
			case AGGREGATE_MIN: {
				if (strval == NULL && (cur == NULL || (xmmsv_get_int (cur, &oldval) && oldval > ival))) {
					if (cur != NULL) {
						xmmsv_unref (cur);
					}
					cur = xmmsv_new_int (ival);
					new_val = 1;
				}
				break;
			}
			case AGGREGATE_MAX: {
				if (strval == NULL && (cur == NULL || (xmmsv_get_int (cur, &oldval) && oldval < ival))) {
					if (cur != NULL) {
						xmmsv_unref (cur);
					}
					cur = xmmsv_new_int (ival);
					new_val = 1;
				}
				break;
			}
			case AGGREGATE_RANDOM: {
				random_data_t *data;

				if (cur == NULL) {
					random_data_t init = { 0 };
					cur = xmmsv_new_bin ((guchar *) &init, sizeof (random_data_t));
					new_val = 1;
				}

				xmmsv_get_bin (cur, (const guchar **) &data, &len);

				data->n++;
				if (g_random_int_range (0, data->n) == 0) {
					xmmsv_unref (data->data);
					if (strval != NULL) {
						data->data = xmmsv_new_string (strval);
					} else {
						data->data = xmmsv_new_int (ival);
					}
				}
				break;
			}
			case AGGREGATE_AVG: {
				avg_data_t *data;

				if (cur == NULL) {
					avg_data_t init = { 0 };
					cur = xmmsv_new_bin ((guchar *) &init, sizeof (avg_data_t));
					new_val = 1;
				}

				xmmsv_get_bin (cur, (const guchar **) &data, &len);

				if (strval == NULL) {
					data->n++;
					data->sum += ival;
				}
				break;
			}
		}

		/* Update the previous dict (if there is one) */
		if (i > 1 && new_val) {
			xmmsv_dict_set (dict, key, cur);
			xmmsv_unref (cur);
		} else if (new_val) {
			ret = cur;
		}

		res = s4_result_next (res);
	}

	return ret;
}

/* Converts the temporary value returned by result_to_xmmsv into the real value */
static xmmsv_t *
aggregate_data (xmmsv_t *value, aggregate_function_t aggr_func)
{
	const random_data_t *random_data;
	const avg_data_t *avg_data;
	const set_data_t *set_data;
	gconstpointer data;
	xmmsv_t *ret;
	guint len;

	ret = NULL;
	data = NULL;

	if (value != NULL && xmmsv_is_type (value, XMMSV_TYPE_BIN))
		xmmsv_get_bin (value, (const guchar **) &data, &len);

		switch (aggr_func) {
			case AGGREGATE_FIRST:
			case AGGREGATE_MIN:
			case AGGREGATE_MAX:
			case AGGREGATE_SUM:
			case AGGREGATE_LIST:
				ret = xmmsv_ref (value);
				break;
			case AGGREGATE_RANDOM:
				random_data = data;
				if (random_data != NULL) {
					ret = random_data->data;
				}
				break;
			case AGGREGATE_SET:
				set_data = data;
				g_hash_table_destroy (set_data->ht);
				ret = set_data->list;
				break;
			case AGGREGATE_AVG:
				avg_data = data;
				if (avg_data != NULL) {
					ret = xmmsv_new_int (avg_data->n ? avg_data->sum / avg_data->n : 0);
				}
				break;
	}

	xmmsv_unref (value);

	return ret;
}

/* Applies an aggregation function to the leafs in an xmmsv dict tree */
static xmmsv_t *
aggregate_result (xmmsv_t *val, gint depth, aggregate_function_t aggr_func)
{
	xmmsv_dict_iter_t *it;

	if (val == NULL) {
		return NULL;
	}

	if (depth == 0) {
		return aggregate_data (val, aggr_func);
	}

	/* If it's a dict we call this function recursively on all its values */
	xmmsv_get_dict_iter (val, &it);

	while (xmmsv_dict_iter_valid (it)) {
		xmmsv_t *entry;

		xmmsv_dict_iter_pair (it, NULL, &entry);

		entry = aggregate_result (entry, depth - 1, aggr_func);
		xmmsv_dict_iter_set (it, entry);
		xmmsv_unref (entry);

		xmmsv_dict_iter_next (it);
	}

	return val;
}

/* Converts an S4 resultset to an xmmsv using the fetch specification */
static xmmsv_t *
metadata_to_xmmsv (s4_resultset_t *set, xmms_fetch_spec_t *spec)
{
	const s4_resultrow_t *row;
	xmmsv_t *ret = NULL;
	gint i;

	/* Loop over the rows in the resultset */
	for (i = 0; s4_resultset_get_row (set, i, &row); i++) {
		gint32 id, j;

		s4_val_get_int (s4_result_get_val (s4_resultset_get_result (set, i, 0)), &id);
		for (j = 0; j < spec->data.metadata.col_count; j++) {
			const s4_result_t *res;

			if (s4_resultrow_get_col (row, spec->data.metadata.cols[j], &res)) {
				ret = result_to_xmmsv (ret, id, res, spec);
			}
		}
	}

	return aggregate_result (ret, spec->data.metadata.get_size - 1,
	                         spec->data.metadata.aggr_func);
}


/* Divides an S4 set into a list of smaller sets with
 * the same values for the cluster attributes
 */
static void
cluster_set (s4_resultset_t *set, xmms_fetch_spec_t *spec,
             GHashTable *table, GList **list)
{
	const s4_resultrow_t *row;
	gint position;

	/* Run through all the rows in the result set.
	 * Uses a hash table to find the correct cluster to put the row in
	 */
	for (position = 0; s4_resultset_get_row (set, position, &row); position++) {
		s4_resultset_t *cluster;
		const s4_result_t *res;
		const gchar *value = "(No value)"; /* Used to represent NULL */
		gchar buf[12];

		if (spec->data.cluster.type == CLUSTER_BY_POSITION) {
			g_snprintf (buf, sizeof (buf), "%i", position);
			value = buf;
		} else if (s4_resultrow_get_col (row, spec->data.cluster.column, &res)) {
			const s4_val_t *val = s4_result_get_val (res);
			if (!s4_val_get_str (val, &value)) {
				gint32 ival;
				s4_val_get_int (val, &ival);
				g_snprintf (buf, sizeof (buf), "%i", ival);
				value = buf;
			}
		}

		cluster = g_hash_table_lookup (table, value);
		if (cluster == NULL) {
			cluster = s4_resultset_create (s4_resultset_get_colcount (set));
			g_hash_table_insert (table, g_strdup (value), cluster);
			*list = g_list_prepend (*list, cluster);
		}
		s4_resultset_add_row (cluster, row);
	}
}

static GList *
cluster_list (s4_resultset_t *set, xmms_fetch_spec_t *spec)
{
	GHashTable *table;
	GList *list = NULL;

	table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	cluster_set (set, spec, table, &list);
	g_hash_table_destroy (table);

	return g_list_reverse (list);
}

static GHashTable *
cluster_dict (s4_resultset_t *set, xmms_fetch_spec_t *spec)
{
	GHashTable *table;
	GList *list = NULL;

	table = g_hash_table_new_full (g_str_hash, g_str_equal,
	                               g_free, (GDestroyNotify) s4_resultset_free);

	cluster_set (set, spec, table, &list);
	g_list_free (list);

	return table;
}

/* Converts an S4 resultset into an xmmsv_t, based on the fetch specification */
static xmmsv_t *
resultset_to_xmmsv (s4_resultset_t *set, xmms_fetch_spec_t *spec)
{
	GHashTable *set_table;
	GList *sets;
	xmmsv_t *val, *ret = NULL;
	gint i;

	switch (spec->type) {
		case FETCH_COUNT:
			ret = xmmsv_new_int (s4_resultset_get_rowcount (set));
			break;
		case FETCH_METADATA:
			ret = metadata_to_xmmsv (set, spec);
			break;
		case FETCH_ORGANIZE:
			ret = xmmsv_new_dict ();

			for (i = 0; i < spec->data.organize.count; i++) {
				val = resultset_to_xmmsv (set, spec->data.organize.data[i]);
				if (val != NULL) {
					xmmsv_dict_set (ret, spec->data.organize.keys[i], val);
					xmmsv_unref (val);
				}
			}
			break;
		case FETCH_CLUSTER_LIST:
			sets = cluster_list (set, spec);
			ret = xmmsv_new_list ();
			for (; sets != NULL; sets = g_list_delete_link (sets, sets)) {
				set = sets->data;

				val = resultset_to_xmmsv (set, spec->data.cluster.data);
				if (val != NULL) {
					xmmsv_list_append (ret, val);
					xmmsv_unref (val);
				}
				s4_resultset_free (set);
			}
			break;
		case FETCH_CLUSTER_DICT:
			set_table = cluster_dict (set, spec);
			ret = convert_ghashtable_to_xmmsv (set_table, 1,
			                                   (void_to_xmmsv_t) resultset_to_xmmsv,
			                                   spec->data.cluster.data);

			g_hash_table_destroy (set_table);
			break;
	}

	return ret;
}



/**
 * Queries the medialib and returns an xmmsv_t with the info requested
 *
 * @param coll The collection to find
 * @param fetch Specifies what to fetch
 * @return An xmmsv_t with the structure requested in fetch
 */
xmmsv_t *
xmms_medialib_query (xmms_medialib_session_t *session, xmmsv_coll_t *coll,
                     xmmsv_t *fetch, xmms_error_t *err)
{
	s4_sourcepref_t *sourcepref;
	s4_resultset_t *set;
	xmmsv_t *ret;
	xmms_fetch_info_t *info;
	xmms_fetch_spec_t *spec;

	xmms_error_reset (err);

	sourcepref = xmms_medialib_get_source_preference (session);

	info = xmms_fetch_info_new (sourcepref);
	spec = xmms_fetch_spec_new (fetch, info, sourcepref, err);

	if (spec == NULL) {
		xmms_fetch_spec_free (spec);
		xmms_fetch_info_free (info);
		return NULL;
	}

	set = xmms_medialib_query_recurs (session, coll, info);
	ret = resultset_to_xmmsv (set, spec);
	s4_resultset_free (set);

	xmms_fetch_spec_free (spec);
	xmms_fetch_info_free (info);

	xmmsv_list_append (session->vals, ret);

	return ret;
}
