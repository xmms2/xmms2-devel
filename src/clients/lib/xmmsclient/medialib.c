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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsc_stringport.h"

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
	return xmmsc_send_cmd (conn, XMMS_IPC_OBJECT_MEDIALIB, id,
	                       XMMSV_LIST_ENTRY_STR (arg), XMMSV_LIST_END);
}

/**
 * Search for a entry (URL) in the medialib db and return its ID number
 * @param conn The #xmmsc_connection_t
 * @param url The URL to search for
 */
xmmsc_result_t *
xmmsc_medialib_get_id (xmmsc_connection_t *conn, const char *url)
{
	xmmsc_result_t *res;
	char *enc_url;
	x_check_conn (conn, NULL);

	enc_url = xmmsc_medialib_encode_url (url);
	if (!enc_url)
		return NULL;

	res = xmmsc_medialib_get_id_encoded (conn, enc_url);

	free (enc_url);

	return res;
}

/**
 * Search for a entry (URL) in the medialib db and return its ID number
 *
 * Same as #xmmsc_medialib_get_id but expects a encoded URL instead
 *
 * @param conn The #xmmsc_connection_t
 * @param url The URL to search for
 */
xmmsc_result_t *
xmmsc_medialib_get_id_encoded (xmmsc_connection_t *conn, const char *url)
{
	x_check_conn (conn, NULL);

	return do_methodcall (conn, XMMS_IPC_CMD_GET_ID, url);
}

/**
 * Change the url property of an entry in the media library.  Note
 * that you need to handle the actual file move yourself.
 *
 * @param conn The #xmmsc_connection_t
 * @param entry The entry id you want to move
 * @param url The url to move it to
 */
xmmsc_result_t *
xmmsc_medialib_move_entry (xmmsc_connection_t *conn, int entry, const char *url)
{
	x_check_conn (conn, NULL);

	return xmmsc_send_cmd (conn, XMMS_IPC_OBJECT_MEDIALIB,
	                       XMMS_IPC_CMD_MOVE_URL,
	                       XMMSV_LIST_ENTRY_INT (entry),
	                       XMMSV_LIST_ENTRY_STR (url),
	                       XMMSV_LIST_END);
}

/**
 * Remove a entry from the medialib
 * @param conn The #xmmsc_connection_t
 * @param entry The entry id you want to remove
 */
xmmsc_result_t *
xmmsc_medialib_remove_entry (xmmsc_connection_t *conn, int entry)
{
	x_check_conn (conn, NULL);

	return xmmsc_send_cmd (conn, XMMS_IPC_OBJECT_MEDIALIB,
	                       XMMS_IPC_CMD_REMOVE_ID,
	                       XMMSV_LIST_ENTRY_INT (entry),
	                       XMMSV_LIST_END);
}

/**
 * Add a URL to the medialib. If you want to add mutiple files
 * you should call #xmmsc_medialib_import_path
 * @param conn The #xmmsc_connection_t
 * @param url URL to add to the medialib.
 */
xmmsc_result_t *
xmmsc_medialib_add_entry (xmmsc_connection_t *conn, const char *url)
{
	return xmmsc_medialib_add_entry_full (conn, url, NULL);
}

/**
 * Add a URL with arguments to the medialib.
 *
 * xmmsc_medialib_add_entry_args (conn, "file:///data/HVSC/C64Music/Hubbard_Rob/Commando.sid", 1, "subtune=2");
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

	enc_url = _xmmsc_medialib_encode_url_old (url, numargs, args);
	if (!enc_url)
		return NULL;

	res = xmmsc_medialib_add_entry_encoded (conn, enc_url);

	free (enc_url);

	return res;
}

/**
 * Add a URL with arguments to the medialib.
 *
 * @param conn The #xmmsc_connection_t
 * @param url URL to add to the medialib.
 * @param numargs The number of arguments
 * @param args array of numargs strings used as arguments
 */
xmmsc_result_t *
xmmsc_medialib_add_entry_full (xmmsc_connection_t *conn, const char *url, xmmsv_t *args)
{
	char *enc_url;
	xmmsc_result_t *res;

	x_check_conn (conn, NULL);

	enc_url = xmmsc_medialib_encode_url_full (url, args);
	if (!enc_url)
		return NULL;

	res = xmmsc_medialib_add_entry_encoded (conn, enc_url);

	free (enc_url);

	return res;
}

/**
 * Add a URL to the medialib. If you want to add mutiple files
 * you should call #xmmsc_medialib_import_path
 *
 * same as #xmmsc_medialib_add_entry but expects a encoded URL
 * instead
 *
 * @param conn The #xmmsc_connection_t
 * @param url URL to add to the medialib.
 */
xmmsc_result_t *
xmmsc_medialib_add_entry_encoded (xmmsc_connection_t *conn, const char *url)
{
	x_check_conn (conn, NULL);

	if (!_xmmsc_medialib_verify_url (url))
		x_api_error ("with a non encoded url", NULL);

	return do_methodcall (conn, XMMS_IPC_CMD_MLIB_ADD_URL, url);
}

/**
 * Import a all files recursivly from the directory passed
 * as argument.
 * @param conn #xmmsc_connection_t
 * @param path A directory to recursive search for mediafiles, this must
 * 		  include the protocol, i.e file://
 */
xmmsc_result_t *
xmmsc_medialib_import_path (xmmsc_connection_t *conn, const char *path)
{
	xmmsc_result_t *res;
	char *enc_path;

	x_check_conn (conn, NULL);

	enc_path = xmmsc_medialib_encode_url (path);
	if (!enc_path)
		return NULL;

	res = xmmsc_medialib_import_path_encoded (conn, enc_path);

	free (enc_path);

	return res;
}

/**
 * Import a all files recursivly from the directory passed as argument
 * which must already be url encoded. You probably want to use
 * #xmmsc_medialib_import_path unless you want to add a string that
 * comes as a result from the daemon, such as from
 * #xmmsc_xform_media_browse
 *
 * @param conn #xmmsc_connection_t
 * @param path A directory to recursive search for mediafiles, this must
 * 		  include the protocol, i.e file://
 */
xmmsc_result_t *
xmmsc_medialib_import_path_encoded (xmmsc_connection_t *conn,
                                    const char *path)
{
	x_check_conn (conn, NULL);

	if (!_xmmsc_medialib_verify_url (path))
		x_api_error ("with a non encoded url", NULL);

	return do_methodcall (conn, XMMS_IPC_CMD_PATH_IMPORT, path);
}

/**
 * Import a all files recursivly from the directory passed
 * as argument.
 * @param conn #xmmsc_connection_t
 * @param path A directory to recursive search for mediafiles, this must
 * 		  include the protocol, i.e file://
 */
xmmsc_result_t *
xmmsc_medialib_path_import (xmmsc_connection_t *conn, const char *path)
{
	return xmmsc_medialib_import_path (conn, path);
}

/**
 * Import a all files recursivly from the directory passed as argument
 * which must already be url encoded. You probably want to use
 * #xmmsc_medialib_path_import unless you want to add a string that
 * comes as a result from the daemon, such as from
 * #xmmsc_xform_media_browse
 *
 * @param conn #xmmsc_connection_t
 * @param path A directory to recursive search for mediafiles, this must
 * 		  include the protocol, i.e file://
 */
xmmsc_result_t *
xmmsc_medialib_path_import_encoded (xmmsc_connection_t *conn,
                                    const char *path)
{
	return xmmsc_medialib_import_path_encoded (conn, path);
}

/**
 * Rehash the medialib, this will check data in the medialib
 * still is the same as the data in files.
 *
 * @param conn #xmmsc_connection_t
 * @param id The id to rehash. Set it to 0 if you want to rehash
 * the whole medialib.
 */
xmmsc_result_t *
xmmsc_medialib_rehash (xmmsc_connection_t *conn, int id)
{
	x_check_conn (conn, NULL);

	return xmmsc_send_cmd (conn, XMMS_IPC_OBJECT_MEDIALIB,
	                       XMMS_IPC_CMD_REHASH,
	                       XMMSV_LIST_ENTRY_INT (id),
	                       XMMSV_LIST_END);
}

/**
 * Retrieve information about a entry from the medialib.
 */
xmmsc_result_t *
xmmsc_medialib_get_info (xmmsc_connection_t *c, int id)
{
	x_check_conn (c, NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_INFO,
	                       XMMSV_LIST_ENTRY_INT (id), XMMSV_LIST_END);
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
 * Associate a int value with a medialib entry. Uses default
 * source which is client/&lt;clientname&gt;
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_set_int (xmmsc_connection_t *c, int id,
                                       const char *key, int32_t value)
{
	char tmp[256];

	x_check_conn (c, NULL);

	snprintf (tmp, 256, "client/%s", c->clientname);
	return xmmsc_medialib_entry_property_set_int_with_source (c, id,
	                                                          tmp, key,
	                                                          value);
}

/**
 * Set a custom int field in the medialib associated with a entry,
 * the same as #xmmsc_medialib_entry_property_set_int but with specifing
 * your own source.
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_set_int_with_source (xmmsc_connection_t *c,
                                                   int id,
                                                   const char *source,
                                                   const char *key,
                                                   int32_t value)
{
	x_check_conn (c, NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_MEDIALIB,
	                       XMMS_IPC_CMD_PROPERTY_SET_INT,
	                       XMMSV_LIST_ENTRY_INT (id),
	                       XMMSV_LIST_ENTRY_STR (source),
	                       XMMSV_LIST_ENTRY_STR (key),
	                       XMMSV_LIST_ENTRY_INT (value),
	                       XMMSV_LIST_END);
}

/**
 * Associate a value with a medialib entry. Uses default
 * source which is client/&lt;clientname&gt;
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_set_str (xmmsc_connection_t *c, int id,
                                       const char *key, const char *value)
{
	char tmp[256];

	x_check_conn (c, NULL);

	snprintf (tmp, 256, "client/%s", c->clientname);
	return xmmsc_medialib_entry_property_set_str_with_source (c, id,
	                                                          tmp, key,
	                                                          value);
}

/**
 * Set a custom field in the medialib associated with a entry,
 * the same as #xmmsc_medialib_entry_property_set_str but with specifing
 * your own source.
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_set_str_with_source (xmmsc_connection_t *c,
                                                   int id,
                                                   const char *source,
                                                   const char *key,
                                                   const char *value)
{
	x_check_conn (c, NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_MEDIALIB,
	                       XMMS_IPC_CMD_PROPERTY_SET_STR,
	                       XMMSV_LIST_ENTRY_INT (id),
	                       XMMSV_LIST_ENTRY_STR (source),
	                       XMMSV_LIST_ENTRY_STR (key),
	                       XMMSV_LIST_ENTRY_STR (value),
	                       XMMSV_LIST_END);
}

/**
 * Remove a custom field in the medialib associated with an entry.
 * Uses default source which is client/&lt;clientname&gt;
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_remove (xmmsc_connection_t *c, int id,
                                      const char *key)
{
	char tmp[256];

	x_check_conn (c, NULL);

	snprintf (tmp, 256, "client/%s", c->clientname);
	return xmmsc_medialib_entry_property_remove_with_source (c, id,
	                                                         tmp, key);
}

/**
 * Remove a custom field in the medialib associated with an entry.
 * Identical to #xmmsc_medialib_entry_property_remove except with specifying
 * your own source.
 */
xmmsc_result_t *
xmmsc_medialib_entry_property_remove_with_source (xmmsc_connection_t *c,
                                                  int id,
                                                  const char *source,
                                                  const char *key)
{
	x_check_conn (c, NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_MEDIALIB,
	                       XMMS_IPC_CMD_PROPERTY_REMOVE,
	                       XMMSV_LIST_ENTRY_INT (id),
	                       XMMSV_LIST_ENTRY_STR (source),
	                       XMMSV_LIST_ENTRY_STR (key),
	                       XMMSV_LIST_END);
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


int
_xmmsc_medialib_verify_url (const char *url)
{
	int i;

	for (i = 0; url[i]; i++) {
		if (!(GOODCHAR (url[i]) || url[i] == '+' || url[i] == '%' || url[i] == '?'  || url[i] == '=' || url[i] == '&'))
			return 0;
	}
	return 1;
}

char *
_xmmsc_medialib_encode_url_old (const char *url, int narg, const char **args)
{
	static const char hex[16] = "0123456789abcdef";
	int i = 0, j = 0, extra = 0;
	char *res;

	x_api_error_if (!url, "with a NULL url", NULL);

	for (i = 0; i < narg; i++) {
		extra += strlen (args[i]) + 2;
	}

	res = malloc (strlen (url) * 3 + 1 + extra);
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

static void
_sum_len_string_dict (const char *key, xmmsv_t *val, void *userdata)
{
	const char *arg;
	int *extra = (int *) userdata;

	if (xmmsv_get_type (val) == XMMSV_TYPE_NONE) {
		*extra += strlen (key) + 1; /* Leave room for the ampersand. */
	} else if (xmmsv_get_string (val, &arg)) {
		/* Leave room for the equals sign and ampersand. */
		*extra += strlen (key) + strlen (arg) + 2;
	} else {
		x_api_warning ("with non-string argument");
	}
}

/**
 * Encodes an url with arguments stored in dict args.
 *
 * The encoded url is allocated using malloc and has to be freed by the user.
 *
 * @param url The url to encode.
 * @param args The dict with arguments, or NULL.
 * @return The encoded url
 */
char *
xmmsc_medialib_encode_url_full (const char *url, xmmsv_t *args)
{
	static const char hex[16] = "0123456789abcdef";
	int i = 0, j = 0, extra = 0, l;
	char *res;
	xmmsv_dict_iter_t *it;

	x_api_error_if (!url, "with a NULL url", NULL);

	if (args) {
		if (!xmmsv_dict_foreach (args, _sum_len_string_dict, (void *) &extra)) {
			return NULL;
		}
	}

	/* Provide enough room for the worst-case scenario (all characters of the
	   URL must be encoded), the args, and a \0. */
	res = malloc (strlen (url) * 3 + 1 + extra);
	if (!res) {
		return NULL;
	}

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

	if (args) {
		for (xmmsv_get_dict_iter (args, &it), i = 0;
		     xmmsv_dict_iter_valid (it);
		     xmmsv_dict_iter_next (it), i++) {

			const char *arg, *key;
			xmmsv_t *val;

			xmmsv_dict_iter_pair (it, &key, &val);
			l = strlen (key);
			res[j] = (i == 0) ? '?' : '&';
			j++;
			memcpy (&res[j], key, l);
			j += l;
			if (xmmsv_get_string (val, &arg)) {
				l = strlen (arg);
				res[j] = '=';
				j++;
				memcpy (&res[j], arg, l);
				j += l;
			}
		}
	}

	res[j] = '\0';

	return res;
}

/**
 * Encodes an url.
 *
 * The encoded url is allocated using malloc and has to be freed by the user.
 *
 * @param url The url to encode.
 * @return The encoded url
 */
char *
xmmsc_medialib_encode_url (const char *url)
{
	return xmmsc_medialib_encode_url_full (url, NULL);
}
