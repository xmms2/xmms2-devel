/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003  Peter Alm, Tobias Rundström, Anders Gustafsson
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




#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/playlist.h"
#include "xmms/playlist_entry.h"
#include "xmms/plsplugins.h"
#include "xmms/util.h"
#include "xmms/xmms.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>

/*
 * Function prototypes
 */

static gboolean xmms_html_can_handle (const gchar *mime);
static gboolean xmms_html_read_playlist (xmms_playlist_plugin_t *plsplugin,
                                         xmms_transport_t *transport,
                                         xmms_playlist_t *playlist);
static gboolean xmms_html_write_playlist (xmms_playlist_plugin_t *plsplugin,
                                          xmms_playlist_t *playlist,
                                          gchar *filename);

static gchar *escape_html (const gchar *in);
static gboolean valid_suffix (gchar **suffix, gchar *path);
static gchar* parse_tag (const gchar *tag, const gchar *plspath);
static gchar* build_url (const gchar *plspath, const gchar *file);
static gchar* path_get_body (const gchar *path);

static gchar html_header[] =
"<?xml version=\"1.0\" encoding=\"utf8\"?>\n"
"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">\n"
"<head>\n"
"	<title>XMMS2 Playlist</title>\n"
"	<meta name=\"generator\" content=\"XMMS2\"/>\n"
"	<meta http-equiv=\"content-type\" content=\"text/xhtml; charset=utf8\"/>\n"
"	<meta http-equiv=\"content-style-type\" content=\"text/css\"/>\n"
"	<link href=\"playlist.css\" rel=\"stylesheet\" type=\"text/css\"/>\n"
"</head>\n"
"<body>\n"
"	<h1>XMMS2 Playlist</h1>\n"
"\n"
"	<div id=\"container\">\n"
"		<p>\n"
"			Number of tracks: %i\n"
"			<br/>\n"
"			Total playtime: %i:%02i:%02i\n"
"		</p>\n"
"\n"
"		<h2>Tracks</h2>\n"
"\n"
"		<ol id=\"playlist\">\n";

static gchar html_entry_even[] = "\t\t\t<li class=\"entry_even\">%s</li>\n";
static gchar html_entry_odd[] = "\t\t\t<li class=\"entry_odd\">%s</li>\n";
static gchar html_footer[] =
"		</div>\n"
"	</div>\n"
"</body>\n"
"</html>\n";

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_PLAYLIST, "html",
	                          "HTML Playlist " XMMS_VERSION,
	                          "HTML Playlist reader");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_html_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ_PLAYLIST, xmms_html_read_playlist);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE_PLAYLIST, xmms_html_write_playlist);

	xmms_plugin_config_value_register (plugin, "suffixes", "mp3,ogg,flac,wav,spx,sid", NULL, NULL);

	return plugin;
}

static gboolean
xmms_html_can_handle (const gchar *mime)
{
	g_return_val_if_fail (mime, FALSE);

	XMMS_DBG ("xmms_html_can_handle (%s)", mime);

	if ((g_strncasecmp (mime, "text/html", 9) == 0))
		return TRUE;

	return FALSE;
}

static gboolean
xmms_html_read_playlist (xmms_playlist_plugin_t *plsplugin,
                         xmms_transport_t *transport,
                         xmms_playlist_t *playlist)
{
	gchar *buffer, *plsurl;
	gchar **tags;
	gchar **suffix;
	xmms_config_value_t *val;

	gint cnt, readlen, buflen;

	g_return_val_if_fail (plsplugin, FALSE);
	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (playlist, FALSE);

	buflen = xmms_transport_size (transport);
	if (buflen == 0) {
		XMMS_DBG ("Empty playlist, nothing to add here");
		return TRUE;
	}

	if (buflen == -1) {
		buflen = 4096;
	}

	buffer = g_malloc0 (buflen);
	g_return_val_if_fail (buffer, FALSE);

	readlen = 0;
	while (readlen < buflen) {
		xmms_error_t error;
		gint ret;

		ret = xmms_transport_read (transport, buffer + readlen, buflen - readlen, &error);

		XMMS_DBG ("Got %d bytes", ret);
		if (ret <= 0) {

			if (readlen > 0) {
				break;
			}

			g_free (buffer);
			return FALSE;
		}

		readlen += ret;
		g_assert (readlen >= 0);
	}

	tags = g_strsplit (buffer, "<", 0);
	g_free (buffer);

	val = xmms_plugin_config_lookup (plsplugin->plugin, "suffixes");
	suffix = g_strsplit (xmms_config_value_string_get (val), ",", 0);

	plsurl = xmms_util_decode_path (xmms_transport_url_get (transport));

	for (cnt = 0; tags[cnt] != NULL; cnt++) {
		gchar *url, *full, *enc;
		xmms_playlist_entry_t *entry = NULL;

		url = parse_tag (tags[cnt], plsurl);
		if (!url) {
			continue;
		}

		if (!valid_suffix (suffix, url)) {
			g_free (url);
			continue;
		}

		full = build_url (plsurl, url);

		enc = xmms_util_encode_path (full);
		entry = xmms_playlist_entry_new (enc);
		XMMS_DBG ("Adding %s", xmms_playlist_entry_url_get (entry));
		xmms_playlist_add (playlist, entry);

		g_free (enc);
		g_free (url);
		g_free (full);
		xmms_object_unref (entry);
	}

	g_free (plsurl);
	g_strfreev (suffix);
	g_strfreev (tags);

	return TRUE;
}

static gboolean
xmms_html_write_playlist (xmms_playlist_plugin_t *plsplugin,
                          xmms_playlist_t *playlist, gchar *filename)
{
	FILE *fp;
	GList *list, *l;
	gboolean is_even = TRUE;
	guint num_entries = 0, total_len = 0;
	xmms_error_t err;

	g_return_val_if_fail (plsplugin, FALSE);
	g_return_val_if_fail (playlist, FALSE);
	g_return_val_if_fail (filename, FALSE);

	xmms_error_reset (&err);

	fp = fopen (filename, "w");
	if (!fp)
		return FALSE;

	list = xmms_playlist_list (playlist, &err);
	if (!list) {
		fclose (fp);
		return FALSE;
	}

	/* get the playlists total playtime */
	for (l = list; l; l = l->next) {
		xmms_playlist_entry_t *entry;

		num_entries++;

		entry = xmms_playlist_get_byid (playlist,
		                                GPOINTER_TO_UINT (l->data));

		total_len += xmms_playlist_entry_property_get_int (entry,
			XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION);

		xmms_object_unref (entry);
	}

	fprintf (fp, html_header, num_entries,
	         total_len / 3600000, (total_len / 60000) % 60,
	         (total_len / 1000) % 60);

	while (list) {
		gchar buf[256], *artist, *title;
		xmms_playlist_entry_t *entry;
		guint len;

		entry = xmms_playlist_get_byid (playlist,
		                                GPOINTER_TO_UINT (list->data));

		artist = escape_html (xmms_playlist_entry_property_get (entry,
			XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST));
		title = escape_html (xmms_playlist_entry_property_get (entry,
			XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE));
		len = xmms_playlist_entry_property_get_int (entry,
			XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION);

		g_snprintf (buf, sizeof (buf), "%s - %s (%02i:%02i)",
		            artist, title,
		            len / 60000, (len / 1000) % 60);

		g_free (artist);
		g_free (title);

		fprintf (fp, is_even ? html_entry_even : html_entry_odd, buf);
		is_even = !is_even;

		xmms_object_unref (entry);

		list = g_list_remove_link (list, list);
	}

	fprintf (fp, html_footer);

	fclose (fp);

	return TRUE;
}

static gchar *
escape_html (const gchar *in)
{
	gchar *ret = NULL, *retptr;
	const gchar *inptr;
	gsize len = 0;
	gboolean need_escape = FALSE;

	/* check whether we need to escape this string at all,
	 * and if we do, get the required length of the new buffer.
	 */
	for (inptr = in; *inptr; inptr++) {
		switch (*inptr) {
			case '"':
				need_escape = TRUE;
				len += 6; /* &quot; */
				break;
			case '&':
				need_escape = TRUE;
				len += 5; /* &amp; */
				break;
			case '>':
			case '<':
				need_escape = TRUE;
				len += 4; /* &gt; resp &lt; */
				break;
			default:
				len++;
		}
	}

	if (!need_escape)
		return g_strdup (in);

	len++; /* terminating NUL */
	retptr = ret = g_malloc (len);
	*ret = '\0';

	for (inptr = in; *inptr; inptr++) {
		gint n;

		switch (*inptr) {
			case '"':
				n = g_snprintf (retptr, len, "&quot;");
				break;
			case '&':
				n = g_snprintf (retptr, len, "&amp;");
				break;
			case '>':
				n = g_snprintf (retptr, len, "&gt;");
				break;
			case '<':
				n = g_snprintf (retptr, len, "&lt;");
				break;
			default:
				n = g_snprintf (retptr, len, "%c", *inptr);
		}

		len -= n;
		retptr += n;
	}

	return ret;
}

static gboolean
valid_suffix (gchar **suffix, gchar *path)
{
	guint current;

	g_return_val_if_fail (suffix, FALSE);
	g_return_val_if_fail (path, FALSE);

	for (current = 0; suffix[current] != NULL; current++) {
		if (g_str_has_suffix (path, g_strstrip (suffix[current])))
			return TRUE;
	}

	return FALSE;
}

static gchar *
build_url (const gchar *plspath, const gchar *file) {
	gchar *url;
	gchar *path;

	g_return_val_if_fail (plspath, NULL);
	g_return_val_if_fail (file, NULL);

	if (strstr (file, "://") != NULL) {
		return g_strdup (file);
	}

	if (file[0] == '/') {

		path = path_get_body (plspath);
		url = g_build_filename (path, file, NULL);

		g_free (path);
		return url;
	}

	path = g_path_get_dirname (plspath);
	url = g_build_filename (path, file, NULL);

	g_free (path);
	return url;
}

/*
 * proto://domain/dir/file -> proto://domain
 * domain/dir/file -> domain
 **/

static gchar *
path_get_body (const gchar *path)
{
	gchar *beg, *end;

	g_return_val_if_fail (path, NULL);

	beg = strstr (path, "://");

	if (!beg) {
		return g_strndup (path, strcspn (path, "/"));
	}

	beg += 3;
	end = strchr (beg, '/');

	if (!end) {
		return g_strdup (path);
	}

	return g_strndup (path, end - path);
}

static gchar *
parse_tag (const gchar *tag, const gchar *plspath)
{
	size_t urlend;
	gchar *href, *url;

	g_return_val_if_fail (tag, NULL);
	g_return_val_if_fail (plspath, NULL);

	if (g_ascii_strncasecmp (tag, "a ", 2) != 0) {
		return NULL;
	}

	href = strstr (tag, "href=\"");

	if (!href) {
		href = strstr (tag, "HREF=\"");
	}

	if (!href) {
		return NULL;
	}

	urlend = strcspn (href + 6, "\"");

	url = g_strndup (href + 6, urlend);

	return url;
}

