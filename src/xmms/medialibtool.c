#include "util.h"
#include "transport.h"
#include "playlist.h"
#include "medialib.h"
#include "plugin.h"

#include <glib.h>

int
main (int argc, char **argv)
{
	xmms_plugin_t *a;
	xmms_medialib_t *medialib;


	g_thread_init (NULL);

	if (!xmms_plugin_init ())
		return 1;

	a = xmms_medialib_find_plugin ("sqlite");
	if (a) {
		xmms_playlist_entry_t *find;

		find = xmms_playlist_entry_new (NULL);

		medialib = xmms_medialib_init (a);

		xmms_playlist_entry_set_prop (find, XMMS_ENTRY_PROPERTY_ARTIST, "*AP*");
		xmms_playlist_entry_set_prop (find, XMMS_ENTRY_PROPERTY_BITRATE, "128");
		xmms_medialib_search (medialib, find);

		xmms_playlist_entry_free (find);
	}
		
}

