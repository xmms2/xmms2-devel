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




/** @file This file controls the mediainfothread.
  *
  */

#include "xmms/xmms.h"
#include "xmms/transport.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/playlist.h"
#include "xmms/mediainfo.h"
#include "xmms/medialib.h"
#include "xmms/plsplugins.h"
#include "xmms/signal_xmms.h"

#include "internal/transport_int.h"
#include "internal/decoder_int.h"

#include <glib.h>

struct xmms_mediainfo_thread_St {
	GThread *thread;
	GMutex *mutex;
	GCond *cond;

	gboolean running;
	GQueue *queue;
	xmms_playlist_t *playlist;
	/* 45574 */
};

static gpointer xmms_mediainfo_thread_thread (gpointer data);
static void xmms_mediainfo_playlist_changed_cb (xmms_object_t *object, gconstpointer arg, gpointer userdata);


xmms_mediainfo_thread_t *
xmms_mediainfo_thread_start (xmms_playlist_t *playlist)
{
	xmms_mediainfo_thread_t *mtt;

	g_return_val_if_fail (playlist, NULL);

	mtt = g_new0 (xmms_mediainfo_thread_t, 1);

	mtt->mutex = g_mutex_new ();
	mtt->cond = g_cond_new ();
	mtt->playlist = playlist;
	mtt->queue = g_queue_new ();
	mtt->running = TRUE;
	mtt->thread = g_thread_create (xmms_mediainfo_thread_thread, mtt, FALSE, NULL);

	xmms_object_connect (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, xmms_mediainfo_playlist_changed_cb, mtt);
	

	return mtt;
}

void
xmms_mediainfo_thread_stop (xmms_mediainfo_thread_t *mit)
{
	g_mutex_lock (mit->mutex);

	while (g_queue_pop_head (mit->queue))
		;
	mit->running = FALSE;
	g_cond_signal (mit->cond);

	g_mutex_unlock (mit->mutex);

	g_thread_join (mit->thread);

	g_queue_free (mit->queue);
}

static void
xmms_mediainfo_playlist_changed_cb (xmms_object_t *object, gconstpointer arg, gpointer userdata)
{
	xmms_mediainfo_thread_t *mit = userdata;
	const xmms_object_method_arg_t *oarg = arg;
	xmms_playlist_changed_msg_t *chmsg = oarg->retval.plch;

	if (chmsg->type == XMMS_PLAYLIST_CHANGED_ADD) {
		g_mutex_lock (mit->mutex);

		g_queue_push_tail (mit->queue, GUINT_TO_POINTER (chmsg->id));

		g_cond_signal (mit->cond);

		g_mutex_unlock (mit->mutex);
	}
}

static gpointer
xmms_mediainfo_thread_thread (gpointer data)
{
	xmms_mediainfo_thread_t *mtt = (xmms_mediainfo_thread_t *) data;

	g_mutex_lock (mtt->mutex);

	while (mtt->running) {
		xmms_playlist_entry_t *entry;
		guint id;
		

		XMMS_DBG ("MediainfoThread is idle.");
		g_cond_wait (mtt->cond, mtt->mutex);

		XMMS_DBG ("MediainfoThread is awake!");

		while ((id = GPOINTER_TO_UINT (g_queue_pop_head (mtt->queue)))) {
			xmms_transport_t *transport;
			xmms_playlist_plugin_t *plsplugin;
			xmms_decoder_t *decoder;
			xmms_error_t err;
			const gchar *mime;

			xmms_error_reset (&err);

			g_mutex_unlock (mtt->mutex);

			entry = xmms_playlist_get_byid (mtt->playlist, id, &err);

			/* Check if this is in the medialib first.*/
			if (xmms_medialib_entry_get (entry)) {
				xmms_object_unref (entry);
				continue;
			}

			XMMS_DBG ("Entry is : %s", xmms_playlist_entry_url_get (entry));

			transport = xmms_transport_new ();
			xmms_transport_open (transport, entry);
			if (!transport) {
				xmms_object_unref (entry);
				continue;
			}

			if (!xmms_transport_islocal (transport))
				continue;

			mime = xmms_transport_mimetype_get (transport);

			if (!mime) {
				xmms_object_unref (entry);
				xmms_object_unref (transport);
				continue;
			}

			plsplugin = xmms_playlist_plugin_new (mime);
			if (plsplugin) {

				/* This is a playlist file... */
				XMMS_DBG ("Playlist!!");
				xmms_playlist_plugin_read (plsplugin, mtt->playlist, transport);

				/* we don't want it in the playlist. */
				xmms_playlist_id_remove (mtt->playlist, xmms_playlist_entry_id_get (entry), &err);
				xmms_object_unref (entry);

				/* cleanup */
				xmms_playlist_plugin_free (plsplugin);
				xmms_object_unref (transport);
				continue;
			}


			xmms_playlist_entry_mimetype_set (entry, mime);
			decoder = xmms_decoder_new ();
			if (!xmms_decoder_open (decoder, transport)) {
				xmms_object_unref (entry);
				xmms_object_unref (transport);
				xmms_object_unref (decoder);
				continue;
			}

			xmms_decoder_mediainfo_get (decoder, transport);

			/* Store this in the database */
			xmms_medialib_entry_store (entry);

			xmms_object_unref (entry);
			xmms_object_unref (transport);
			xmms_object_unref (decoder);
				

			g_mutex_lock (mtt->mutex);

		}

	}

	g_mutex_unlock (mtt->mutex);

	return NULL;
}

