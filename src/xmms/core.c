/** @file
 *
 */

#include "plugin.h"
#include "transport.h"
#include "decoder.h"
#include "config.h"
#include "config_xmms.h"
#include "playlist.h"
#include "plsplugins.h"
#include "unixsignal.h"
#include "util.h"
#include "core.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

/** @todo detta är lite buskis */
static xmms_core_t core_object;
xmms_core_t *core = &core_object;

static gboolean running = TRUE;
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
void
xmms_core_output_set (xmms_output_t *output)
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
void
xmms_core_play_next ()
{
	if (core->decoder) {
		XMMS_DBG ("playing next");
		xmms_transport_close (xmms_decoder_transport_get (core->decoder));
		xmms_decoder_destroy (core->decoder);
		core->decoder = NULL;
	}
}

void
xmms_core_playback_stop ()
{
	if (core->decoder) {
		XMMS_DBG ("Stopping playback");
		xmms_transport_close (xmms_decoder_transport_get (core->decoder));
		xmms_decoder_destroy (core->decoder);
		core->decoder = NULL;
		core->status = XMMS_CORE_PLAYBACK_STOPPED;
		xmms_object_emit (XMMS_OBJECT (core), "playback-stopped", NULL);
	}
}


void
xmms_core_playback_start ()
{
	if (core->status == XMMS_CORE_PLAYBACK_STOPPED) {
		/* @todo race condition? */
		core->status = XMMS_CORE_PLAYBACK_RUNNING;
		g_cond_signal (core->cond);
	}
}

void
xmms_core_playlist_adduri (gchar *nuri)
{
	xmms_playlist_add (core->playlist, xmms_playlist_entry_new (nuri), XMMS_PLAYLIST_APPEND);

}

void
xmms_core_playlist_jump (guint id)
{
	xmms_playlist_set_current_position (core->playlist, id);
}

void
xmms_core_playlist_shuffle ()
{
	xmms_playlist_shuffle (core->playlist);
}

void
xmms_core_playlist_clear ()
{
	/* @todo Kanske inte skitsnygt. */
	xmms_core_playback_stop ();
	core->curr_song = NULL;
	xmms_playlist_clear (core->playlist);
}

void
xmms_core_playlist_remove (guint id)
{
	xmms_playlist_id_remove (core->playlist, id);
}

void
xmms_core_quit ()
{
	exit (0); /** @todo BUSKIS! */
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
	core->cond = g_cond_new ();
	core->mutex = g_mutex_new ();
}

static gpointer 
core_thread(gpointer data){
	xmms_transport_t *transport;
	xmms_decoder_t *decoder;
	xmms_playlist_plugin_t *plsplugin;
	const gchar *mime;

	while (running) {
		
		if (core->status != XMMS_CORE_PLAYBACK_STOPPED)
			core->curr_song = xmms_playlist_get_next (core->playlist);

		while (core->status == XMMS_CORE_PLAYBACK_STOPPED) {
			XMMS_DBG ("Waiting until playback starts...");
			g_mutex_lock (core->mutex);
			g_cond_wait (core->cond, core->mutex);
			g_mutex_unlock (core->mutex);
		}
		
		XMMS_DBG ("Playing %s", core->curr_song->uri);
		
		transport = xmms_transport_open (core->curr_song->uri);

		if (!transport)
			continue;

		mime = xmms_transport_mime_type_get (transport);
		if (!mime) {
			xmms_transport_close (transport);
			continue;
		}

		XMMS_DBG ("mime-type: %s", mime);

		plsplugin = xmms_playlist_plugin_new (mime);
		if (plsplugin) {

			/* This is a playlist file... */
			XMMS_DBG ("Playlist!!");
			xmms_transport_start (transport);
			XMMS_DBG ("transport started");
			xmms_playlist_plugin_read (plsplugin, core->playlist, transport);

			/* we don't want it in the playlist. */
			xmms_playlist_id_remove (core->playlist, xmms_playlist_entry_id_get (core->curr_song));

			/* cleanup */
			xmms_playlist_plugin_free (plsplugin);
			xmms_transport_close (transport);
			continue;
		}

		decoder = xmms_decoder_new (mime);
		if (!decoder) {
			xmms_transport_close (transport);
			continue;
		}

		xmms_object_connect (XMMS_OBJECT (decoder), "mediainfo-changed", 
				     handle_mediainfo_changed, NULL);

		XMMS_DBG ("starting threads..");
		xmms_transport_start (transport);
		XMMS_DBG ("transport started");
		xmms_decoder_start (decoder, transport, core->output);
		XMMS_DBG ("output started");
		
		core->decoder = decoder;

		xmms_decoder_wait (core->decoder);

		if (core->decoder) {
			XMMS_DBG ("closing transport");
			xmms_transport_close (xmms_decoder_transport_get (core->decoder));
			XMMS_DBG ("destroying decoder");
			xmms_decoder_destroy (core->decoder);
			core->decoder = NULL;
		}
		
	}

	return NULL;
}

/**
 * Starts playing of the first song in playlist.
 */
void
xmms_core_start ()
{
	g_thread_create (core_thread, NULL, FALSE, NULL);
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
 *
 * @internal
 */
xmms_playlist_t *
xmms_core_get_playlist ()
{
	return core->playlist;
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
	return core->curr_song ? core->curr_song->uri ? g_strdup(core->curr_song->uri) : NULL : NULL;
}

gint
xmms_core_get_id ()
{
	if (core->curr_song && core->status != XMMS_CORE_PLAYBACK_STOPPED) 
		return xmms_playlist_entry_id_get (core->curr_song);
	return 0;
}

xmms_playlist_entry_t *
xmms_core_playlist_entry_mediainfo (guint id)
{
	return xmms_playlist_get_byid (core->playlist, id);
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
