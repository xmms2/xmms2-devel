#include <glib.h>

#include "plugin.h"
#include "transport.h"
#include "decoder.h"
#include "config.h"
#include "config_xmms.h"
#include "playlist.h"
#include "util.h"

#include <stdlib.h>
#include <getopt.h>
#include <string.h>

int
main (int argc, char **argv)
{
	xmms_transport_t *transport;
	xmms_decoder_t *decoder;
	xmms_playlist_t *pl;
	xmms_playlist_entry_t *entry;
	int opt;
	int verbose=0;
	const gchar *mime;
	
	if (argc < 2)
		exit (1);
	
	g_thread_init (NULL);
	if (!xmms_plugin_init ())
		return 1;

	pl = xmms_playlist_init ();

	while (42) {
		opt = getopt (argc, argv, "vV");

		if (opt == -1)
			break;

		switch (opt) {
			case 'v':
				verbose++;
				break;

			case 'V':
				printf ("XMMS version %s\n", VERSION);
				exit (0);
				break;
		}
	}

	if (optind) {
		while (argv[optind]) {
			gchar nuri[XMMS_MAX_URI_LEN];
			if (!strchr (argv[optind], ':')) {
				XMMS_DBG ("No protocol, assuming file");
				g_snprintf (nuri, XMMS_MAX_URI_LEN, "file://%s", argv[optind]);
			} else {
				g_snprintf (nuri, XMMS_MAX_URI_LEN, "%s", argv[optind]);
			}
				
			XMMS_DBG ("Adding uri %s to playlist", nuri);
			xmms_playlist_add (pl, xmms_playlist_entry_new (nuri), XMMS_PL_APPEND);
			optind++;
		}
	}

	XMMS_DBG ("Playlist contains %d entries", xmms_playlist_entries (pl));

	while ((entry = xmms_playlist_pop (pl))) { /* start playback */

		XMMS_DBG ("Playing %s", entry->uri);
		
		transport = xmms_transport_open (entry->uri);
		mime = xmms_transport_mime_type_get (transport);
		if (mime) {
			XMMS_DBG ("mime-type: %s", mime);
			decoder = xmms_decoder_new (mime);
			if (!decoder)
				return 1;
			xmms_decoder_start (decoder, transport);
		}

		xmms_transport_start (transport);

		xmms_transport_wait (transport);
		xmms_playlist_entry_free (entry);
	}

	return 0;
}
