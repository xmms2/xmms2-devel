#include <glib.h>

#include "plugin.h"
#include "transport.h"
#include "decoder.h"
#include "config.h"
#include "playlist.h"
#include "util.h"

#include <stdlib.h>

int
main (int argc, char **argv)
{
	xmms_transport_t *transport;
	xmms_decoder_t *decoder;
	const gchar *mime;
	
	if (argc < 2)
		exit (1);
	
	g_thread_init (NULL);
	if (!xmms_plugin_init ())
		return 1;

	transport = xmms_transport_open (argv[1]);
	mime = xmms_transport_mime_type_get (transport);
	if (mime) {
		XMMS_DBG ("mime-type: %s", mime);
		decoder = xmms_decoder_new (mime);
		if (!decoder)
			return 1;
		xmms_decoder_start (decoder, transport);
	}

	xmms_transport_start (transport);


	for (;;)
		sleep (1);
	
#if 0

	xmms_playlist_t *pl;
	xmms_playlist_entry_t *entry;

	pl = xmms_playlist_open_create ("playlist.db");

	entry = g_new0 (xmms_playlist_entry_t, 1);

	g_snprintf (entry->uri, 1024, "file://tmp/foo.mp3");
	entry->id = 10010101;

	xmms_playlist_add (pl, entry, XMMS_PL_APPEND);
	xmms_playlist_add (pl, entry, XMMS_PL_APPEND);

	xmms_playlist_close ();
#endif
	return 0;
}
