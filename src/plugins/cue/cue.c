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
#include <xmmsc/xmmsc_util.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_util.h>

#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * Type definitions
 */

/*
 * Function prototypes
 */

/*static gboolean xmms_cue_read_playlist (xmms_transport_t *transport, guint playlist_id);*/
static gboolean xmms_cue_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_cue_init (xmms_xform_t *xform);
static gboolean xmms_cue_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error);
static void xmms_cue_destroy (xmms_xform_t *xform);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN ("cue",
                   "CUE reader",
                   XMMS_VERSION,
                   "Playlist parser for cue files",
                   xmms_cue_plugin_setup);

static gboolean
xmms_cue_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_cue_init;
	methods.destroy = xmms_cue_destroy;
	methods.browse = xmms_cue_browse;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-cue",
	                              NULL);

	xmms_magic_extension_add ("application/x-cue", "*.cue");

	return TRUE;
}

static gboolean
xmms_cue_init (xmms_xform_t *xform)
{
	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/x-xmms2-playlist-entries",
	                             XMMS_STREAM_TYPE_END);
	return TRUE;
}

/*
 * Member functions
 */

static gchar *
skip_white_space (gchar *line)
{
	gchar *p = line;
	while (*p && isspace (*p)) p ++;
	return p;
}

static gchar *
skip_to_char (gchar *p, gchar c)
{
	while (*p && *p != c) p ++;
	return p;
}

static void
save_to_char (gchar *p, gchar c, gchar *f)
{
	int i = 0;

	while (p[i] && p[i] != c) {
		f[i] = p[i];
		i ++;
	}

	f[i] = '\0';
}

typedef struct {
	gchar file[XMMS_PATH_MAX];
	gchar title[1024];
	gchar artist[1024];
	gchar album[1024];
	gint index;
	gint index2;
	GList *tracks;
} cue_track;

static void
add_index (cue_track *tr, gchar *idx)
{
	guint ms = 0;
	guint tmp;

	gchar **a = g_strsplit (idx, ":", 0);

	if (a[0] != NULL) {
		tmp = strtol (a[0], NULL, 10);
		ms = tmp * 60000;

		if (a[1] != NULL) {
			tmp = strtol (a[1], NULL, 10);
			ms += tmp * 1000;

			if (a[2] != NULL) {
				tmp = strtol (a[2], NULL, 10);
				ms += (guint)((gfloat)tmp * 1.0/75.0) * 1000.0;
			}
		}
	}

	if (!tr->index) {
		tr->index = ms;
	} else {
		tr->index2 = ms;
	}

	g_strfreev (a);
}

static void
add_track (xmms_xform_t *xform, cue_track *tr)
{
	GList *n;
	gchar *file;

	tr->tracks = g_list_reverse (tr->tracks);
	n = tr->tracks;

	file = xmms_build_playlist_url (xmms_xform_get_url (xform), tr->file);

	while (n) {
		gchar arg0[32], arg1[32];
		gchar *arg[2] = { arg0, arg1 };
		gint numargs = 1;
		cue_track *t = n->data;
		if (!t) {
			continue;
		}

		g_snprintf (arg0, sizeof (arg0), "startms=%d",
		            t->index2 ? t->index2 : t->index);

		if (n->next && n->next->data) {
			cue_track *t2 = n->next->data;
			g_snprintf (arg1, sizeof (arg1), "stopms=%d", t2->index);
			numargs = 2;
		}

		xmms_xform_browse_add_symlink_args (xform, NULL, file, numargs, arg);
		xmms_xform_browse_add_entry_property_int (xform, "intsort", t->index);
		if (*t->title) {
			xmms_xform_browse_add_entry_property_str (xform, "title", t->title);
		}
		if (*t->artist || *tr->artist) {
			xmms_xform_browse_add_entry_property_str (xform, "artist",
			                                          (*t->artist)? t->artist : tr->artist);
		}
		if (*tr->album) {
			xmms_xform_browse_add_entry_property_str (xform, "album", tr->album);
		}

		g_free (t);
		n = g_list_delete_link (n, n);
	}

	g_free (file);

	tr->file[0] = '\0';
	tr->tracks = NULL;
}

static gboolean
xmms_cue_browse (xmms_xform_t *xform,
                 const gchar *url,
                 xmms_error_t *error)
{
	gchar line[XMMS_XFORM_MAX_LINE_SIZE];
	cue_track track;
	gchar *p;

	g_return_val_if_fail (xform, FALSE);

	memset (&track, 0, sizeof (cue_track));

	if (!xmms_xform_read_line (xform, line, error)) {
		xmms_error_set (error, XMMS_ERROR_INVAL, "error reading cue-file!");
		return FALSE;
	}

	do {
		p = skip_white_space (line);
		if (g_ascii_strncasecmp (p, "FILE", 4) == 0) {
			if (track.file[0]) {
				add_track (xform, &track);
			}
			p = skip_to_char (p, '"');
			p ++;
			save_to_char (p, '"', track.file);
		} else if (g_ascii_strncasecmp (p, "TRACK", 5) == 0) {
			p = skip_to_char (p, ' ');
			p = skip_white_space (p);
			p = skip_to_char (p, ' ');
			p = skip_white_space (p);
			if (g_ascii_strncasecmp (p, "AUDIO", 5) == 0) {
				cue_track *t = g_new0 (cue_track, 1);
				track.tracks = g_list_prepend (track.tracks, t);
			}
		} else if (g_ascii_strncasecmp (p, "INDEX", 5) == 0) {
			cue_track *t = g_list_nth_data (track.tracks, 0);
			if (!t) {
				continue;
			}
			p = skip_to_char (p, ' ');
			p = skip_white_space (p);
			p = skip_to_char (p, ' ');
			p = skip_white_space (p);
			add_index (t, p);
		} else if (g_ascii_strncasecmp (p, "TITLE", 5) == 0) {
			cue_track *t = g_list_nth_data (track.tracks, 0);

			p = skip_to_char (p, '"');
			p ++;

			if (!t) {
				save_to_char (p, '"', track.album);
			} else {
				save_to_char (p, '"', t->title);
			}
		} else if (g_ascii_strncasecmp (p, "PERFORMER", 9) == 0) {
			cue_track *t = g_list_nth_data (track.tracks, 0);

			p = skip_to_char (p, '"');
			p ++;

			if (!t) {
				save_to_char (p, '"', track.artist);
			} else {
				save_to_char (p, '"', t->artist);
			}
		}
	} while (xmms_xform_read_line (xform, line, error));

	if (track.file[0]) {
		add_track (xform, &track);
	}

	xmms_error_reset (error);

	return TRUE;
}

static void
xmms_cue_destroy (xmms_xform_t *xform)
{
}


