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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsc_stringport.h"

static const char* constraint_templates[4] = {"m%d.key = LOWER(%s)",
					      "LOWER(m%d.value) LIKE LOWER(%s)",
					      "m%d.id = m%d.id",
					      "Media AS m%d"};

typedef enum templ_type_e {templ_key, templ_value, templ_id, templ_table} templ_type;

/**
 * @defgroup MedialibControl MedialibControl
 * @ingroup XMMSClient
 * @brief This controls the medialib.
 *
 * @{
 */

static xmmsc_result_t *
do_methodcall (xmmsc_connection_t *conn, unsigned int id, const char *arg)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, id);
	xmms_ipc_msg_put_string (msg, arg);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/**
 * Make a SQL query to the server medialib. The result will contain
 * a list of dicts.
 * @param conn The #xmmsc_connection_t
 * @param query The SQL query.
 */
xmmsc_result_t *
xmmsc_medialib_select (xmmsc_connection_t *conn, const char *query)
{
	x_check_conn (conn, NULL);

	return do_methodcall (conn, XMMS_IPC_CMD_SELECT, query);
}

/**
 * Escape a string so that it can be used in sqlite queries.
 *
 * @param Input string, is not freed by this function!
 * @returns string enclosed in single quotes, with all single quotes
 * in the string replaced with double single quotes
 *
 * Example:
 * Ain't -> 'Ain''t'
 */
char *
xmmsc_sqlite_prepare_string (const char *input) {
	char *output;
	int outsize, nquotes = 0;
	int i, o;

	for (i = 0; input[i] != '\0'; i++) {
		if (input[i] == '\'') {
			nquotes++;
		}
	}

	outsize = strlen(input) + nquotes + 2 + 1; /* 2 quotes to terminate the string , and one \0 in the end */
	output = malloc(outsize);

	if (output == NULL) {
		x_oom();
		return NULL;
	}

	i = o = 0;
	output[o++] = '\'';
	while (input[i] != '\0') {
		output[o++] = input[i];
		if (input[i++] == '\'') {
			output[o++] = '\'';
		}
	}
	output[o++] = '\'';
	output[o] = '\0';

	return output;

}


/**
 * @internal
 *
 */

static char *
xmmsc_querygen_fill_template (templ_type idx, xmmsc_query_attribute_t *attributes, unsigned i)
{
	int res_size = 0;
	char *res;
	char t;

	switch (idx) {
	case templ_key:
		res_size = snprintf(&t, 1, constraint_templates[templ_key], i, attributes[i].key);
		break;
	case templ_value:
		res_size = snprintf(&t, 1, constraint_templates[templ_value], i, attributes[i].value);
		break;
	case templ_id:
		res_size = snprintf(&t, 1, constraint_templates[templ_id], i-1, i);
		break;
	case templ_table:
		res_size = snprintf(&t, 1, constraint_templates[templ_table], i);
		break;
	default:
		/* do we need a default error case? */
		break;
	}

	res_size += 1;

	res = malloc(res_size);
	if (res == NULL) {
		x_oom();
		return NULL;
	}


	switch (idx) {
	case templ_key:
		snprintf(res, res_size, constraint_templates[templ_key], i, attributes[i].key);
		break;
	case templ_value:
		snprintf(res, res_size, constraint_templates[templ_value], i, attributes[i].value);
		break;
	case templ_id:
		snprintf(res, res_size, constraint_templates[templ_id], i-1, i);
		break;
	case templ_table:
		snprintf(res, res_size, constraint_templates[templ_table], i);
		break;
	}
	return res;

}

/**
 * @internal
 * Construct constraints of the query string from query attribute vector
 */

static int
xmmsc_querygen_parse_constraints (char **pconstraints,
                                  xmmsc_query_attribute_t *attributes,
                                  unsigned int n)
{
	int success = 1, tmp_size;
	char *oconstraints = NULL, *constraints, *tmp = NULL;
	unsigned int i, size = 0;
	templ_type template;

	constraints = strdup (" WHERE ");

	if (constraints == NULL) {
		x_oom ();
		*pconstraints = NULL;
		return 0;
	} else {
		size = strlen (constraints) + 1;
	}

	for (i = 0; i < n; i++) {
		for (template = templ_key; template <= templ_id; template++) {
			if (!i && template == templ_id) {
				break; /* Can't do id matching on the first attribute */
			}

			tmp = xmmsc_querygen_fill_template(template, attributes, i);
			if (!tmp) {
				success = 0;
			}

			tmp_size = strlen (tmp);

			size += tmp_size + (!i && template == templ_key ? 0 : 5);
			oconstraints = constraints;
			constraints = realloc(constraints, size);
			if (!constraints) {
				success = 0;
				free (oconstraints);
				break;
			}

			if (!(!i && template == templ_key) ) {
				/* Don't need AND for first constraint */
				strcat (constraints, " AND ");
			}

			strcat (constraints, tmp);
			free (tmp);
		}
	}

	*pconstraints = constraints;

	return success;
}

/**
 * @internal
 * Construct tables of the query string from query attribute vector
 */

static int
xmmsc_querygen_parse_tables (char **ptables,
                             xmmsc_query_attribute_t *attributes,
                             unsigned int n)
{
	int success = 1;
	char *otables = NULL, *tables, *tmp = NULL;
	unsigned int i, size = 1; /* make space for the terminating null byte */
	unsigned int tmp_size = 0;

	tables = malloc (1);
	if (tables == NULL) {
		x_oom ();
		*ptables = NULL;
		return 0;
	}

	tables[0] = '\0';

	for (i = 0; i < n; i++) {
		tmp = xmmsc_querygen_fill_template (templ_table, attributes, i);
		if (!tmp) {
			success = 0;
			break;
		}

		tmp_size = strlen (tmp);

		size += tmp_size + (i==0 ? 0 : 2); /* space for ", " */
		otables = tables;
		tables = realloc (tables, size);

		if (tables == NULL) {
			x_oom ();
			success = 0;
			free (otables);
			break;
		}

		if (i) {
			strcat (tables, ", ");
		}

		strcat (tables, tmp);
		free (tmp);
	}

	(*ptables) = tables;

	return success;
}

/**
 * Construct a query to match songs with all the given attrbutes.
 *
 * @param A vector of attribute pointers
 * @param The length of the vector
 * @returns string with the query to match given attributes. Caller is responsible
 * of freeing both the table and the string.
 *
 *
 */

char *
xmmsc_querygen_and (xmmsc_query_attribute_t *attributes, unsigned n)
{
	char *tables = NULL, *constraints = NULL, *query = NULL;
	int success, fullsize;

	const char *initquery = "SELECT DISTINCT m0.id FROM ";

	success = xmmsc_querygen_parse_tables (&tables, attributes, n);

	if (success) {
		success = xmmsc_querygen_parse_constraints (&constraints,
		                                            attributes, n);
	}

	if (success) {
		fullsize = strlen (initquery);
		fullsize += strlen (tables);
		fullsize += strlen (constraints);

		query = malloc (fullsize + 1);
		success = !!query;
	}

	if (success) {
		query[0] = '\0';
		strcat (query, initquery);
		strcat (query, tables);
		strcat (query, constraints);
	}

	if (tables) 
		free (tables);
	free (constraints);

	return query;
}

/**
 * Search for a entry (URL) in the medialib db and return its ID number
 * @param conn The #xmmsc_connection_t
 * @param url The URL to search for
 */
xmmsc_result_t *
xmmsc_medialib_get_id (xmmsc_connection_t *conn, const char *url)
{
	x_check_conn (conn, NULL);

	return do_methodcall (conn, XMMS_IPC_CMD_GET_ID, url);
}

/**
 * Export a serverside playlist to a format that could be read
 * from another mediaplayer.
 * @param conn The #xmmsc_connection_t
 * @param playlist Name of a serverside playlist
 * @param mime Mimetype of the export format.
 */
xmmsc_result_t *
xmmsc_medialib_playlist_export (xmmsc_connection_t *conn, const char *playlist, const char *mime)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_PLAYLIST_EXPORT);
	xmms_ipc_msg_put_string (msg, playlist);
	xmms_ipc_msg_put_string (msg, mime);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/**
 * This will make the server list the given playlist. 
 */
xmmsc_result_t *
xmmsc_medialib_playlist_list (xmmsc_connection_t *conn, const char *playlist)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_PLAYLIST_LIST);
	xmms_ipc_msg_put_string (msg, playlist);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/**
 * Returns a list of all available playlists
 */
xmmsc_result_t *
xmmsc_medialib_playlists_list (xmmsc_connection_t *conn)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_PLAYLISTS_LIST);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/**
 * Import a playlist from a playlist file.
 * @param conn The #xmmsc_connection_t
 * @param playlist The name of the new playlist.
 * @param url URL to the playlist file.
 */
xmmsc_result_t *
xmmsc_medialib_playlist_import (xmmsc_connection_t *conn, const char *playlist, const char *url)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_PLAYLIST_IMPORT);
	xmms_ipc_msg_put_string (msg, playlist);
	xmms_ipc_msg_put_string (msg, url);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/**
 * Remove a entry from the medialib
 * @param conn The #xmmsc_connection_t
 * @param entry The entry id you want to remove
 */
xmmsc_result_t *
xmmsc_medialib_remove_entry (xmmsc_connection_t *conn, uint32_t entry)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_REMOVE);
	xmms_ipc_msg_put_uint32 (msg, entry);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/**
 * Add a URL to the medialib. If you want to add mutiple files
 * you should call #xmmsc_medialib_path_import
 * @param conn The #xmmsc_connection_t
 * @param url URL to add to the medialib.
 */
xmmsc_result_t *
xmmsc_medialib_add_entry (xmmsc_connection_t *conn, const char *url)
{
	return xmmsc_medialib_add_entry_args (conn, url, 0, NULL);
}

/**
 * Add a URL with arguments to the medialib.
 *
 * xmmsc_medialib-add_antry_args (conn, "file:///data/HVSC/C64Music/Hubbard_Rob/Commando.sid", 1, "subtune=2");
 * 
 * @param conn The #xmmsc_connection_t
 * @param url URL to add to the medialib.
 * @param numargs The number of arguments
 * @param args array of numargs strings used as arguments
 */
xmmsc_result_t *
xmmsc_medialib_add_entry_args (xmmsc_connection_t *conn, const char *url, int numargs, const char **args)
{
	char *enc_url;
	xmmsc_result_t *res;

	x_check_conn (conn, NULL);

	enc_url = xmmsc_medialib_encode_url (url, numargs, args);
	if (!enc_url)
		return NULL;

	res = do_methodcall (conn, XMMS_IPC_CMD_ADD, enc_url);

	free (enc_url);

	return res;
}

/**
 * Save the current playlist to a serverside playlist
 */
xmmsc_result_t *
xmmsc_medialib_playlist_save_current (xmmsc_connection_t *conn,
                                      const char *name)
{
	x_check_conn (conn, NULL);

	return do_methodcall (conn, XMMS_IPC_CMD_PLAYLIST_SAVE_CURRENT, name);
}

/**
 * Load a playlist from the medialib to the current active playlist
 */
xmmsc_result_t *
xmmsc_medialib_playlist_load (xmmsc_connection_t *conn,
                                      const char *name)
{
	x_check_conn (conn, NULL);

	return do_methodcall (conn, XMMS_IPC_CMD_PLAYLIST_LOAD, name);
}

/**
 * Remove a playlist from the medialib, keeping the songs of course.
 * @param conn #xmmsc_connection_t
 * @param playlist The playlist to remove
 */
xmmsc_result_t *
xmmsc_medialib_playlist_remove (xmmsc_connection_t *conn, const char *playlist)
{
	x_check_conn (conn, NULL);

	return do_methodcall (conn, XMMS_IPC_CMD_PLAYLIST_REMOVE, playlist);
}

/**
 * Import a all files recursivly from the directory passed
 * as argument.
 * @param conn #xmmsc_connection_t
 * @param path A directory to recursive search for mediafiles, this must
 * 		  include the protocol, i.e file://
 */
xmmsc_result_t *
xmmsc_medialib_path_import (xmmsc_connection_t *conn,
			    const char *path)
{
	xmmsc_result_t *res;
	char *enc_path;

	x_check_conn (conn, NULL);

	enc_path = xmmsc_medialib_encode_url (path, 0, NULL);
	if (!enc_path)
		return NULL;

	res = do_methodcall (conn, XMMS_IPC_CMD_PATH_IMPORT, enc_path);

	free (enc_path);

	return res;
}

/**
 * Rehash the medialib, this will check data in the medialib
 * still is the same as the data in files.
 * @param id The id to rehash. Set it to 0 if you want to rehash
 * the whole medialib.
 */
xmmsc_result_t *
xmmsc_medialib_rehash (xmmsc_connection_t *conn,
                       unsigned int id)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_REHASH);
	xmms_ipc_msg_put_uint32 (msg, id);

	res = xmmsc_send_msg (conn, msg);

	return res;

}

/**
 * Retrieve information about a entry from the medialib.
 */
xmmsc_result_t *
xmmsc_medialib_get_info (xmmsc_connection_t *c, unsigned int id)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_INFO);
	xmms_ipc_msg_put_uint32 (msg, id);

	res = xmmsc_send_msg (c, msg);

	return res;
}

/**
 * Request the medialib_playlist_loaded broadcast. This will be called
 * if a playlist is loaded server-side. The argument will be a string
 * with the playlist name.
 */
xmmsc_result_t *
xmmsc_broadcast_medialib_playlist_loaded (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_MEDIALIB_PLAYLIST_LOADED);
}

/** 
 * Request the medialib_entry_added broadcast. This will be called
 * if a new entry is added to the medialib serverside.
 */
xmmsc_result_t * 
xmmsc_broadcast_medialib_entry_added (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_ADDED);
}

/**
 * Request the medialib_entry_changed broadcast. This will be called
 * if a entry changes on the serverside. The argument will be an medialib
 * id.
 */
xmmsc_result_t *
xmmsc_broadcast_medialib_entry_changed (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);
}

/**
 * Queries the medialib for files and adds the matching ones to
 * the current playlist. Remember to include a field called id
 * in the query.
 *
 * @param c The connection structure.
 * @param query sql-query to medialib.
 *
 */
xmmsc_result_t *
xmmsc_medialib_add_to_playlist (xmmsc_connection_t *c, const char *query)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_ADD_TO_PLAYLIST);
	xmms_ipc_msg_put_string (msg, query);
	res = xmmsc_send_msg (c, msg);

	return res;

}

/**
 * Associate a int value with a medialib entry. Uses default
 * source which is client/<clientname>
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_set_int (xmmsc_connection_t *c, uint32_t id,
                                       const char *key, int32_t value)
{
	xmmsc_result_t *res;
	char tmp[256];

	x_check_conn (c, NULL);

	snprintf (tmp, 256, "client/%s", c->clientname);
	res = xmmsc_medialib_entry_property_set_int_with_source (c, id,
	                                                         tmp, key,
	                                                         value);
	return res;
}

/**
 * Set a custom int field in the medialib associated with a entry,
 * the same as #xmmsc_result_entry_property_set but with specifing
 * your own source.
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_set_int_with_source (xmmsc_connection_t *c, 
                                                   uint32_t id,
                                                   const char *source, 
                                                   const char *key, 
                                                   int32_t value)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB,
	                        XMMS_IPC_CMD_PROPERTY_SET_INT);
	xmms_ipc_msg_put_uint32 (msg, id);
	xmms_ipc_msg_put_string (msg, source);
	xmms_ipc_msg_put_string (msg, key);
	xmms_ipc_msg_put_int32 (msg, value);

	res = xmmsc_send_msg (c, msg);

	return res;
}

/**
 * Associate a value with a medialib entry. Uses default
 * source which is client/<clientname>
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_set_str (xmmsc_connection_t *c, uint32_t id,
                                       const char *key, const char *value)
{
	xmmsc_result_t *res;
	char tmp[256];

	x_check_conn (c, NULL);

	snprintf (tmp, 256, "client/%s", c->clientname);
	res = xmmsc_medialib_entry_property_set_str_with_source (c, id,
	                                                         tmp, key,
	                                                         value);
	return res;
}

/**
 * Set a custom field in the medialib associated with a entry,
 * the same as #xmmsc_result_entry_property_set but with specifing
 * your own source.
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_set_str_with_source (xmmsc_connection_t *c, 
                                                   uint32_t id,
                                                   const char *source, 
                                                   const char *key, 
                                                   const char *value)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB,
	                        XMMS_IPC_CMD_PROPERTY_SET_STR);
	xmms_ipc_msg_put_uint32 (msg, id);
	xmms_ipc_msg_put_string (msg, source);
	xmms_ipc_msg_put_string (msg, key);
	xmms_ipc_msg_put_string (msg, value);

	res = xmmsc_send_msg (c, msg);

	return res;
}

/** 
 * Remove a custom field in the medialib associated with an entry.
 * Uses default source which is client/<clientname>
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_remove (xmmsc_connection_t *c, uint32_t id,
                                      const char *key)
{
	xmmsc_result_t *res;
	char tmp[256];

	x_check_conn (c, NULL);

	snprintf(tmp, 256, "client/%s", c->clientname);
	res = xmmsc_medialib_entry_property_remove_with_source (c, id, 
	                                                        tmp, key);
	return res;
}

/**
 * Remove a custom field in the medialib associated with an entry.
 * Identical to #xmmsc_result_entry_property_remove except with specifying
 * your own source.
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_remove_with_source (xmmsc_connection_t *c,
                                                  uint32_t id, 
                                                  const char *source,
                                                  const char *key)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB,
	                        XMMS_IPC_CMD_PROPERTY_REMOVE);
	xmms_ipc_msg_put_uint32 (msg, id);
	xmms_ipc_msg_put_string (msg, source);
	xmms_ipc_msg_put_string (msg, key);

	res = xmmsc_send_msg (c, msg);

	return res;
}

/** @} */

#define GOODCHAR(a) ((((a) >= 'a') && ((a) <= 'z')) || \
                     (((a) >= 'A') && ((a) <= 'Z')) || \
                     (((a) >= '0') && ((a) <= '9')) || \
                     ((a) == ':') || \
                     ((a) == '/') || \
                     ((a) == '-') || \
                     ((a) == '.') || \
                     ((a) == '_'))


char *
xmmsc_medialib_encode_url (const char *url, int narg, const char **args)
{
	static char hex[16] = "0123456789abcdef";
	int i = 0, j = 0, extra = 0;
	char *res;

	x_api_error_if (!url, "with a NULL url", NULL);

	for (i = 0; i < narg; i++) {
		extra += strlen (args[i]) + 2;
	}

	res = malloc (strlen(url) * 3 + 1 + extra);
	if (!res)
		return NULL;

	for (i = 0; url[i]; i++) {
		unsigned char chr = url[i];
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

	for (i = 0; i < narg; i++) {
		int l;
		l = strlen (args[i]);
		res[j] = (i == 0) ? '?' : '&';
		j++;
		memcpy (&res[j], args[i], l);
		j += l;
	}

	res[j] = '\0';

	return res;
}
