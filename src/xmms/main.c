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
#include <unistd.h>

static xmms_playlist_t *playlist;
static xmms_output_t *output;

void play_next (void);

void
eos_reached (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *)object;
	XMMS_DBG ("eos_reached");

	XMMS_DBG ("closing transport");
	xmms_transport_close (xmms_decoder_transport_get (decoder));
	XMMS_DBG ("destroying decoder");
	xmms_decoder_destroy (decoder);
	XMMS_DBG ("playing next");
	play_next ();
	XMMS_DBG ("done");
}

void
play_next (void)
{
	xmms_transport_t *transport;
	xmms_decoder_t *decoder;
	xmms_playlist_entry_t *entry;
	const gchar *mime;
	
	entry = xmms_playlist_pop (playlist);
	if (!entry)
		exit (1);

	XMMS_DBG ("Playing %s", entry->uri);
	
	transport = xmms_transport_open (entry->uri);
	xmms_playlist_entry_free (entry);

	if (!transport)
		play_next ();
	
	mime = xmms_transport_mime_type_get (transport);
	if (mime) {
		XMMS_DBG ("mime-type: %s", mime);
		decoder = xmms_decoder_new (mime);
		if (!decoder) {
			xmms_transport_close (transport);
			play_next ();
			return;
		}
	} else {
		xmms_transport_close (transport);
		play_next (); /* FIXME */
		return;
	}

	xmms_object_connect (XMMS_OBJECT (decoder), "eos-reached", eos_reached, NULL);

	XMMS_DBG ("starting threads..");
	xmms_transport_start (transport);
	XMMS_DBG ("transport started");
	xmms_decoder_start (decoder, transport, output);
	XMMS_DBG ("output started");
	
}


int
main (int argc, char **argv)
{

	xmms_plugin_t *o_plugin;
	int opt;
	int verbose=0;
	gchar *outname = NULL;
	
	if (argc < 2)
		exit (1);
	
	g_thread_init (NULL);
	if (!xmms_plugin_init ())
		return 1;

	playlist = xmms_playlist_init ();

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
			xmms_playlist_add (playlist, xmms_playlist_entry_new (nuri), XMMS_PL_APPEND);
			optind++;
		}
	}

	XMMS_DBG ("Playlist contains %d entries", xmms_playlist_entries (playlist));

	if (!outname)
		outname = "oss";

	o_plugin = xmms_output_find_plugin (outname);
	g_return_val_if_fail (o_plugin, -1);
	output = xmms_output_open (o_plugin);
	g_return_val_if_fail (output, -1);
	xmms_output_start (output);
	play_next ();

	sleep (3600);

	return 0;
}
