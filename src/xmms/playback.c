/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *                   
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

/** @file
 *
 */

#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/decoder.h"
#include "xmms/config.h"
#include "xmms/playlist.h"
#include "xmms/plsplugins.h"
#include "xmms/playback.h"
#include "xmms/unixsignal.h"
#include "xmms/util.h"
#include "xmms/core.h"
#include "xmms/signal_xmms.h"
#include "xmms/magic.h"
#include "xmms/dbus.h"

#include "internal/transport_int.h"
#include "internal/decoder_int.h"
#include "internal/playback_int.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	XMMS_PLAYBACK_NEXT,
	XMMS_PLAYBACK_PREV,
	XMMS_PLAYBACK_HOLD,
} xmms_playback_pl_op_t;

typedef enum {
	XMMS_PLAYBACK_MODE_NONE,
	XMMS_PLAYBACK_MODE_REPEATALL,
	XMMS_PLAYBACK_MODE_REPEATONE,
	XMMS_PLAYBACK_MODE_STOPAFTERONE,
} xmms_playback_mode_t;

typedef enum {
	XMMS_PLAYBACK_PLAY,
	XMMS_PLAYBACK_STOP,
	XMMS_PLAYBACK_PAUSE,
} xmms_playback_status_t;


struct xmms_playback_St {
	xmms_object_t object;

	xmms_core_t *core;

	GCond *cond;
	GMutex *mtx;

	xmms_playlist_t *playlist;
	xmms_playlist_entry_t *current_song;
	xmms_mediainfo_thread_t *mediainfothread;

	xmms_playback_mode_t mode;
	xmms_playback_pl_op_t playlist_op;
	xmms_playback_status_t status;
};

/* Callbacks */
static void xmms_playback_next (xmms_playback_t *playback);
static void xmms_playback_prev (xmms_playback_t *playback);
static void xmms_playback_stop (xmms_playback_t *playback);
static void xmms_playback_start (xmms_playback_t *playback);
static void xmms_playback_pause (xmms_playback_t *playback);
static void xmms_playback_seekms (xmms_playback_t *playback, guint32 milliseconds);
static void xmms_playback_seeksamples (xmms_playback_t *playback, guint32 samples);
static void xmms_playback_jump (xmms_playback_t *playback, guint id);

/**
 * @internal
 */
static void
next_song (xmms_playback_t *playback)
{
	XMMS_DBG ("Next song");
	playback->current_song = xmms_playlist_get_next_entry (playback->playlist);
	if (playback->current_song) 
		xmms_object_emit (XMMS_OBJECT (playback), XMMS_SIGNAL_PLAYBACK_CURRENTID, 
	      			  GUINT_TO_POINTER (xmms_playlist_entry_id_get (playback->current_song)));
}

/**
 * @internal
 */
static void
prev_song (xmms_playback_t *playback)
{
	XMMS_DBG ("Prev song");
	playback->current_song = xmms_playlist_get_prev_entry (playback->playlist);
	if (playback->current_song) 
		xmms_object_emit (XMMS_OBJECT (playback), XMMS_SIGNAL_PLAYBACK_CURRENTID, 
				  GUINT_TO_POINTER (xmms_playlist_entry_id_get (playback->current_song)));
}

static void
xmms_playback_wakeup (xmms_playback_t *playback)
{
	XMMS_DBG ("Signaling");
	g_cond_signal (playback->cond);
}

XMMS_METHOD_DEFINE (jump, xmms_playback_jump, xmms_playback_t *, NONE, UINT32, NONE);
static void
xmms_playback_jump (xmms_playback_t *playback, guint id)
{
	g_return_if_fail (playback);

	xmms_playlist_set_current_position (playback->playlist, id);
	playback->current_song = xmms_playlist_get_current_entry (playback->playlist);
	if (playback->current_song) 
		xmms_object_emit (XMMS_OBJECT (playback), XMMS_SIGNAL_PLAYBACK_CURRENTID, 
				  GUINT_TO_POINTER (xmms_playlist_entry_id_get (playback->current_song)));
	playback->playlist_op = XMMS_PLAYBACK_HOLD;

	if (playback->status == XMMS_PLAYBACK_PLAY) {
		xmms_core_decoder_stop (playback->core);
		xmms_playback_wakeup (playback);
	}

}

XMMS_METHOD_DEFINE (next, xmms_playback_next, xmms_playback_t *, NONE, NONE, NONE);
static void
xmms_playback_next (xmms_playback_t *playback)
{
	if (playback->status == XMMS_PLAYBACK_PLAY) {

		playback->playlist_op = XMMS_PLAYBACK_NEXT;
		xmms_core_decoder_stop (playback->core);
		xmms_playback_wakeup (playback); /* Ding dong! */

		return;
	}

	next_song (playback);

}

XMMS_METHOD_DEFINE (prev, xmms_playback_prev, xmms_playback_t *, NONE, NONE, NONE);
static void
xmms_playback_prev (xmms_playback_t *playback)
{
	if (playback->status == XMMS_PLAYBACK_PLAY) {

		playback->playlist_op = XMMS_PLAYBACK_PREV;
		xmms_core_decoder_stop (playback->core);
		xmms_playback_wakeup (playback); /* Ding dong! */

		return;
	}

	prev_song (playback);
}

XMMS_METHOD_DEFINE (start, xmms_playback_start, xmms_playback_t *, NONE, NONE, NONE);
static void
xmms_playback_start (xmms_playback_t *playback)
{
	if (playback->status == XMMS_PLAYBACK_PAUSE) {
		xmms_output_resume (xmms_core_output_get (playback->core));
	}

	playback->status = XMMS_PLAYBACK_PLAY;
	xmms_playback_wakeup (playback);
}

XMMS_METHOD_DEFINE (stop, xmms_playback_stop, xmms_playback_t *, NONE, NONE, NONE);
static void
xmms_playback_stop (xmms_playback_t *playback)
{
	if (playback->status != XMMS_PLAYBACK_STOP) {
		playback->status = XMMS_PLAYBACK_STOP;
		xmms_core_flush_set (playback->core, TRUE);
		playback->playlist_op = XMMS_PLAYBACK_HOLD;
		xmms_object_emit (XMMS_OBJECT (playback), 
				  XMMS_SIGNAL_PLAYBACK_STATUS, 
				  GUINT_TO_POINTER (XMMS_PLAYBACK_STOP));
		xmms_core_decoder_stop (playback->core);
	}
}

XMMS_METHOD_DEFINE (pause, xmms_playback_pause, xmms_playback_t *, NONE, NONE, NONE);
static void
xmms_playback_pause (xmms_playback_t *playback)
{
	if (playback->status == XMMS_PLAYBACK_PLAY) {
		xmms_output_pause (xmms_core_output_get (playback->core));
		xmms_object_emit (XMMS_OBJECT (playback),
				XMMS_SIGNAL_PLAYBACK_STATUS,
				GUINT_TO_POINTER (XMMS_PLAYBACK_PAUSE));
		playback->status = XMMS_PLAYBACK_PAUSE;
	}

}

XMMS_METHOD_DEFINE (currentid, xmms_playback_currentid, xmms_playback_t *, UINT32, NONE, NONE);
guint
xmms_playback_currentid (xmms_playback_t *playback)
{
	if (!playback->current_song)
		return 0;
	return xmms_playlist_entry_id_get (playback->current_song);
}

XMMS_METHOD_DEFINE (seek_ms, xmms_playback_seekms, xmms_playback_t *, NONE, UINT32, NONE);
static void
xmms_playback_seekms (xmms_playback_t *playback, guint32 milliseconds)
{
	xmms_decoder_seek_ms (xmms_core_decoder_get (playback->core), milliseconds);
}

XMMS_METHOD_DEFINE (seek_samples, xmms_playback_seeksamples, xmms_playback_t *, NONE, UINT32, NONE);
static void
xmms_playback_seeksamples (xmms_playback_t *playback, guint32 samples)
{
	xmms_decoder_seek_samples (xmms_core_decoder_get (playback->core), samples);
}

void
xmms_playback_playtime_set (xmms_playback_t *playback, guint time)
{
	xmms_object_emit (XMMS_OBJECT (playback), XMMS_SIGNAL_PLAYBACK_PLAYTIME, GUINT_TO_POINTER (time));
}

static xmms_playback_mode_t
xmms_playback_modechr_to_int (const gchar *mode)
{
	if (g_strcasecmp (mode, "none") == 0)
		return XMMS_PLAYBACK_MODE_NONE;
	if (g_strcasecmp (mode, "repeatone") == 0)
		return XMMS_PLAYBACK_MODE_REPEATONE;
	if (g_strcasecmp (mode, "repeatall") == 0)
		return XMMS_PLAYBACK_MODE_REPEATALL;
	if (g_strcasecmp (mode, "stopafterone") == 0)
		return XMMS_PLAYBACK_MODE_STOPAFTERONE;

	return XMMS_PLAYBACK_MODE_NONE;
}

/**
 * @todo fix stopafterone
 */

xmms_playlist_entry_t *
xmms_playback_entry (xmms_playback_t *playback)
{
	xmms_playback_mode_t mode;
	g_mutex_lock (playback->mtx);
	
	mode = playback->mode;

	if (playback->current_song && playback->playlist_op != XMMS_PLAYBACK_HOLD)
		xmms_playlist_entry_unref (playback->current_song);
	
	if (playback->status == XMMS_PLAYBACK_STOP || 
	    playback->status == XMMS_PLAYBACK_PAUSE) {
		while (playback->status == XMMS_PLAYBACK_STOP || 
		       playback->status == XMMS_PLAYBACK_PAUSE) {
			XMMS_DBG ("Sleeping on playback cond");
			g_cond_wait (playback->cond, playback->mtx);
			XMMS_DBG ("Awake");
		}
	}


	if (playback->status == XMMS_PLAYBACK_PLAY) {
		if (playback->playlist_op == XMMS_PLAYBACK_NEXT) {
			if (mode == XMMS_PLAYBACK_MODE_REPEATONE) {
				playback->current_song = xmms_playlist_get_current_entry (playback->playlist);
				if (playback->current_song)
					xmms_object_emit (XMMS_OBJECT (playback), XMMS_SIGNAL_PLAYBACK_CURRENTID, 
							  GUINT_TO_POINTER (xmms_playlist_entry_id_get (playback->current_song)));
			} else {
				next_song (playback);
			}
		}
		else if (playback->playlist_op == XMMS_PLAYBACK_PREV)
			prev_song (playback);
	} 

	playback->playlist_op = XMMS_PLAYBACK_NEXT;

	XMMS_DBG ("Current song: %p", playback->current_song);

	if (!playback->current_song) {
		if (mode != XMMS_PLAYBACK_MODE_REPEATALL) {
			xmms_playback_stop (playback);
		}
		playback->playlist_op = XMMS_PLAYBACK_NEXT; /* ugly hack */
		g_mutex_unlock (playback->mtx);
		return NULL;
	}
	g_mutex_unlock (playback->mtx);
	return playback->current_song;
}

static void
mode_change (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_playback_t *playback = userdata;

	XMMS_DBG ("%s", (gchar *) data);
	playback->mode = xmms_playback_modechr_to_int ((gchar *)data);

}

static void
handle_playlist_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_object_emit (XMMS_OBJECT ((xmms_playback_t *)userdata), XMMS_SIGNAL_PLAYLIST_CHANGED, data);
}

static void
handle_playlist_entry_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_object_emit (XMMS_OBJECT ((xmms_playback_t *)userdata), XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID, data);
}

xmms_playback_t *
xmms_playback_init (xmms_core_t *core, xmms_playlist_t *playlist)
{
	xmms_playback_t *playback;
	xmms_config_value_t *val;

	g_return_val_if_fail (core, NULL);
	g_return_val_if_fail (playlist, NULL);

	playback = g_new0 (xmms_playback_t, 1);

	playback->cond = g_cond_new ();
	playback->mtx = g_mutex_new ();
	playback->playlist_op = XMMS_PLAYBACK_NEXT;
	val = xmms_config_value_register ("core.next_mode", "none", mode_change, playback);
	playback->mode = xmms_playback_modechr_to_int (xmms_config_value_string_get (val));

	xmms_object_init (XMMS_OBJECT (playback));

	xmms_object_method_add (XMMS_OBJECT (playback), XMMS_METHOD_PLAY, XMMS_METHOD_FUNC (start));
	xmms_object_method_add (XMMS_OBJECT (playback), XMMS_METHOD_STOP, XMMS_METHOD_FUNC (stop));
	xmms_object_method_add (XMMS_OBJECT (playback), XMMS_METHOD_PAUSE, XMMS_METHOD_FUNC (pause));
	xmms_object_method_add (XMMS_OBJECT (playback), XMMS_METHOD_NEXT, XMMS_METHOD_FUNC (next));
	xmms_object_method_add (XMMS_OBJECT (playback), XMMS_METHOD_PREV, XMMS_METHOD_FUNC (prev));
	xmms_object_method_add (XMMS_OBJECT (playback), XMMS_METHOD_CURRENTID, XMMS_METHOD_FUNC (currentid));
	xmms_object_method_add (XMMS_OBJECT (playback), XMMS_METHOD_SEEKMS, XMMS_METHOD_FUNC (seek_ms));
	xmms_object_method_add (XMMS_OBJECT (playback), XMMS_METHOD_SEEKSAMPLES, XMMS_METHOD_FUNC (seek_samples));
	xmms_object_method_add (XMMS_OBJECT (playback), XMMS_METHOD_JUMP, XMMS_METHOD_FUNC (jump));
	
	xmms_dbus_register_object ("playback", XMMS_OBJECT (playback));

	playback->playlist = playlist;
	xmms_object_connect (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED,
			     handle_playlist_changed, playback);
	xmms_object_connect (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID,
			     handle_playlist_entry_changed, playback);
	playback->core = core;

	playback->mediainfothread = xmms_mediainfo_thread_start (core, playlist);

	return playback;
}

void
xmms_playback_mediainfo_add_entry (xmms_playback_t *playback, xmms_playlist_entry_t *entry)
{

	g_return_if_fail (playback);
	g_return_if_fail (entry);

	xmms_mediainfo_thread_add (playback->mediainfothread, xmms_playlist_entry_id_get (entry));
}

xmms_playlist_t *
xmms_playback_playlist_get (xmms_playback_t *playback)
{
	return playback->playlist;
}

void
xmms_playback_destroy (xmms_playback_t *playback)
{
	g_free (playback);
}

void
xmms_playback_vis_spectrum (xmms_core_t *playback, gfloat *spec)
{
	xmms_object_emit (XMMS_OBJECT (playback), XMMS_SIGNAL_VISUALISATION_SPECTRUM, spec);
}

