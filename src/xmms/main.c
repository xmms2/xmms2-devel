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
	xmms_output_t *output;
	xmms_decoder_t *decoder;
	xmms_playlist_t *pl;
	xmms_playlist_entry_t *entry;
	xmms_plugin_t *o_plugin;
	int opt;
	int verbose=0;
	const gchar *mime;
	gchar *outname = NULL;
	
	if (argc < 2)
		exit (1);
	
	g_thread_init (NULL);
	if (!xmms_plugin_init ())
		return 1;

	pl = xmms_playlist_init ();

	while (42) {
		opt = getopt (argc, argv, "vVo:");

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

			case 'o':
				outname = g_strdup (optarg);
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

	if (!outname)
		outname = "oss";

	o_plugin = xmms_output_find_plugin (outname);
	g_return_val_if_fail (o_plugin, -1);
	output = xmms_output_open (o_plugin);
	g_return_val_if_fail (output, -1);
	xmms_output_start (output);

	while ((entry = xmms_playlist_pop (pl))) { /* start playback */

		XMMS_DBG ("Playing %s", entry->uri);
		
		transport = xmms_transport_open (entry->uri);
		if (!transport)
			continue;

		mime = xmms_transport_mime_type_get (transport);
		if (mime) {
			XMMS_DBG ("mime-type: %s", mime);
			decoder = xmms_decoder_new (mime);
			if (!decoder)
				return 1;
			xmms_decoder_start (decoder, transport, output);
		} else {
			return 1;
		}

		
		xmms_transport_start (transport);

		
		if (xmms_decoder_get_mediainfo (decoder, entry)) {
			XMMS_DBG ("id3tag: Artist=%s, album=%s, title=%s, year=%d, comment=%s, tracknr=%d", entry->artist, entry->album, entry->title, entry->year, entry->comment, entry->tracknr);
		}

		xmms_transport_wait (transport);
		XMMS_DBG ("EOS");

		xmms_transport_free (transport);

		xmms_playlist_entry_free (entry);

	}

	return 0;
}
