/** @file
 *  This controls the medialib.
 */

#include "util.h"
#include "config_xmms.h"
#include "medialib.h"
#include "transport.h"
#include "decoder.h"

#include <glib.h>

xmms_plugin_t *
xmms_medialib_find_plugin (gchar *name)
{
	GList *list;
	xmms_plugin_t *plugin = NULL;

	g_return_val_if_fail (name, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_MEDIALIB);

	while (list) {
		plugin = (xmms_plugin_t *) list->data;
		if (g_strcasecmp (xmms_plugin_shortname_get (plugin), name) == 0)
			return plugin;

		list = g_list_next (list);
	}

	return NULL;
}

xmms_medialib_t *
xmms_medialib_init (xmms_plugin_t *plugin)
{
	xmms_medialib_new_method_t new_method;
	xmms_medialib_t *medialib;
	
	new_method = xmms_plugin_method_get (plugin, XMMS_METHOD_NEW);

	if (!new_method)
		return NULL;
	
	medialib = g_new0 (xmms_medialib_t, 1);

	medialib->plugin = plugin;
	medialib->mutex = g_mutex_new ();
	
	if (!new_method (medialib)) {
		XMMS_DBG ("new_method failed");
		g_mutex_free (medialib->mutex);
		g_free (medialib);
		return NULL;
	}
	
	return medialib;
}

void
xmms_medialib_set_data (xmms_medialib_t *medialib, gpointer data)
{

	g_return_if_fail (medialib);
	g_return_if_fail (data);

	medialib->data = data;

}

gpointer
xmms_medialib_get_data (xmms_medialib_t *medialib)
{
	g_return_val_if_fail (medialib, NULL);

	return medialib->data;

}

GList *
xmms_medialib_search (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry)
{
	xmms_medialib_search_method_t search;

	g_return_val_if_fail (medialib, NULL);
	g_return_val_if_fail (entry, NULL);

	search = xmms_plugin_method_get (medialib->plugin, XMMS_METHOD_SEARCH);

	g_return_val_if_fail (search, NULL);

	return (search (medialib, entry));
	
}


void
xmms_medialib_add_entry (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry)
{
	xmms_medialib_add_entry_method_t add_entry;

	g_return_if_fail (medialib);
	g_return_if_fail (entry);

	add_entry = xmms_plugin_method_get (medialib->plugin, XMMS_METHOD_ADD_ENTRY);
	g_return_if_fail (add_entry);

	add_entry (medialib, entry);

}


void
xmms_medialib_list_free (GList *list)
{
	g_return_if_fail (list);

	while (list) {
		xmms_playlist_entry_t *e = list->data;
		if (e)
			xmms_playlist_entry_free (e);
		list = g_list_next (list);
	}

	g_list_free (list);

}

gboolean
xmms_medialib_check_if_exists (xmms_medialib_t *medialib, gchar *uri)
{
	xmms_playlist_entry_t *f;
	GList *list;
	
	g_return_val_if_fail (medialib, FALSE);
	g_return_val_if_fail (uri, FALSE);

	f = xmms_playlist_entry_new (uri);

	list = xmms_medialib_search (medialib, f);

	xmms_playlist_entry_free (f);

	if (!list)
		return FALSE;

	if (g_list_length (list) < 1) {
		xmms_medialib_list_free (list);
		return FALSE;
	}

	xmms_medialib_list_free (list);
	
	return TRUE;

}

void
xmms_medialib_add_dir (xmms_medialib_t *medialib, const gchar *dir)
{
	GDir *d;
	const gchar *name;

	g_return_if_fail (medialib);
	g_return_if_fail (dir);

	d = g_dir_open (dir, 0, NULL);

	if (!d) {
		return;
	}

	while ((name = g_dir_read_name (d))) {
		gchar *path = g_build_filename (dir, name, NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			XMMS_DBG ("%s is dir", path);
			xmms_medialib_add_dir (medialib, path);
		} else {
			xmms_transport_t *transport;
			xmms_decoder_t *decoder;
			xmms_playlist_entry_t *entry;
			const gchar *mime;

			if (xmms_medialib_check_if_exists (medialib, path)) {
				XMMS_DBG ("%s in db", name);
				g_free (path);
				continue;
			}

			transport = xmms_transport_open (path);
			if (!transport) {
				g_free (path);
				continue;
			}
			
			mime = xmms_transport_mime_type_get (transport);
			if (mime) {
				decoder = xmms_decoder_new (mime);
				if (!decoder) {
					xmms_transport_close (transport);
					g_free (path);
					continue;
				}
			} else {
				xmms_transport_close (transport);
				g_free (path);
				continue;
			}

			xmms_transport_start (transport);
			entry = xmms_decoder_get_mediainfo_offline (decoder, transport);


			if (entry) {
				XMMS_DBG ("Adding %s", path);
				xmms_playlist_entry_set_uri (entry, path);
				xmms_medialib_add_entry (medialib, entry);
				xmms_playlist_entry_free (entry);
				decoder->mediainfo=NULL;
			} else {
				XMMS_DBG ("Got null from apa");
			}

			xmms_transport_close (transport);
			xmms_decoder_destroy (decoder);

		}

		g_free (path);


	}

	g_dir_close (d);

}

void
xmms_medialib_close (xmms_medialib_t *medialib)
{
	xmms_medialib_close_method_t cm;

	g_return_if_fail (medialib);

	cm = xmms_plugin_method_get (medialib->plugin, XMMS_METHOD_CLOSE);

	if (cm)
		cm (medialib);

	g_mutex_free (medialib->mutex);
	g_free (medialib);

}

