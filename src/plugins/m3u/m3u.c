#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/playlist.h"
#include "xmms/playlist_entry.h"
#include "xmms/util.h"
#include "xmms/xmms.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Type definitions
 */

/*
 * Function prototypes
 */

static gboolean xmms_m3u_can_handle (const gchar *mimetype);
static gboolean xmms_m3u_read_playlist (xmms_transport_t *transport, 
		xmms_playlist_t *playlist);
static gboolean xmms_m3u_write_playlist (xmms_playlist_t *playlist, 
		gchar *filename);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_PLAYLIST, "m3u",
			"M3U Playlist " XMMS_VERSION,
			"M3U Playlist reader / writer");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE,
				xmms_m3u_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ_PLAYLIST, 
				xmms_m3u_read_playlist);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE_PLAYLIST, 
				xmms_m3u_write_playlist);

	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_m3u_can_handle (const gchar *mime)
{
	g_return_val_if_fail (mime, FALSE);

	XMMS_DBG ("xmms_m3u_can_handle (%s)", mime);
	
	if ((g_strncasecmp (mime, "audio/mpegurl", 13) == 0))
		return TRUE;

	if ((g_strncasecmp (mime, "audio/x-mpegurl", 15) == 0))
		return TRUE;

	return FALSE;
}

static gboolean
xmms_m3u_read_playlist (xmms_transport_t *transport, 
			xmms_playlist_t *playlist)
{
	gint len = 0;
	gint buffer_len = 0;
	gint i;
	gchar *buffer;
	gchar **lines;
	gboolean extm3u = FALSE;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (playlist, FALSE);
	
	buffer_len = xmms_transport_size (transport);
	buffer = g_malloc (buffer_len);

	while (len < buffer_len) {
		gint ret;

		ret = xmms_transport_read (transport, buffer+len, buffer_len);
		XMMS_DBG ("Got %d bytes.", ret);

		if ( ret < 0 ) {
			g_free (buffer);
			buffer=NULL;
			return FALSE;
		}
		len += ret;
		g_assert (len >= 0);
	}

	lines = g_strsplit (buffer, "\n", 0);

	if (!lines && !lines[0])
		return FALSE;

	i = 0;

	if (strcmp (lines[0], "#EXTM3U") == 0) {
		extm3u = TRUE;
		i = 1;
	}

	while (lines[i]) {
		xmms_playlist_entry_t *entry = NULL;

		if (extm3u && lines[i][0] == '#') {
			gchar *len;
			gchar *title;
			gchar *p;

			p = strchr (lines[i], ',');
			if (p) {
				*p = '\0';
				*p++;
			} else {
				XMMS_DBG ("Malformated m3u");
				return FALSE;
			}

			len = lines[i]+8;
			title = p;

			i++; /* skip to track */

			if (lines[i] && lines[i][0]) {
				if (lines[i][0] != '/') {
					gchar *new, *path, *p;
					path = g_strdup (xmms_transport_url_get (transport));
					
					p = strrchr (path, '/');
					if (p) {
						*p = '\0';
						new = g_strdup_printf ("%s/%s", path, lines[i]);
						entry = xmms_playlist_entry_new (new);
						g_free (path);
					}
				} else {
					entry = xmms_playlist_entry_new (lines[i]);
				}
			}

			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION, len);
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE, title);
		} else {
			if (lines[i] && lines[i][0]) {
				if (lines[i][0] != '/') {
					gchar *new, *path, *p;
					path = g_strdup (xmms_transport_url_get (transport));
					
					p = strrchr (path, '/');
					if (p) {
						*p = '\0';
						new = g_strdup_printf ("%s/%s", path, lines[i]);
						entry = xmms_playlist_entry_new (new);
						g_free (path);
					}
				} else {
					entry = xmms_playlist_entry_new (lines[i]);
				}
			}
		}

		if (entry) {
			XMMS_DBG ("Adding %s", xmms_playlist_entry_url_get (entry));
			xmms_playlist_add (playlist, entry, XMMS_PLAYLIST_APPEND);
		}
			
		i++;
	}

	return TRUE;

}

static gboolean
xmms_m3u_write_playlist (xmms_playlist_t *playlist, gchar *filename)
{
	FILE *fp;
	GList *lista, *tmp;

	fp = fopen (filename, "w+");
	if (!fp)
		return FALSE;

	lista = xmms_playlist_list (playlist);

	fwrite ("#EXTM3U\n", 8, 1, fp);

	for (tmp = lista; tmp; tmp = g_list_next (tmp)) {
		xmms_playlist_entry_t *entry;
		gchar *d, *artist, *title;
		gint duration;
		gchar *uri;
		gchar *uri2;

		entry = (xmms_playlist_entry_t *)tmp->data;

		d = xmms_playlist_entry_property_get (entry, 
				XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION);

		if (d)
			duration = atoi (d);
		else
			d = 0;

		artist = xmms_playlist_entry_property_get (entry,
				XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST);
		
		title = xmms_playlist_entry_property_get (entry,
				XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE);

		xmms_playlist_entry_unref (entry);

		if (title && artist && duration) {
			gchar *t;

			t = g_strdup_printf ("#EXTINF:%d,%s - %s\n", duration / 1000,
					artist, title);

			fwrite (t, strlen (t), 1, fp);
			g_free (t);
		}

		uri2 = xmms_util_decode_path (xmms_playlist_entry_url_get (entry));
		uri = strchr (uri2, ':');
		if (!uri)
			continue;

		uri = uri + 2;

		uri = g_strdup_printf ("%s\n", uri);

		fwrite (uri, strlen (uri), 1, fp);

		g_free (uri);
		g_free (uri2);
	}
	
	fclose (fp);
	
	return TRUE;
}
