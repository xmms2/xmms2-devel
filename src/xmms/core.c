/** @file
 *
 */

#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/decoder.h"
#include "xmms/config.h"
#include "xmms/playlist.h"
#include "xmms/plsplugins.h"
#include "xmms/unixsignal.h"
#include "xmms/util.h"
#include "xmms/core.h"
#include "xmms/signal_xmms.h"
#include "xmms/magic.h"
#include "xmms/dbus.h"

#include "internal/transport_int.h"
#include "internal/decoder_int.h"

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
	
	GCond *cond;
	GMutex *mutex;
	
	gint status;
	gint playlist_op;

	gboolean flush;
};


static gboolean running = TRUE;
/**
 *
 */

static void xmms_core_effect_init (void);


static void
handle_mediainfo_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYBACK_CURRENTID, data);
}


static guint32
handle_current_id_request (xmms_core_t *core)
{
	return xmms_core_get_id (core);

}
XMMS_METHOD_DEFINE (currid, handle_current_id_request, xmms_core_t *, UINT32, NONE, NONE);

static gboolean
next_song ()
{
	if (core->curr_song)
		xmms_playlist_entry_unref (core->curr_song);

	core->curr_song = xmms_playlist_get_next_entry (core->playlist);

	if (!core->curr_song) {
		xmms_core_playback_stop (core);
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
		xmms_core_playback_stop (core);
		return FALSE;
	}
	return TRUE;
}

static void
eos_reached (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	XMMS_DBG ("eos_reached");

}

static void
open_fail (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_core_playback_stop (core);
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
	xmms_object_connect (XMMS_OBJECT (output), XMMS_SIGNAL_OUTPUT_OPEN_FAIL, open_fail, NULL);
}

static void
wake_core ()
{
	if (core->decoder) {
		xmms_decoder_destroy (core->decoder);
		core->decoder = NULL;
	}
}

/**
 * Play next song in playlist.
 *
 * Stops the playing of the current song, and frees any resources used
 * by the decoder and its transport. Then starts playing the next
 * song in playlist.
 *
 */

XMMS_METHOD_DEFINE (next, xmms_core_play_next, xmms_core_t *, NONE, NONE, NONE);
void
xmms_core_play_next (xmms_core_t *core)
{
	if (core->status == XMMS_CORE_PLAYBACK_RUNNING) {
		core->playlist_op = XMMS_CORE_NEXT_SONG;
		core->flush = TRUE;
		wake_core ();
	} else {
		next_song ();
	}
}

XMMS_METHOD_DEFINE (prev, xmms_core_play_prev, xmms_core_t *, NONE, NONE, NONE);
void
xmms_core_play_prev (xmms_core_t *core)
{
	if (core->status == XMMS_CORE_PLAYBACK_RUNNING) {
		core->playlist_op = XMMS_CORE_PREV_SONG;
		core->flush = TRUE;
		wake_core ();
	} else {
		prev_song ();
	}
}

XMMS_METHOD_DEFINE (stop, xmms_core_playback_stop, xmms_core_t *, NONE, NONE, NONE);

void
xmms_core_playback_stop (xmms_core_t *core)
{
	if (core->status == XMMS_CORE_PLAYBACK_RUNNING) {
		core->status = XMMS_CORE_PLAYBACK_STOPPED;
		core->playlist_op = XMMS_CORE_HOLD_SONG;
		xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYBACK_STATUS, GUINT_TO_POINTER (XMMS_CORE_PLAYBACK_STOPPED));
		core->flush = TRUE;
		wake_core ();
	} else {
		XMMS_DBG ("xmms_core_playback_stop with status != XMMS_CORE_PLAYBACK_RUNNING");
	}
}

void
xmms_core_playlist_mode_set (xmms_playlist_mode_t mode)
{
	xmms_playlist_mode_set (core->playlist, mode);
}

xmms_playlist_mode_t
xmms_core_playlist_mode_get ()
{
	return (xmms_playlist_mode_get (core->playlist));
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


XMMS_METHOD_DEFINE (pause, xmms_core_playback_pause, xmms_core_t *, NONE, NONE, NONE);
void
xmms_core_playback_pause (xmms_core_t *core)
{
	if (core->status == XMMS_CORE_PLAYBACK_RUNNING) {
		core->status = XMMS_CORE_PLAYBACK_PAUSED;
		xmms_output_pause (core->output);
		xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYBACK_STATUS, GUINT_TO_POINTER (XMMS_CORE_PLAYBACK_PAUSED));
	}
}

XMMS_METHOD_DEFINE (start, xmms_core_playback_start, xmms_core_t *, NONE, NONE, NONE);
void
xmms_core_playback_start (xmms_core_t *core)
{
	if (core->status == XMMS_CORE_PLAYBACK_PAUSED) {
		core->status = XMMS_CORE_PLAYBACK_RUNNING;
		xmms_output_resume (core->output);
		xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYBACK_STATUS, GUINT_TO_POINTER (XMMS_CORE_PLAYBACK_RUNNING));
	}
	
	if (core->status == XMMS_CORE_PLAYBACK_STOPPED) {
		/* @todo race condition? */
		core->status = XMMS_CORE_PLAYBACK_RUNNING;
		xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYBACK_STATUS, GUINT_TO_POINTER (XMMS_CORE_PLAYBACK_RUNNING));
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
xmms_core_playlist_sort (gchar *property)
{
	xmms_playlist_sort (core->playlist, property);
}

void
xmms_core_playlist_clear ()
{
	/** @todo Kanske inte skitsnygt. */
	xmms_core_playback_stop (core);
	xmms_playlist_entry_unref (core->curr_song);
	core->curr_song = NULL;
	xmms_playlist_clear (core->playlist);
}

void
xmms_core_playlist_remove (guint id)
{
	xmms_playlist_id_remove (core->playlist, id);
}

XMMS_METHOD_DEFINE (seek_ms, xmms_core_playback_seek_ms, xmms_core_t *, NONE, UINT32, NONE);

void
xmms_core_playback_seek_ms (xmms_core_t *core, guint32 milliseconds)
{
	xmms_decoder_seek_ms (core->decoder, milliseconds);
}

XMMS_METHOD_DEFINE (seek_samples, xmms_core_playback_seek_samples, xmms_core_t *, NONE, UINT32, NONE);
void
xmms_core_playback_seek_samples (xmms_core_t *core, guint32 samples)
{
	xmms_decoder_seek_samples (core->decoder, samples);
}

XMMS_METHOD_DEFINE (quit, xmms_core_quit, xmms_core_t *, NONE, NONE, NONE);
void
xmms_core_quit (xmms_core_t *core)
{
	gchar *filename;
	filename = g_strdup_printf ("%s/.xmms2/xmms2.conf", g_get_home_dir ());
	xmms_config_save (filename);
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

	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_PLAY, XMMS_METHOD_FUNC (start));
	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_STOP, XMMS_METHOD_FUNC (stop));
	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_PAUSE, XMMS_METHOD_FUNC (pause));
	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_NEXT, XMMS_METHOD_FUNC (next));
	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_PREV, XMMS_METHOD_FUNC (prev));
	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_CURRENTID, XMMS_METHOD_FUNC (currid));
	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_SEEKMS, XMMS_METHOD_FUNC (seek_ms));
	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_SEEKSAMPLES, XMMS_METHOD_FUNC (seek_samples));

	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_QUIT, XMMS_METHOD_FUNC (quit));

	/** @todo really split this into to different objects */

	xmms_dbus_register_object ("playback", XMMS_OBJECT (core));
	xmms_dbus_register_object ("core", XMMS_OBJECT (core));

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
		
		if (core->curr_song) 
			xmms_playlist_entry_unref (core->curr_song);

		core->curr_song = xmms_playlist_get_current_entry (core->playlist);

		if (!core->curr_song) {
			core->status = XMMS_CORE_PLAYBACK_STOPPED;
			continue;
		}
		
		XMMS_DBG ("Playing %s", xmms_playlist_entry_url_get (core->curr_song));
		
		core->transport = xmms_transport_new ();

		if (!core->transport) {
			continue;
		}
		
		if  (!xmms_transport_open (core->transport, core->curr_song)) {
			xmms_transport_close (core->transport);
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
			xmms_core_play_next (core);
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
			xmms_core_play_next (core);
		}

		xmms_playlist_entry_mimetype_set (core->curr_song, mime);

		core->decoder = xmms_decoder_new ();
		if (!xmms_decoder_open (core->decoder, core->curr_song)) {
			xmms_decoder_destroy (core->decoder);
			xmms_core_play_next (core);
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
			core->decoder = NULL;
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
xmms_core_start (void)
{
	core->mediainfothread = xmms_mediainfo_thread_start (core->playlist);
	xmms_core_effect_init ();
	xmms_dbus_register_object ("playlist", XMMS_OBJECT (core->playlist));
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
xmms_core_playlist_mediainfo_changed (guint id)
{
	xmms_object_emit (XMMS_OBJECT (core), XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID, 
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
xmms_core_get_id (xmms_core_t *core)
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

static void
xmms_core_effect_init (void)
{
	xmms_config_value_t *cv;
	const gchar *order;
	gchar **list;
	gint i;

	core->effects = NULL;

	cv = xmms_config_value_register ("core.effectorder", "", NULL, NULL);
	order = xmms_config_value_string_get (cv);
	XMMS_DBG ("effectorder = '%s'", order);

	list = g_strsplit (order, ",", 0);

	for (i = 0; list[i]; i++) {
		XMMS_DBG ("adding effect '%s'", list[i]);
		core->effects = xmms_effect_prepend (core->effects, list[i]);
	}

	g_strfreev (list);

}
