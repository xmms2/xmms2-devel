#include <glib.h>

#include "plugin.h"
#include "transport.h"
#include "config.h"
#include "playlist.h"


int
main (int argc, char **argv)
{
#if 0
	xmms_transport_t *transport;
	xmms_config_data_t *cdata;
	xmms_config_value_t *val;
	xmms_config_value_t *n;

	
	if (argc < 2)
		exit (1);
	
	g_thread_init (NULL);
	if (!xmms_plugin_init ())
		return 1;

	transport = xmms_transport_open (argv[1]);
	
	cdata = xmms_config_init ("xmms2.conf");

	if (!cdata)
		return 1;

	val = g_hash_table_lookup (cdata->decoder, "maddecoder");

	printf ("%s\n", xmms_config_value_as_string (xmms_config_value_property_lookup (val, "rate")));
	
	xmms_config_save_to_file (cdata, "xmms2.conf");
#endif

	xmms_playlist_t *pl;
	xmms_playlist_entry_t *entry;

	pl = xmms_playlist_open_create ("playlist.db");

	entry = g_new0 (xmms_playlist_entry_t, 1);

	g_snprintf (entry->uri, 1024, "file://tmp/foo.mp3");
	entry->id = 10010101;

	xmms_playlist_add (pl, entry, XMMS_PL_APPEND);
	xmms_playlist_add (pl, entry, XMMS_PL_APPEND);

	xmms_playlist_close ();

	return 0;
}
