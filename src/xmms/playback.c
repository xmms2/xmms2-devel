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

/** @file Playback support */

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

/**
 * @defgroup PlayBack PlayBack
 * @ingroup XMMSServer
 * @brief Playback support.
 * @{
 */
 

/** The different playlist operations. This describes
  * What to do when core is asking for the next entry
  * in the playlist
  */
typedef enum {
	/** Play next entry */
	XMMS_PLAYBACK_NEXT,
	/** Play previous entry */
	XMMS_PLAYBACK_PREV,
	/** Hold the same entry as now */
	XMMS_PLAYBACK_HOLD,
} xmms_playback_pl_op_t;

/** The different modes that the playback can act */
typedef enum {
	/** Normal mode, upon next, choose the next song */
	XMMS_PLAYBACK_MODE_NONE,
	/** Repeat the whole playlist when we reach the end */
	XMMS_PLAYBACK_MODE_REPEATALL,
	/** Repeat the same entry on and on and on and on .. */
	XMMS_PLAYBACK_MODE_REPEATONE,
	/** Stop right after we played one entry */
	XMMS_PLAYBACK_MODE_STOPAFTERONE,
} xmms_playback_mode_t;


/** Structure for the playback */
struct xmms_playback_St {
	xmms_object_t object;

	/** Core that we use */
	xmms_core_t *core;

	GCond *cond;
	GMutex *mtx;
	/** How far we have played */
	guint current_playtime;

	/** Playlist that tells us which songs to play */
	xmms_playlist_t *playlist;
	/** Current song playing */
	xmms_playlist_entry_t *current_song;
	xmms_mediainfo_thread_t *mediainfothread;

	/** The current mode */
	xmms_playback_mode_t mode;
	/** Next Playlist operation */ 
	xmms_playback_pl_op_t playlist_op;
	/** Are we playing or just ideling? */
	xmms_playback_status_t status;
};

/* Methods */
static void xmms_playback_next (xmms_playback_t *playback, xmms_error_t *err);
static void xmms_playback_prev (xmms_playback_t *playback, xmms_error_t *err);
static void xmms_playback_stop (xmms_playback_t *playback, xmms_error_t *err);
static void xmms_playback_start (xmms_playback_t *playback, xmms_error_t *err);
static void xmms_playback_pause (xmms_playback_t *playback, xmms_error_t *err);
static void xmms_playback_seekms (xmms_playback_t *playback, guint32 milliseconds, xmms_error_t *err);
static void xmms_playback_seeksamples (xmms_playback_t *playback, guint32 samples, xmms_error_t *err);
static void xmms_playback_jump (xmms_playback_t *playback, guint id, xmms_error_t *err);
static guint xmms_playback_status (xmms_playback_t *playback, xmms_error_t *err);
static guint xmms_playback_currentid (xmms_playback_t *playback, xmms_error_t *err);
static guint xmms_playback_current_playtime (xmms_playback_t *playback, xmms_error_t *err); 

/** 
 * Convinent macro for use inside playback.c
 * Will emit #signal on the playback object.
 */
#define XMMS_PLAYBACK_EMIT(signal,argument) { \
	xmms_object_method_arg_t *arg; \
	arg = xmms_object_arg_new (XMMS_OBJECT_METHOD_ARG_UINT32, \
				   GUINT_TO_POINTER (argument)); \
	xmms_object_emit (XMMS_OBJECT (playback), signal, arg); \
	g_free (arg);\
}
	

/**
 * @internal
 */
static xmms_playlist_entry_t *
next_song (xmms_playback_t *playback)
{
	xmms_playlist_entry_t *ret;
	ret = xmms_playlist_get_next_entry (playback->playlist);
	XMMS_DBG ("Next song %p", ret);
	return ret;
}

/**
 * @internal
 */
static xmms_playlist_entry_t *
prev_song (xmms_playback_t *playback)
{
	xmms_playlist_entry_t *ret;
	XMMS_DBG ("Prev song");
	ret = xmms_playlist_get_prev_entry (playback->playlist);
	return ret;
}

static void
xmms_playback_wakeup (xmms_playback_t *playback)
{
	XMMS_DBG ("Signaling");
	g_cond_signal (playback->cond);
}

/**
  * @defgroup PlaybackClientMethods PlaybackClientMethods
  * @ingroup PlayBack
  * @{
  */

/** This jumps to a certain ID in the playlist and plays it,
  * if status is playing. Otherwise it just aligns the playlist for
  * next entry 
  */
XMMS_METHOD_DEFINE (jump, xmms_playback_jump, xmms_playback_t *, NONE, UINT32, NONE);
static void
xmms_playback_jump (xmms_playback_t *playback, guint id, xmms_error_t *err)
{
	g_return_if_fail (playback);

	if (!xmms_playlist_set_current_position (playback->playlist, id)) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Trying to jump to nonexistant playlist entry");
		return;
	}
	playback->current_song = xmms_playlist_get_current_entry (playback->playlist);
	if (playback->current_song) 
		XMMS_PLAYBACK_EMIT (XMMS_SIGNAL_PLAYBACK_CURRENTID,
				    xmms_playlist_entry_id_get (playback->current_song));

	playback->playlist_op = XMMS_PLAYBACK_HOLD;

	if (playback->status == XMMS_PLAYBACK_PLAY) {
		xmms_core_decoder_stop (playback->core);
		xmms_core_flush_set (playback->core, TRUE);
		xmms_playback_wakeup (playback);
	}

}

/**
  * This jumps to the next song in the playlist
  */
XMMS_METHOD_DEFINE (next, xmms_playback_next, xmms_playback_t *, NONE, NONE, NONE);
static void
xmms_playback_next (xmms_playback_t *playback, xmms_error_t *err)
{
	if (playback->status == XMMS_PLAYBACK_PLAY) {

		playback->playlist_op = XMMS_PLAYBACK_NEXT;
		xmms_core_decoder_stop (playback->core);
		xmms_core_flush_set (playback->core, TRUE);
		xmms_playback_wakeup (playback); /* Ding dong! */

		return;
	}

	next_song (playback);

}


/**
  * This jumps to the previous song in the playlist
  */
XMMS_METHOD_DEFINE (prev, xmms_playback_prev, xmms_playback_t *, NONE, NONE, NONE);
static void
xmms_playback_prev (xmms_playback_t *playback, xmms_error_t *err)
{
	if (playback->status == XMMS_PLAYBACK_PLAY) {

		playback->playlist_op = XMMS_PLAYBACK_PREV;
		xmms_core_decoder_stop (playback->core);
		xmms_core_flush_set (playback->core, TRUE);
		xmms_playback_wakeup (playback); /* Ding dong! */

		return;
	}

	prev_song (playback);
}

/**
  * Starts playback
  */
XMMS_METHOD_DEFINE (start, xmms_playback_start, xmms_playback_t *, NONE, NONE, NONE);
static void
xmms_playback_start (xmms_playback_t *playback, xmms_error_t *err)
{
	if (playback->status == XMMS_PLAYBACK_PLAY)
		return;

	if (playback->status == XMMS_PLAYBACK_PAUSE) {
		xmms_output_resume (xmms_core_output_get (playback->core));
	}
	
	playback->status = XMMS_PLAYBACK_PLAY;
	xmms_playback_wakeup (playback);
	
	XMMS_PLAYBACK_EMIT (XMMS_SIGNAL_PLAYBACK_STATUS, XMMS_PLAYBACK_PLAY);
}

/**
  * Stops playback and destroys the decoder
  */
XMMS_METHOD_DEFINE (stop, xmms_playback_stop, xmms_playback_t *, NONE, NONE, NONE);
static void
xmms_playback_stop (xmms_playback_t *playback, xmms_error_t *err)
{
	if (playback->status != XMMS_PLAYBACK_STOP) {
		playback->status = XMMS_PLAYBACK_STOP;
		xmms_core_flush_set (playback->core, TRUE);
		playback->playlist_op = XMMS_PLAYBACK_HOLD;
		XMMS_PLAYBACK_EMIT (XMMS_SIGNAL_PLAYBACK_STATUS, XMMS_PLAYBACK_STOP);
		xmms_core_decoder_stop (playback->core);
	}
}

/**
  * Suspends playback
  */
XMMS_METHOD_DEFINE (pause, xmms_playback_pause, xmms_playback_t *, NONE, NONE, NONE);
static void
xmms_playback_pause (xmms_playback_t *playback, xmms_error_t *err)
{
	if (playback->status == XMMS_PLAYBACK_PLAY) {
		xmms_output_pause (xmms_core_output_get (playback->core));
		XMMS_PLAYBACK_EMIT (XMMS_SIGNAL_PLAYBACK_STATUS, XMMS_PLAYBACK_PAUSE);
		playback->status = XMMS_PLAYBACK_PAUSE;
	}

}

/**
  * Gives the client the current playing ID
  */
XMMS_METHOD_DEFINE (currentid, xmms_playback_currentid, xmms_playback_t *, UINT32, NONE, NONE);
static guint
xmms_playback_currentid (xmms_playback_t *playback, xmms_error_t *err)
{
	if (!playback->current_song)
		return 0;
	return xmms_playlist_entry_id_get (playback->current_song);
}

/**
  * Gives the client the status
  */
XMMS_METHOD_DEFINE (status, xmms_playback_status, xmms_playback_t *, UINT32, NONE, NONE);
static guint
xmms_playback_status (xmms_playback_t *playback, xmms_error_t *err)
{
	return (guint) playback->status;
}

/**
  * Seeks the specified number of milliseconds
  */
XMMS_METHOD_DEFINE (seek_ms, xmms_playback_seekms, xmms_playback_t *, NONE, UINT32, NONE);
static void
xmms_playback_seekms (xmms_playback_t *playback, guint32 milliseconds, xmms_error_t *err)
{
	xmms_decoder_seek_ms (xmms_core_decoder_get (playback->core), milliseconds, err);
}

/**
  * Seeks the specified number of samples
  */
XMMS_METHOD_DEFINE (seek_samples, xmms_playback_seeksamples, xmms_playback_t *, NONE, UINT32, NONE);
static void
xmms_playback_seeksamples (xmms_playback_t *playback, guint32 samples, xmms_error_t *err)
{
	xmms_decoder_seek_samples (xmms_core_decoder_get (playback->core), samples, err);
}

/**
  * Gives the client the playback time in milliseconds
  */
XMMS_METHOD_DEFINE (current_playtime, xmms_playback_current_playtime, xmms_playback_t *, UINT32, NONE, NONE);
static guint
xmms_playback_current_playtime (xmms_playback_t *playback, xmms_error_t *err)
{
	return playback->current_playtime;
}

static const gchar *
status_str (xmms_playback_t *playback)
{
	g_return_val_if_fail (playback, NULL);

	if (playback->status == XMMS_PLAYBACK_PLAY)
		return "playing";
	else if (playback->status == XMMS_PLAYBACK_STOP)
		return "stopped";
	else if (playback->status == XMMS_PLAYBACK_PAUSE)
		return "paused";

	return "unknown";
}

static GList *xmms_playback_stats (xmms_playback_t *playback, xmms_error_t *err);

/**
  * Shows some statistics about the Server
  */
XMMS_METHOD_DEFINE (statistics, xmms_playback_stats, xmms_playback_t *, STRINGLIST, NONE, NONE);

static GList *
xmms_playback_stats (xmms_playback_t *playback, xmms_error_t *err)
{
	GList *ret = NULL;
	gchar *tmp;

	ret = xmms_output_stats (xmms_core_output_get (playback->core), ret);
	ret = xmms_core_stats (playback->core, ret);
	ret = xmms_transport_stats (xmms_core_transport_get (playback->core), ret);
	ret = xmms_playlist_stats (playback->playlist, ret);
	ret = xmms_dbus_stats (ret);

	/* Add more values */
	tmp = g_strdup_printf ("playback.status=%s", status_str (playback));
	ret = g_list_append (ret, tmp);

	/** @todo This list is horribly leaked! Should we have
	  * a _free callback to METHOD_DEFINE? yes maybe */
	return ret;
}

/** @} */

/**
  * Sets the playback time to #time
  * @param playback the playback to modify
  * @param time the current playback time in milliseconds
  */
void
xmms_playback_playtime_set (xmms_playback_t *playback, guint time)
{
	xmms_object_method_arg_t *arg;
	playback->current_playtime = time;

	arg = xmms_object_arg_new (XMMS_OBJECT_METHOD_ARG_UINT32, GUINT_TO_POINTER (time));
	xmms_object_emit (XMMS_OBJECT (playback), XMMS_SIGNAL_PLAYBACK_PLAYTIME, arg);
	g_free (arg);
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

void
xmms_playback_active_entry_set (xmms_playback_t *playback, 
				xmms_playlist_entry_t *entry)
{
	g_return_if_fail (playback);
	g_return_if_fail (entry);

	if (playback->current_song) {
		xmms_playlist_entry_unref (playback->current_song);
	}
	playback->current_song = entry;
	XMMS_PLAYBACK_EMIT (XMMS_SIGNAL_PLAYBACK_CURRENTID,
			    xmms_playlist_entry_id_get (entry));
}

/**
 * Return the next entry in the playlist, the one to be played.
 * @param playback the #xmms_playback_t to query
 * @return next #xmms_playlist_entry_t to play
 * @todo fix stopafterone
 */

xmms_playlist_entry_t *
xmms_playback_entry (xmms_playback_t *playback)
{
	xmms_playback_mode_t mode;
	xmms_playlist_entry_t *ret;
	xmms_error_t err;

	xmms_error_reset (&err);

	g_mutex_lock (playback->mtx);
	
	mode = playback->mode;

	/*
	if (playback->current_song && playback->playlist_op != XMMS_PLAYBACK_HOLD)
		xmms_playlist_entry_unref (playback->current_song);
	*/
	
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
				ret = xmms_playlist_get_current_entry (playback->playlist);
			} else {
				ret = next_song (playback);
			}
		}
		else if (playback->playlist_op == XMMS_PLAYBACK_PREV)
			ret = prev_song (playback);
	} 

	playback->playlist_op = XMMS_PLAYBACK_NEXT;

	if (!ret) {
		if (mode != XMMS_PLAYBACK_MODE_REPEATALL) {
			xmms_playback_stop (playback, &err);
		}
		playback->playlist_op = XMMS_PLAYBACK_NEXT; /* ugly hack */
		g_mutex_unlock (playback->mtx);
		return NULL;
	}
	g_mutex_unlock (playback->mtx);
	return ret;
}

static void
mode_change (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_playback_t *playback = userdata;

	XMMS_DBG ("%s", (gchar *) data);
	playback->mode = xmms_playback_modechr_to_int ((gchar *)data);

}

/**
  * Initialize a new #xmms_playback_t for use by core.
  *
  * @param core a core to be linked to this playback.
  * @param playlist the #xmms_playlist_t to be used.
  * @returns newly allocated #xmms_playback_t
  */
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
	playback->playlist = playlist;
	playback->core = core;
	playback->mediainfothread = xmms_mediainfo_thread_start (core, playlist);

	val = xmms_config_value_register ("core.next_mode", "none", mode_change, playback);
	playback->mode = xmms_playback_modechr_to_int (xmms_config_value_string_get (val));
	playback->status = XMMS_PLAYBACK_STOP;

	xmms_object_init (XMMS_OBJECT (playback));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_PLAY, 
				XMMS_METHOD_FUNC (start));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_STOP, 
				XMMS_METHOD_FUNC (stop));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_PAUSE, 
				XMMS_METHOD_FUNC (pause));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_NEXT, 
				XMMS_METHOD_FUNC (next));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_PREV, 
				XMMS_METHOD_FUNC (prev));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_CURRENTID, 
				XMMS_METHOD_FUNC (currentid));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_SEEKMS, 
				XMMS_METHOD_FUNC (seek_ms));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_SEEKSAMPLES, 
				XMMS_METHOD_FUNC (seek_samples));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_CPLAYTIME, 
				XMMS_METHOD_FUNC (current_playtime));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_JUMP, 
				XMMS_METHOD_FUNC (jump));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_STATUS, 
				XMMS_METHOD_FUNC (status));

	xmms_object_method_add (XMMS_OBJECT (playback), 
				XMMS_METHOD_STATS, 
				XMMS_METHOD_FUNC (statistics));

	xmms_dbus_register_object ("playback", XMMS_OBJECT (playback));

	xmms_dbus_register_onchange (XMMS_OBJECT (playback), 
				     XMMS_SIGNAL_PLAYBACK_PLAYTIME);
	xmms_dbus_register_onchange (XMMS_OBJECT (playback), 
				     XMMS_SIGNAL_PLAYBACK_CURRENTID);
	xmms_dbus_register_onchange (XMMS_OBJECT (playback), 
				     XMMS_SIGNAL_PLAYBACK_STATUS);

	return playback;
}

/**
  * Calls #xmms_mediainfo_thread_add to schedule a mediainfo extraction.
  * @param playback a #xmms_playback_t
  * @param entry the entry to schedule
  */
void
xmms_playback_mediainfo_add_entry (xmms_playback_t *playback, xmms_playlist_entry_t *entry)
{

	g_return_if_fail (playback);
	g_return_if_fail (entry);

	xmms_mediainfo_thread_add (playback->mediainfothread, xmms_playlist_entry_id_get (entry));
}

/**
  * Fetch the #xmms_playlist_t from the playback
  * 
  * @return a #xmms_playlist_t that are used by the playback
  */
xmms_playlist_t *
xmms_playback_playlist_get (xmms_playback_t *playback)
{
	return playback->playlist;
}

/**
  * Deallocate all memory used by the playback
  */
void
xmms_playback_destroy (xmms_playback_t *playback)
{
	g_free (playback);
}

/** @} */
