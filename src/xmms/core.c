/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundstr�m, Anders Gustafsson
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
#include "xmms/playback.h"
#include "xmms/plsplugins.h"
#include "xmms/unixsignal.h"
#include "xmms/util.h"
#include "xmms/core.h"
#include "xmms/signal_xmms.h"
#include "xmms/magic.h"
#include "xmms/dbus.h"

#include "internal/transport_int.h"
#include "internal/decoder_int.h"
#include "internal/output_int.h"
#include "internal/playback_int.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

/** @defgroup Core Core
  * @ingroup XMMSServer
  * @{
  */

/** Core object */
struct xmms_core_St {
	xmms_object_t object;

	xmms_output_t *output;
	xmms_decoder_t *decoder;
	xmms_transport_t *transport;

	xmms_playback_t *playback;

	xmms_mediainfo_thread_t *mediainfothread;

	xmms_effect_t *effects;
	
	GCond *cond;
	GMutex *mutex;
	
	gint status;

	gboolean flush;
	/** For calculation of uptime */
	guint startuptime; 
	guint songs_played;
};


gboolean
xmms_core_play_entry (xmms_core_t *core, xmms_playlist_entry_t *entry,
		      xmms_decoder_t **decoder, xmms_transport_t **transport)
{
	xmms_transport_t *t;
	xmms_decoder_t *d;
	const gchar *mime;

	XMMS_DBG ("Starting up for %s", xmms_playlist_entry_url_get (entry));

	t = xmms_transport_new (core);

	if (!t) {
		return FALSE;
	}

	if (!xmms_transport_open (t, entry)) {
		xmms_transport_close (t);
		return FALSE;
	}

	xmms_transport_start (t);

	/*
	 * Waiting for the mimetype forever
	 * All transporters MUST set a mimetype, 
	 * NULL on error
	 */
	XMMS_DBG ("Waiting for mimetype");
	mime = xmms_transport_mimetype_get_wait (t);
	if (!mime) {
		xmms_transport_close (t);
		return FALSE;
	}

	XMMS_DBG ("mime-type: %s", mime);

	xmms_playlist_entry_mimetype_set (entry, mime);

	d = xmms_decoder_new (core);

	if (!d) {
		xmms_transport_close (t);
		return FALSE;
	}

	if (!xmms_decoder_open (d, entry)) {
		xmms_transport_close (t);
		xmms_object_unref (d);
		return FALSE;
	}

	xmms_decoder_start (d, t, core->effects, core->output);

	/* make sure that the output can use this decoder */
	xmms_output_decoder_append (core->output, d);

	*transport = t;
	*decoder = d;

	return TRUE;
}


static void xmms_core_effect_init (xmms_core_t *core);

/**
 * @internal
 * The Main loop of the core.
 */
static gpointer
xmms_core_thread (gpointer data)
{
	xmms_playlist_entry_t *entry = NULL;
	xmms_decoder_t *decoder = NULL;
	xmms_transport_t *transport = NULL;
	gboolean running = TRUE;
	xmms_core_t *core = data;

	core->startuptime = xmms_util_time ();
	
	while (running) {

		if (!transport && !decoder) {
			entry = xmms_playback_entry (core->playback);
		
			if (!xmms_core_play_entry (core, entry, &decoder, &transport)) {
				continue;
			}
		} else {
			xmms_playback_wait_for_play (core->playback);
			XMMS_DBG ("Core is waiting for action!");
		}

		core->transport = transport;
		core->decoder = decoder;

		xmms_playback_active_entry_set (core->playback, entry);

		transport = NULL;
		decoder = NULL;

		/*
		 * Wait for the playback to say that we have two seconds left
		 */

		/*
		xmms_playback_playtime_wait (core->playback, 10000);
		*/
			
		entry = xmms_playback_entry (core->playback);

		while (entry && !xmms_core_play_entry (core, entry, &decoder, &transport)) {
			entry = xmms_playback_entry (core->playback);
		}

		if (!entry) {
			decoder = NULL;
			transport = NULL;
		}

		/* 
		 * the decoder will wake this up when done
		 */

		xmms_decoder_wait (core->decoder);
		core->songs_played++; 

		XMMS_DBG ("destroying decoder");
		if (core->decoder) {
			xmms_decoder_stop (core->decoder);
			xmms_object_unref (core->decoder);
			core->decoder = NULL;
		}
		XMMS_DBG ("closing transport");
		if (core->transport)
			xmms_object_unref (core->transport);
		if (core->flush) {
			XMMS_DBG ("Flushing buffers");
			if (core->output)
				xmms_output_flush (core->output);
			core->flush = FALSE;
		}
		
	}

	return NULL;
}

xmms_playlist_t *
xmms_core_playlist_get (xmms_core_t *core)
{
	g_return_val_if_fail (core, NULL);
	return xmms_playback_playlist_get (core->playback);
}

xmms_playback_t *
xmms_core_playback_get (xmms_core_t *core)
{
	g_return_val_if_fail (core, NULL);
	return core->playback;
}

xmms_decoder_t *
xmms_core_decoder_get (xmms_core_t *core)
{
	g_return_val_if_fail (core, NULL);
	return core->decoder;
}

xmms_transport_t *
xmms_core_transport_get (xmms_core_t *core)
{
	g_return_val_if_fail (core, NULL);
	return core->transport;
}

xmms_output_t *
xmms_core_output_get (xmms_core_t *core)
{
	g_return_val_if_fail (core, NULL);
	return core->output;
}

void
xmms_core_decoder_stop (xmms_core_t *core)
{
	g_return_if_fail (core);
	if (core->decoder)
		xmms_decoder_stop (core->decoder);
}

void
xmms_core_flush_set (xmms_core_t *core, gboolean b)
{
	core->flush = b;
}

XMMS_METHOD_DEFINE (quit, xmms_core_quit, xmms_core_t *, NONE, NONE, NONE);
void
xmms_core_quit (xmms_core_t *core, xmms_error_t *err)
{
	gchar *filename;
	filename = g_strdup_printf ("%s/.xmms2/xmms2.conf", g_get_home_dir ());
	xmms_config_save (filename);
	/* xmms_output_destroy (core->output); */
	/* remove socket in /tmp */
	exit (0); /** @todo BUSKIS! */
}

static void
xmms_core_destroy (xmms_object_t *object)
{
	xmms_core_t *core = (xmms_core_t *)object;
	g_cond_free (core->cond);
	g_mutex_free (core->mutex);
	xmms_object_unref (core->playback);
}

/**
 * Intializes a new core object
 */
xmms_core_t *
xmms_core_init (xmms_playlist_t *playlist)
{
	xmms_core_t *core;

	core = xmms_object_new (xmms_core_t, xmms_core_destroy);

	xmms_playlist_core_set (playlist, core);
	
	core->cond = g_cond_new ();
	core->mutex = g_mutex_new ();
	core->flush = FALSE;
	core->playback = xmms_playback_init (core, playlist);

	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_QUIT, XMMS_METHOD_FUNC (quit));

	xmms_dbus_register_object ("core", XMMS_OBJECT (core));

	return core;
}

GList *
xmms_core_stats (xmms_core_t *core, GList *list)
{
	gchar *tmp;

	g_return_val_if_fail (core, NULL);
	g_return_val_if_fail (list, NULL);

	tmp = g_strdup_printf ("core.uptime=%u", xmms_util_time()-core->startuptime);
	list = g_list_append (list, tmp);
	tmp = g_strdup_printf ("core.songs_played=%u", core->songs_played);
	list = g_list_append (list, tmp);

	return list;
}

/**
 * Start the core thread and main operations
 */
void
xmms_core_start (xmms_core_t *core)
{
	xmms_core_effect_init (core);
	g_thread_create (xmms_core_thread, core, FALSE, NULL);
}

void
xmms_core_output_set (xmms_core_t *core, xmms_output_t *output)
{
	g_return_if_fail (output);
	g_return_if_fail (core);

	core->output = output;
}


/**
 * Init the effect engine
 */
static void
xmms_core_effect_init (xmms_core_t *core)
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

/** @} */

