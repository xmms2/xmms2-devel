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

#include <glib.h>
#include <stdlib.h>
#include <string.h>

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
};

static void xmms_core_effect_init (xmms_core_t *core);

/**
 * @internal
 * The Main loop of the core.
 */
static gpointer
xmms_core_thread (gpointer data)
{
	xmms_playlist_entry_t *entry = NULL;
	const gchar *mime;
	gboolean running = TRUE;
	xmms_core_t *core = data;

	while (running) {

		core->transport = NULL;
		core->decoder = NULL;
		
		/*
		while (core->status == XMMS_CORE_PLAYBACK_STOPPED) {
			XMMS_DBG ("Waiting until playback starts...");
			g_mutex_lock (core->mutex);
			g_cond_wait (core->cond, core->mutex);
			g_mutex_unlock (core->mutex);
		}
		*/

		/* Block until entry is ready. */
		while (!(entry = xmms_playback_entry (core->playback)))
			;

		XMMS_DBG ("Playing %s", xmms_playlist_entry_url_get (entry));
		
		core->transport = xmms_transport_new (core);

		if (!core->transport) {
			continue;
		}
		
		if  (!xmms_transport_open (core->transport, entry)) {
			xmms_transport_close (core->transport);
			continue;
		}
		
		xmms_transport_start (core->transport);
		
		/*
		 * Waiting for the mimetype forever
		 * All transporters MUST set a mimetype, 
		 * NULL on error
		 */
		XMMS_DBG ("Waiting for mimetype");
		mime = xmms_transport_mimetype_get_wait (core->transport);
		if (!mime) {
			xmms_transport_close (core->transport);
			continue;
		}

		XMMS_DBG ("mime-type: %s", mime);

		/* Make sure that mediainfo thread takes care of ALL playlists */

		xmms_playlist_entry_mimetype_set (entry, mime);

		core->decoder = xmms_decoder_new (core);
		if (!xmms_decoder_open (core->decoder, entry)) {
			xmms_transport_close (core->transport);
			xmms_decoder_destroy (core->decoder);
			continue;
		}

		xmms_decoder_start (core->decoder, core->transport, core->effects, core->output);

		/* 
		 * Ok, now decode the whole file,
		 * the decoder will wake this up when done
		 */
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
		xmms_decoder_destroy (core->decoder);
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

/**
 * Intializes a new core object
 */
xmms_core_t *
xmms_core_init (xmms_playlist_t *playlist)
{
	xmms_core_t *core;

	core = g_new0 (xmms_core_t, 1);
	
	xmms_playlist_core_set (playlist, core);
	
	xmms_object_init (XMMS_OBJECT (core));
	core->cond = g_cond_new ();
	core->mutex = g_mutex_new ();
	core->flush = FALSE;
	core->playback = xmms_playback_init (core, playlist);


	xmms_object_method_add (XMMS_OBJECT (core), XMMS_METHOD_QUIT, XMMS_METHOD_FUNC (quit));

	/** @todo really split this into to different objects */

	xmms_dbus_register_object ("core", XMMS_OBJECT (core));

	return core;
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
