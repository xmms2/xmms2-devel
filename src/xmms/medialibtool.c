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
	if (!a) 
		return 1;

	medialib = xmms_medialib_init (a);
	g_return_val_if_fail (medialib, -1);

	if (argc < 1) return 1;

	xmms_medialib_add_dir (medialib, argv[1]);


	xmms_medialib_close (medialib);

	return 0;
		
}

