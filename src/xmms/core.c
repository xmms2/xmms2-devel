/** @file
 *
 */

#include "plugin.h"
#include "transport.h"
#include "decoder.h"
#include "config.h"
#include "config_xmms.h"
#include "playlist.h"
#include "unixsignal.h"
#include "util.h"
#include "core.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

/** @todo detta är lite buskis */
static xmms_core_t core_object;
xmms_core_t *core = &core_object;

/**
 *
 */


static void
handle_mediainfo_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_playlist_entry_t *entry;

	entry = xmms_playlist_entry_new (NULL);
	
	xmms_core_get_mediainfo (entry);

	xmms_playlist_entry_print (entry);

	xmms_playlist_entry_free (entry);

	xmms_object_emit (XMMS_OBJECT (core), "mediainfo-changed", NULL);

}


void
play_next (void)
{
	xmms_transport_t *transport;
	xmms_decoder_t *decoder;
	const gchar *mime;

	g_return_if_fail (core->decoder == NULL);

	do {
		if (core->curr_song) {
			xmms_playlist_entry_free (core->curr_song);
			core->curr_song = NULL;
		}
		
		core->curr_song = xmms_playlist_pop (core->playlist);
		if (!core->curr_song)
			exit (1); /** @todo use glib tjoff */
		
		XMMS_DBG ("Playing %s", core->curr_song->uri);
		
		transport = xmms_transport_open (core->curr_song->uri);
		
	} while (!transport);
	
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
		play_next (); /** @todo FIXME */
		return;
	}

	xmms_object_connect (XMMS_OBJECT (decoder), "mediainfo-changed", 
			     handle_mediainfo_changed, NULL);

	XMMS_DBG ("starting threads..");
	xmms_transport_start (transport);
	XMMS_DBG ("transport started");
	xmms_decoder_start (decoder, transport, core->output);
	XMMS_DBG ("output started");

	core->decoder = decoder;

}

static void
eos_reached (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	XMMS_DBG ("eos_reached");

	xmms_core_play_next ();

}

/**
 * Let core know about which output-plugin we use.
 *
 * @internal
 */
void xmms_core_output_set (xmms_output_t *output)
{
	g_return_if_fail (core->output==NULL);
	core->output = output;
	xmms_object_connect (XMMS_OBJECT (output), "eos-reached", eos_reached, NULL);
}

/**
 * Play next song in playlist.
 *
 * Stops the playing of the current song, and frees any resources used
 * by the decoder and its transport. Then starts playing the next
 * song in playlist.
 *
 */
void xmms_core_play_next ()
{
	XMMS_DBG ("closing transport");
	xmms_transport_close (xmms_decoder_transport_get (core->decoder));
	XMMS_DBG ("destroying decoder");
	xmms_decoder_destroy (core->decoder);
	core->decoder = NULL;
	XMMS_DBG ("playing next");
	play_next ();
	XMMS_DBG ("done");
}

/**
 * Initializes the coreobject.
 *
 * @internal
 */
void
xmms_core_init ()
{
	memset (core, 0, sizeof(xmms_core_t));
	xmms_object_init (XMMS_OBJECT (core));
}

/**
 * Starts playing of the first song in playlist.
 */
void
xmms_core_start ()
{
	play_next ();
}

/**
 * Sets information about what is beeing decoded.
 *
 * All information is copied into the core.
 *
 * @param entry Entry containing the information to set.
 */
void
xmms_core_set_mediainfo (xmms_playlist_entry_t *entry)
{
	xmms_playlist_entry_copy_property (entry, core->curr_song);
	xmms_object_emit (XMMS_OBJECT (core), "mediainfo-changed", core);
}

/**
 *
 * @internal
 */
void
xmms_core_set_playlist (xmms_playlist_t *playlist)
{
	core->playlist = playlist;
}

/**
 * Get a copy of structure describing what is beeing decoded.
 *
 * @param entry 
 *
 * @return TRUE if entry was successfully filled in with information, 
 * FALSE otherwise.
 */
gboolean
xmms_core_get_mediainfo (xmms_playlist_entry_t *entry)
{
	g_return_val_if_fail (core, FALSE);
	g_return_val_if_fail (entry, FALSE);

	g_return_val_if_fail (core->curr_song, FALSE);

	xmms_playlist_entry_copy_property (core->curr_song, entry);
	
	return TRUE;
}

/**
 * Get uri of current playing song.
 *
 * Returns a copy of the uri to the song currently playing. I.e it
 * should be freed by the caller.
 *
 * @returns uri or NULL if no song beeing played.
 */
gchar *
xmms_core_get_uri ()
{
	return core->curr_song->uri ? g_strdup(core->curr_song->uri) : NULL;
}


/**
 * Set time current tune have been played.
 *
 * Lets the core know about the progress of the current song.  This
 * call causes the "playtime-changed" signal to be emitted on the core
 * object. @warning The time is measured in milliseconds, which will
 * make it wrap after 49 days on systems with 32bit guint.
 *
 * @param time number of milliseconds played.
 */
void
xmms_core_playtime_set (guint time)
{
	xmms_object_emit (XMMS_OBJECT (core), "playtime-changed", GUINT_TO_POINTER (time) );
}
