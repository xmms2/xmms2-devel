/** @file
 *
 */

#include "plugin.h"
#include "transport.h"
#include "transport_int.h"
#include "decoder.h"
#include "decoder_int.h"
#include "config_xmms.h"
#include "playlist.h"
#include "plsplugins.h"
#include "unixsignal.h"
#include "util.h"
#include "core.h"
#include "signal_xmms.h"
#include "magic.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#define XMMS_CORE_NEXT_SONG 1
#define XMMS_CORE_PREV_SONG 2
#define XMMS_CORE_HOLD_SONG 3

/** @todo detta är lite buskis */
static xmms_core_t core_object;
xmms_core_t *core = &core_object;

struct xmms_core_St {
	xmms_object_t object;

	xmms_output_t *output;
	xmms_decoder_t *decoder;
	xmms_transport_t *transport;

	xmms_playlist_t *playlist;
	xmms_playlist_entry_t *curr_song;
	xmms_mediainfo_thread_t *mediainfothread;

	xmms_effect_t *effects;
	
	xmms_config_data_t *config;

	GCond *cond;
	GMutex *mutex;
	
	gint status;
	gint playlist_op;

	gboolean flush;
};

typedef enum {
	XMMS_CORE_PLAYBACK_RUNNING,
	XMMS_CORE_PLAYBACK_STOPPED
} xmms_core_playback_status_t;

static gboolean running = TRUE;
/**
 *
 */


static void
handle_mediainfo_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYBACK_CURRENTID, data);
}

static gboolean
next_song ()
{
	if (core->curr_song)
		xmms_playlist_entry_unref (core->curr_song);

	core->curr_song = xmms_playlist_get_next_entry (core->playlist);

	if (!core->curr_song) {
		xmms_core_playback_stop ();
		return FALSE;
	}
	return TRUE;
}

static gboolean
prev_song ()
{
	if (core->curr_song)
		xmms_playlist_entry_unref (core->curr_song);

	core->curr_song = xmms_playlist_get_prev_entry (core->playlist);

	if (!core->curr_song) {
		xmms_core_playback_stop ();
		return FALSE;
	}
	return TRUE;
}

static void
eos_reached (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	XMMS_DBG ("eos_reached");

	if (core->playlist_op == XMMS_CORE_PREV_SONG)
		core->playlist_op = XMMS_CORE_NEXT_SONG;
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
	xmms_object_connect (XMMS_OBJECT (output), XMMS_SIGNAL_OUTPUT_EOS_REACHED, eos_reached, NULL);
}


static void
wake_core ()
{
	if (core->decoder) {
		xmms_decoder_destroy (core->decoder);
		core->decoder = NULL;
	}
}


xmms_config_data_t *
xmms_core_config_get (xmms_core_t *core)
{
	g_return_val_if_fail (core, NULL);

	return core->config;
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
	if (core->status == XMMS_CORE_PLAYBACK_RUNNING) {
		core->playlist_op = XMMS_CORE_NEXT_SONG;
		core->flush = TRUE;
		wake_core ();
	} else {
		next_song ();
	}
}

void
xmms_core_play_prev ()
{
	if (core->status == XMMS_CORE_PLAYBACK_RUNNING) {
		core->playlist_op = XMMS_CORE_PREV_SONG;
		core->flush = TRUE;
		wake_core ();
	} else {
		prev_song ();
	}
}

void
xmms_core_playback_stop ()
{
	if (core->status == XMMS_CORE_PLAYBACK_RUNNING) {
		core->status = XMMS_CORE_PLAYBACK_STOPPED;
		xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYBACK_STOP, NULL);
		core->flush = TRUE;
		wake_core ();
	} else {
		XMMS_DBG ("xmms_core_playback_stop with status != XMMS_CORE_PLAYBACK_RUNNING");
	}
}

void
xmms_core_playlist_save (gchar *filename)
{
	const gchar *mime;
	xmms_playlist_plugin_t *plugin;

	mime = xmms_magic_mime_from_file (filename);
	if (!mime)
		return;

	plugin = xmms_playlist_plugin_new (mime);

	g_return_if_fail (plugin);

	xmms_playlist_plugin_save (plugin, core->playlist, filename);

	xmms_playlist_plugin_free (plugin);
}

void
xmms_core_playback_start ()
{
	if (core->status == XMMS_CORE_PLAYBACK_STOPPED) {
		/* @todo race condition? */
		core->status = XMMS_CORE_PLAYBACK_RUNNING;
		g_cond_signal (core->cond);
	} else {
		XMMS_DBG ("xmms_core_playback_start with status != XMMS_CORE_PLAYBACK_STOPPED");
	}
}

void
xmms_core_mediainfo_add_entry (guint id)
{
	xmms_mediainfo_thread_add (core->mediainfothread, id);
}

void
xmms_core_playlist_addurl (gchar *nurl)
{
	xmms_playlist_entry_t *entry = xmms_playlist_entry_new (nurl);
	xmms_playlist_add (core->playlist, entry, XMMS_PLAYLIST_APPEND);
}

void
xmms_core_playlist_jump (guint id)
{
	xmms_playlist_set_current_position (core->playlist, id);

	core->playlist_op = XMMS_CORE_HOLD_SONG;

	if (core->status == XMMS_CORE_PLAYBACK_RUNNING) {
		core->flush = TRUE;
		wake_core ();
	} else {
		XMMS_DBG ("xmms_core_playback_next with status != XMMS_CORE_PLAYBACK_RUNNING");
	}

}

void
xmms_core_playlist_shuffle ()
{
	xmms_playlist_shuffle (core->playlist);
}

void
xmms_core_playlist_clear ()
{
	/** @todo Kanske inte skitsnygt. */
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
xmms_core_playback_seek_ms (guint milliseconds)
{
	xmms_decoder_seek_ms (core->decoder, milliseconds);
}

void
xmms_core_playback_seek_samples (guint samples)
{
	xmms_decoder_seek_samples (core->decoder, samples);
}

void
xmms_core_quit ()
{
	gchar *filename;
	filename = g_strdup_printf ("%s/.xmms2/xmms2.conf", g_get_home_dir ());
	xmms_config_save_to_file (core->config, filename);
	exit (0); /** @todo BUSKIS! */
}


/**
 *  This will send a information message to all clients.
 *
 *  Used by log code to send failures to connected clients.
 */
void 
xmms_core_information (gint loglevel, gchar *information)
{
	if (loglevel > XMMS_LOG_DEBUG)
		xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_CORE_INFORMATION, information);
}

/**
 * 
 */
void
xmms_core_vis_spectrum (gfloat *spec)
{
	xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_VISUALISATION_SPECTRUM, spec);
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
	core->flush = FALSE;

}

static gpointer 
core_thread (gpointer data)
{
	xmms_playlist_plugin_t *plsplugin;
	const gchar *mime;

	core->playlist_op = XMMS_CORE_HOLD_SONG;

	while (running) {

		core->transport = NULL;
		core->decoder = NULL;
		
		while (core->status == XMMS_CORE_PLAYBACK_STOPPED) {
			XMMS_DBG ("Waiting until playback starts...");
			g_mutex_lock (core->mutex);
			g_cond_wait (core->cond, core->mutex);
			g_mutex_unlock (core->mutex);
		}

		if (core->playlist_op == XMMS_CORE_NEXT_SONG) {
			if (!next_song ()) {
				continue;
			}
		} else if (core->playlist_op == XMMS_CORE_PREV_SONG) {
			if (!prev_song ()) {
				continue;
			}
		}

		core->playlist_op = XMMS_CORE_NEXT_SONG;
		
		core->curr_song = xmms_playlist_get_current_entry (core->playlist);

		if (!core->curr_song) {
			core->status = XMMS_CORE_PLAYBACK_STOPPED;
			continue;
		}
		
		XMMS_DBG ("Playing %s", xmms_playlist_entry_url_get (core->curr_song));
		
		core->transport = xmms_transport_new ();
		xmms_transport_open (core->transport, core->curr_song);

		if (!core->transport) {
			continue;
		}
		
/*		xmms_object_connect (XMMS_OBJECT (core->transport), 
				XMMS_SIGNAL_TRANSPORT_MIMETYPE,
				xmms_core_mimetype_callback, NULL);*/

		XMMS_DBG ("Starting transport");
		xmms_transport_start (core->transport);
		
		XMMS_DBG ("Waiting for mimetype");
		mime = xmms_transport_mimetype_get_wait (core->transport);
		if (!mime) {
			xmms_transport_close (core->transport);
			xmms_core_play_next ();
		}

		XMMS_DBG ("mime-type: %s", mime);
	
		plsplugin = xmms_playlist_plugin_new (mime);
		if (plsplugin) {

			/* This is a playlist file... */
			XMMS_DBG ("Playlist!!");
			xmms_playlist_plugin_read (plsplugin, core->playlist, core->transport);

			/* we don't want it in the playlist. */
			xmms_playlist_id_remove (core->playlist, xmms_playlist_entry_id_get (core->curr_song));

			/* cleanup */
			xmms_playlist_plugin_free (plsplugin);
			xmms_core_play_next ();
		}

		xmms_playlist_entry_mimetype_set (core->curr_song, mime);

		core->decoder = xmms_decoder_new ();
		if (!xmms_decoder_open (core->decoder, core->curr_song)) {
			xmms_decoder_destroy (core->decoder);
			xmms_core_play_next ();
		}

		xmms_object_connect (XMMS_OBJECT (core->decoder), XMMS_SIGNAL_PLAYBACK_CURRENTID,
			    	handle_mediainfo_changed, NULL);

		XMMS_DBG ("starting threads..");
		XMMS_DBG ("transport started");
		xmms_decoder_start (core->decoder, core->transport, core->effects, core->output);
		XMMS_DBG ("decoder started");

		xmms_decoder_wait (core->decoder);

		XMMS_DBG ("destroying decoder");
		if (core->decoder)
			xmms_decoder_destroy (core->decoder);
		XMMS_DBG ("closing transport");
		if (core->transport)
			xmms_transport_close (core->transport);
		if (core->flush) {
			XMMS_DBG ("Flushing buffers");
			if (core->output)
				xmms_output_flush (core->output);
			core->flush = FALSE;
		}
		
	}

	return NULL;
}

/**
 * Starts playing of the first song in playlist.
 */
void
xmms_core_start (xmms_config_data_t *config)
{
	core->config = config;
	core->mediainfothread = xmms_mediainfo_thread_start (core->playlist);
	core->effects = xmms_effect_prepend (NULL, "equalizer", config->effect);
	g_thread_create (core_thread, NULL, FALSE, NULL);
}

/**
 * Sets information about what is beeing decoded.
 *
 * All information is copied into the core.
 *
 * @param entry Entry containing the information to set.
 */
/*void
xmms_core_set_mediainfo (xmms_playlist_entry_t *entry)
{
	xmms_playlist_entry_copy_property (entry, core->curr_song);
	xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYBACK_CURRENTID, core);
}*/

void
xmms_core_playlist_mediainfo_changed (guint id)
{
	xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYLIST_MEDIAINFO, 
			GUINT_TO_POINTER (id));
}

/**
 *
 * @internal
 */

static void
handle_playlist_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	const xmms_playlist_changed_msg_t *chmsg = data;
	switch (chmsg->type) {
		case XMMS_PLAYLIST_CHANGED_ADD:
			XMMS_DBG ("Something was added to playlist");
			break;
		case XMMS_PLAYLIST_CHANGED_SHUFFLE:
			XMMS_DBG ("Playlist was shuffled");
			break;
		case XMMS_PLAYLIST_CHANGED_CLEAR:
			XMMS_DBG ("Playlist was cleared");
			break;
		case XMMS_PLAYLIST_CHANGED_REMOVE:
			XMMS_DBG ("Something was removed from playlist");
			break;
		case XMMS_PLAYLIST_CHANGED_SET_POS:
			XMMS_DBG ("We jumped in playlist");
			break;
	}

	xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYLIST_CHANGED, data);
}


void
xmms_core_set_playlist (xmms_playlist_t *playlist)
{
	core->playlist = playlist;
	
	xmms_object_connect (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED,
			     handle_playlist_changed, NULL);
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

	xmms_playlist_entry_property_copy (core->curr_song, entry);
	
	return TRUE;
}

/**
 * Get url of current playing song.
 *
 * Returns a copy of the url to the song currently playing. I.e it
 * should be freed by the caller.
 *
 * @returns url or NULL if no song beeing played.
 */
gchar *
xmms_core_get_url ()
{
	if (core->curr_song)
		return xmms_playlist_entry_url_get (core->curr_song);
	return NULL;
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
	xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYBACK_PLAYTIME, GUINT_TO_POINTER (time) );
}
