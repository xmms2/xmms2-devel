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

		medialib = xmms_medialib_init (a);

		xmms_medialib_add_dir (medialib, "/ftp/music/Songs");

	}

	return 0;
		
}

