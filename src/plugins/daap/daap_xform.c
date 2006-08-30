/** @file daap_xform.c
 *  XMMS2 transform for accessing DAAP music shares.
 *
 *  Copyright (C) 2006 XMMS2 Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

/* XXX as of the current implementation, there is no method of logging out
 * of the servers.
 */

#include "xmms/xmms_defs.h"
#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#include "daap_cmd.h"
#include "daap_util.h"
#include "daap_mdns_browse.h"

#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>

#define DEFAULT_DAAP_PORT 3689

/*
 * Type definitions
 */

typedef struct {
	gchar *url, *host;
	guint port;

	GIOChannel *channel;

	xmms_error_t status;
} xmms_daap_data_t;

typedef struct {
	gboolean logged_in;

	guint session_id;
	guint revision_id;
	guint request_id;
} xmms_daap_login_data_t;

static GHashTable *login_sessions = NULL;

/*
 * Function prototypes
 */

static gboolean
xmms_daap_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean
xmms_daap_init (xmms_xform_t *xform);
static void
xmms_daap_destroy (xmms_xform_t *xform);
static gint
xmms_daap_read (xmms_xform_t *xform, void *buffer,
                gint len, xmms_error_t *error);
static GList *
xmms_daap_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN ("daap",
                   "DAAP access plugin",
                   "SoC",
                   "Accesses iTunes (DAAP) music shares",
                   xmms_daap_plugin_setup);

static gboolean
get_data_from_url (const gchar *url, gchar **host, guint *port, gchar **cmd)
{
	gint host_len;
	const gchar *host_begin, *cmd_begin, *port_begin;

	host_begin = url;
	host_begin += sizeof (gchar) * strlen ("daap://");

	cmd_begin = strstr (host_begin, "/");

	port_begin = strstr (host_begin, ":");
	if ((NULL == port_begin) || (port_begin > cmd_begin)) {
		*port = DEFAULT_DAAP_PORT;
	} else {
		*port = atoi (port_begin + sizeof (gchar) * strlen (":"));
	}

	if (NULL == cmd_begin && NULL == port_begin) {
		host_len = (gint) (host_begin - (strstr (url, "daap://") +
		                                 strlen ("daap://")));
	} else if (NULL == port_begin) {
		host_len = (gint) (cmd_begin - host_begin);
	} else {
		host_len = (gint) (port_begin - host_begin);
	}
	*host = (gchar *) g_malloc0 (host_len+1);
	if (! *host) {
		return FALSE;
	}
	memcpy (*host, host_begin, host_len);

	if (NULL != cmd) {
		*cmd = (gchar *) g_malloc0 (sizeof (gchar) * (strlen (cmd_begin)+1));
		if (! *cmd) {
			g_free (*host);
			return FALSE;
		}
		strncpy (*cmd, cmd_begin, sizeof (gchar) * strlen (cmd_begin));
	}

	return TRUE;
}

static gboolean
xmms_daap_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_daap_init;
	methods.destroy = xmms_daap_destroy;
	methods.read = xmms_daap_read;
	methods.browse = xmms_daap_browse;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url",
	                              XMMS_STREAM_TYPE_URL,
	                              "daap://*",
	                              XMMS_STREAM_TYPE_END);

	daap_mdns_initialize ();

	if (!login_sessions) {
		login_sessions = g_hash_table_new (g_str_hash, g_str_equal);
	}

	return TRUE;
}

static GList *
add_song_to_list (GList *url_list, cc_item_record_t *song, gchar* host, guint port)
{
	GHashTable *h = NULL;
	gchar *songurl;
	gchar *sid = g_malloc (G_ASCII_DTOSTR_BUF_SIZE);

	g_ascii_dtostr (sid, G_ASCII_DTOSTR_BUF_SIZE, song->dbid);
	songurl = g_strdup_printf ("daap://%s:%d/%s.%s",
	                           host, port, sid, song->song_format);

	h = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

	g_hash_table_insert (h, "artist",
	                     xmms_object_cmd_value_str_new (song->song_data_artist));
	XMMS_DBG ("%s", song->song_data_artist);
	g_hash_table_insert (h, "title",
	                     xmms_object_cmd_value_str_new (song->iname));

	url_list = xmms_xform_browse_add_entry (url_list, songurl, FALSE, h);

	g_hash_table_destroy (h);
	g_free (sid);
	g_free (songurl);

	return url_list;
}

static GList *
daap_get_urls_from_server (daap_mdns_server_t *server, GList *url_list)
{
	GSList *dbid_list = NULL;
	GSList *song_list = NULL, *song_el;
	cc_item_record_t *db_data;
	xmms_daap_login_data_t *login_data;
	gchar *host, *hash;
	guint port;

	host = server->address;
	port = server->port;

	hash = g_malloc0 (strlen (host) + 5 + 1 + 1);
	g_sprintf (hash, "%s:%u", host, port);

	login_data = g_hash_table_lookup (login_sessions, hash);

	if (!login_data) {
		login_data = (xmms_daap_login_data_t *)
		             g_malloc0 (sizeof (xmms_daap_login_data_t));

		login_data->session_id = daap_command_login (host, port, 0);
		if (login_data->session_id == 0) {
			return NULL;
		}

		login_data->revision_id = daap_command_update (host, port,
		                                               login_data->session_id,
		                                               0);

		login_data->request_id = 1;
		login_data->logged_in = TRUE;

		g_hash_table_insert (login_sessions, hash, login_data);
	} else {
		login_data->revision_id = daap_command_update (host, port,
		                                               login_data->session_id,
		                                               0);
	}

	dbid_list = daap_command_db_list (host, port, login_data->session_id,
	                                  login_data->revision_id, 0);
	if (!dbid_list) {
		return NULL;
	}

	/* XXX i've never seen more than one db per server out in the wild,
	 *     let's hope that never changes *wink*
	 *     just use the first db in the list */
	db_data = (cc_item_record_t *) dbid_list->data;
	song_list = daap_command_song_list (host, port, login_data->session_id,
	                                    login_data->revision_id,
	                                    0, db_data->dbid);

	song_el = song_list;
	for ( ; song_el != NULL; song_el = g_slist_next (song_el)) {
		url_list = add_song_to_list (url_list, song_el->data, host, port);
	}

	/* cleanup */
	g_slist_foreach (dbid_list, (GFunc) cc_item_record_free, NULL);
	g_slist_foreach (song_list, (GFunc) cc_item_record_free, NULL);
	g_slist_free (dbid_list);
	g_slist_free (song_list);

	return url_list;
}

/*
 * Member functions
 */

static gboolean
xmms_daap_init (xmms_xform_t *xform)
{
	gint dbid;
	GSList *dbid_list = NULL;
	xmms_daap_data_t *data;
	xmms_daap_login_data_t *login_data;
	const gchar *url;
	gchar *command, *hash;
	guint filesize;

	if (!xform) {
		return FALSE;
	}

	url = xmms_xform_indata_get_str (xform, XMMS_STREAM_TYPE_URL);

	if (!url) {
		return FALSE;
	}

	data = xmms_xform_private_data_get (xform);

	if (!data) {
		data = g_malloc0 (sizeof (xmms_daap_data_t));
		if (!data) {
			return FALSE;
		}
	}

	data->url = g_strdup (url);
	get_data_from_url (data->url, &(data->host), &(data->port), &command);

	hash = g_malloc0 (strlen (data->host) + 5 + 1 + 1);
	g_sprintf (hash, "%s:%u", data->host, data->port);

	login_data = g_hash_table_lookup (login_sessions, hash);
	if (!login_data) {
		XMMS_DBG ("creating login data for %s", hash);
		login_data = (xmms_daap_login_data_t *)
		             g_malloc0 (sizeof (xmms_daap_login_data_t));

		login_data->session_id = daap_command_login (data->host, data->port, 0);
		if (login_data->session_id == 0) {
			return FALSE;
		}

		login_data->request_id = 1;
		login_data->logged_in = TRUE;

		g_hash_table_insert (login_sessions, hash, login_data);
	}

	login_data->revision_id = daap_command_update (data->host, data->port,
	                                               login_data->session_id,
	                                               login_data->request_id);
	dbid_list = daap_command_db_list (data->host, data->port,
	                                  login_data->session_id,
	                                  login_data->revision_id,
	                                  login_data->request_id);
	if (!dbid_list) {
		return FALSE;
	}

	/* XXX: see XXX in the browse function above */
	dbid = ((cc_item_record_t *) dbid_list->data)->dbid;
	/* want to request a stream, but don't read the data yet */
	data->channel = daap_command_init_stream (data->host, data->port,
	                                          login_data->session_id,
	                                          login_data->revision_id,
	                                          login_data->request_id, dbid,
	                                          command, &filesize);
	if (! data->channel) {
		return FALSE;
	}
	login_data->request_id++;

	xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE,
	                             filesize);

	xmms_xform_private_data_set (xform, data);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/octet-stream",
	                             XMMS_STREAM_TYPE_END);

	g_slist_foreach (dbid_list, (GFunc) cc_item_record_free, NULL);
	g_slist_free (dbid_list);
	g_free (command);

	return TRUE;
}

static void
xmms_daap_destroy (xmms_xform_t *xform)
{
	xmms_daap_data_t *data;

	data = xmms_xform_private_data_get (xform);

	g_io_channel_shutdown (data->channel, TRUE, NULL);
	g_io_channel_unref (data->channel);

	g_free (data->url);
	g_free (data->host);
	g_free (data);
}

static gint
xmms_daap_read (xmms_xform_t *xform, void *buffer, gint len, xmms_error_t *error)
{
	xmms_daap_data_t *data;
	gsize read_bytes = 0;
	GIOStatus status;

	data = xmms_xform_private_data_get (xform);

	/* request is performed, header is stripped. now read the data. */
	while (read_bytes == 0) {
		status = g_io_channel_read_chars (data->channel, buffer, len,
		                                  &read_bytes, NULL);
		if (status == G_IO_STATUS_EOF || status == G_IO_STATUS_ERROR) {
			break;
		}
	}
	
	return read_bytes;
}

static GList *
xmms_daap_browse (xmms_xform_t *xform, const gchar *url,
                  xmms_error_t *error)
{
	GSList *server_list, *sl;
	GList *url_list = NULL;
	gchar *host;
	guint port;
	daap_mdns_server_t *mdns_serv;

	if (! get_data_from_url (url, &host, &port, NULL)) {
		return NULL;
	}

	mdns_serv = g_malloc0 (sizeof (daap_mdns_server_t));

	mdns_serv->address = g_strdup (host);
	mdns_serv->port = port;

	url_list = daap_get_urls_from_server (mdns_serv, url_list);
	/* after this point, mdns_serv is used only as a reference; the actual
	 * data is no longer needed. */
	g_free (mdns_serv);

	/* if url_list is empty, either the server specified by host has no songs
	 * (unlikely), or communication with the server failed, probably due to a
	 * nonexistant or bogus IP. at any rate, resort to mdns discovery
	 * in this case; hostname resolution will be handled here. */
	if (0 == g_list_length (url_list)) {

		sl = daap_mdns_get_server_list ();
	
		server_list = sl;
		for ( ; server_list != NULL; server_list = g_slist_next (server_list)) {
			gchar *str;
			GHashTable *h = NULL;
			mdns_serv = server_list->data;
	
			if (! strcmp (mdns_serv->mdns_hostname, host)) {
				g_list_free (url_list);
				url_list = daap_get_urls_from_server (mdns_serv, url_list);
				break;
			}

			str = g_strdup_printf ("daap://%s:%d", mdns_serv->address, mdns_serv->port);
	
			h = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
	
			g_hash_table_insert (h, "servername",
			                     xmms_object_cmd_value_str_new (mdns_serv->server_name));
			g_hash_table_insert (h, "ip",
			                     xmms_object_cmd_value_str_new (mdns_serv->address));
			g_hash_table_insert (h, "name",
			                     xmms_object_cmd_value_str_new (mdns_serv->mdns_hostname));
			g_hash_table_insert (h, "port",
			                     xmms_object_cmd_value_int_new (mdns_serv->port));
			/* TODO implement the machinery to allow for this */
			//g_hash_table_insert (h, "passworded",
			//           xmms_object_cmd_value_int_new (mdns_serv->need_auth));
			//g_hash_table_insert (h, "version",
			//           xmms_object_cmd_value_str_new (mdns_serv->version));
	
			url_list = xmms_xform_browse_add_entry (url_list, str, TRUE, h);
	
			g_hash_table_destroy (h);
			g_free (str);

		}
		g_slist_free (sl);
	}

	g_free (host);
	return url_list;
}

