/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_medialib.h>
#include <xmms/xmms_util.h>

/* xform methods */
static gboolean xmms_html_setup (xmms_xform_plugin_t *xform);
static gboolean xmms_html_init (xmms_xform_t *xform);
static gboolean xmms_html_browse (xmms_xform_t *xform,
                                  const gchar *url, xmms_error_t *error);

/* for extracting the href attribute out of a tags */
static gchar* parse_tag (const gchar *tag, const gchar *plspath);

/* declare the plugin */
XMMS_XFORM_PLUGIN ("html",
                   "HTML Playlist Reader",
                   XMMS_VERSION,
                   "Reads HTML playlists",
                   xmms_html_setup);

static gboolean
xmms_html_setup (xmms_xform_plugin_t *xform)
{
	xmms_xform_methods_t methods;
	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_html_init;
	methods.browse = xmms_html_browse;
	xmms_xform_plugin_methods_set (xform, &methods);

	xmms_xform_plugin_indata_add (xform,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "text/html",
	                              NULL);

	xmms_xform_plugin_indata_add (xform,
	                              XMMS_STREAM_TYPE_MIMETYPE,
				      "application/x-xmms2-xml+html",
				      NULL);

	xmms_magic_extension_add ("text/html", "*.html");
	xmms_magic_extension_add ("text/html", "*.xhtml");

	xmms_magic_add ("html doctype", "text/html",
	                "0 string/c <!DOCTYPE HTML ",
	                NULL);

	xmms_magic_add ("html tag", "text/html",
	                "0 string/c <html ",
	                NULL);

	xmms_magic_add ("html header tag", "text/html",
	                "0 string/c <head ",
	                NULL);

	return TRUE;
}

static gboolean
xmms_html_init (xmms_xform_t *xform)
{
	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/x-xmms2-playlist-entries",
	                             XMMS_STREAM_TYPE_END);
	return TRUE;
}

static gboolean
xmms_html_browse (xmms_xform_t *xform, const gchar *url,
                  xmms_error_t *error)
{
	gchar buffer[XMMS_XFORM_MAX_LINE_SIZE];
	const gchar *plsurl;
	gchar *tagbeg, *aurl, *full;

	xmms_error_reset (error);
	plsurl = xmms_xform_get_url (xform);

	while (xmms_xform_read_line (xform, buffer, error)) {
		tagbeg = buffer;
		while ((tagbeg = strchr (tagbeg, '<'))) {
			if ((aurl = parse_tag (++tagbeg, plsurl))) {
				full = xmms_build_playlist_url (plsurl,
				                                aurl);
				xmms_xform_browse_add_symlink (xform,
				                               NULL, full);
				g_free (full);
				g_free (aurl);
			}
		}
	}

	return TRUE;
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
