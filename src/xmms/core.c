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

xmms_playlist_t *playlist;
static xmms_object_t core_object;
static xmms_output_t *m_output=NULL;
static xmms_decoder_t *m_decoder;

/**
 *
 */
xmms_object_t *core = &core_object;

void
play_next (void)
{
	xmms_transport_t *transport;
	xmms_decoder_t *decoder;
	xmms_playlist_entry_t *entry;
	const gchar *mime;

	g_return_if_fail (m_decoder == NULL);
	
	entry = xmms_playlist_pop (playlist);
	if (!entry)
		exit (1);

	XMMS_DBG ("Playing %s", entry->uri);
	
	transport = xmms_transport_open (entry->uri);

	if (!transport) {
		play_next ();
		return;
	}
	
	mime = xmms_transport_mime_type_get (transport);
	if (mime) {
		XMMS_DBG ("mime-type: %s", mime);
		decoder = xmms_decoder_new (mime);
		if (!decoder) {
			xmms_transport_close (transport);
			xmms_playlist_entry_free (entry);
			play_next ();
			return;
		}
	} else {
		xmms_transport_close (transport);
		xmms_playlist_entry_free (entry);
		play_next (); /* FIXME */
		return;
	}

//	xmms_object_connect (XMMS_OBJECT (decoder), "mediainfo-changed", mediainfo_changed, NULL);

	XMMS_DBG ("starting threads..");
	xmms_transport_start (transport);
	XMMS_DBG ("transport started");
	xmms_decoder_start (decoder, transport, m_output);
	XMMS_DBG ("output started");

	m_decoder = decoder;

	xmms_playlist_entry_free (entry);
	
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
void xmms_core_output_set (xmms_output_t *output){
	g_return_if_fail (m_output==NULL);
	m_output = output;
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
void xmms_core_play_next (){
	XMMS_DBG ("closing transport");
	xmms_transport_close (xmms_decoder_transport_get (m_decoder));
	XMMS_DBG ("destroying decoder");
	xmms_decoder_destroy (m_decoder);
	m_decoder = NULL;
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
xmms_core_init () {
	xmms_object_init (&core_object);
}

/**
 * Starts playing of the first song in playlist.
 */
void
xmms_core_start () {
	play_next ();
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
xmms_core_playtime_set (guint time) {
	xmms_object_emit (core, "playtime-changed", GUINT_TO_POINTER (time) );
}
