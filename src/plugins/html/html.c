/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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




#include "xmms/xmms_plugin.h"
#include "xmms/xmms_transport.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_plsplugins.h"
#include "xmms/xmms_medialib.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>

/*
 * Function prototypes
 */

static gboolean xmms_html_read_playlist (xmms_transport_t *transport, guint playlist_id);
static GString *xmms_html_write_playlist (guint32 *list);

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

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_PLAYLIST,
	                          XMMS_PLAYLIST_PLUGIN_API_VERSION,
	                          "html",
	                          "HTML Playlist",
	                          XMMS_VERSION,
	                          "HTML Playlist reader");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ_PLAYLIST, xmms_html_read_playlist);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE_PLAYLIST, xmms_html_write_playlist);

	xmms_plugin_config_property_register (plugin, "suffixes",
	                                      "mp3,ogg,flac,wav,spx,sid",
	                                      NULL, NULL);

	xmms_plugin_magic_add (plugin, "html w/ doctype", "text/html",
	                       "0 string <!DOCTYPE html ", NULL);

	/* we accept broken HTML, too */
	xmms_plugin_magic_add (plugin, "html tag", "text/html",
	                       "0 string <html ", NULL);
	xmms_plugin_magic_add (plugin, "html header tag", "text/html",
	                       "0 string <head ", NULL);

	/* XHTML */
	xmms_plugin_magic_add (plugin, "xml tag", "text/html",
	                       "0 string <?xml ", NULL);

	return plugin;
}

static gboolean
xmms_html_read_playlist (xmms_transport_t *transport,
                         guint32 playlist_id)
{
	gchar *buffer;
	const gchar *plsurl;
	gchar **tags;
	gchar **suffix;
	xmms_config_property_t *val;
	xmms_medialib_session_t *session;
	xmms_error_t error;

	gint cnt, readlen, buflen;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (playlist_id, FALSE);

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
		gint ret;

		ret = xmms_transport_read (transport, buffer + readlen,
		                           buflen - readlen, &error);

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

	val = xmms_config_lookup ("playlist.html.suffixes");
	suffix = g_strsplit (xmms_config_property_get_string (val), ",", 0);

	plsurl = xmms_transport_url_get (transport);

	session = xmms_medialib_begin_write ();

	for (cnt = 0; tags[cnt] != NULL; cnt++) {
		gchar *url, *full;
		xmms_medialib_entry_t entry;

		url = parse_tag (tags[cnt], plsurl);
		if (!url) {
			continue;
		}

		if (!valid_suffix (suffix, url)) {
			g_free (url);
			continue;
		}

		full = build_url (plsurl, url);

		entry = xmms_medialib_entry_new (session, full, &error);
		if (entry)
			xmms_medialib_playlist_add (session, playlist_id, entry);

		g_free (url);
		g_free (full);
	}

	g_strfreev (suffix);
	g_strfreev (tags);

	xmms_medialib_end (session);

	return TRUE;
}

static GString *
xmms_html_write_playlist (guint32 *list)
{
	GString *ret;
	xmms_medialib_session_t *session;
	gboolean is_even = TRUE;
	guint num_entries = 0, total_len = 0;
	guint i;

	g_return_val_if_fail (list, FALSE);

	session = xmms_medialib_begin ();

	/* get the playlists total playtime */
	while (list[num_entries]) {
		xmms_medialib_entry_t entry = list[num_entries];

		total_len += xmms_medialib_entry_property_get_int (session, entry,
		                                                   XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION);

		num_entries++;
	}

	ret = g_string_new (NULL);
	g_string_append_printf (ret, html_header, num_entries,
	                        total_len / 3600000, (total_len / 60000) % 60,
	                        (total_len / 1000) % 60);

	i = 0;

	while (list[i]) {
		gchar buf[256], *artist, *title, *url;
		xmms_medialib_entry_t entry;
		guint len;

		entry = list[i];

		artist = escape_html (xmms_medialib_entry_property_get_str (session, entry,
		    XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST));
		title = escape_html (xmms_medialib_entry_property_get_str (session, entry,
		    XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE));
		len = xmms_medialib_entry_property_get_int (session, entry,
		    XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION);
		url = escape_html (xmms_medialib_entry_property_get_str (session, entry,
		    XMMS_MEDIALIB_ENTRY_PROPERTY_URL));

		if (!artist && !title) {
			g_snprintf (buf, sizeof (buf), "%s (%02i:%02i)",
			            url, len / 60000, (len / 1000) % 60);
		} else {
			g_snprintf (buf, sizeof (buf), "%s - %s (%02i:%02i)",
			            artist ? artist : "Unknown artist",
			            title ? title : "Unknown title",
			            len / 60000, (len / 1000) % 60);
		}

		if (artist)
			g_free (artist);
		if (title)
			g_free (title);
		if (url)
			g_free (url);

		g_string_append_printf (ret, is_even ? html_entry_even
		                                     : html_entry_odd, buf);
		is_even = !is_even;

		i++;
	}

	xmms_medialib_end (session);
	g_string_append (ret, html_footer);

	return ret;
}

static gchar *
escape_html (const gchar *in)
{
	gchar *ret = NULL, *retptr;
	const gchar *inptr;
	gsize len = 0;
	gboolean need_escape = FALSE;

	if (!in)
		return NULL;

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
build_url (const gchar *plspath, const gchar *file)
{
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
