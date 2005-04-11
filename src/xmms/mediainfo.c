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
 * This file controls the mediainfo reader thread.
 *
 */

#include <stdlib.h>

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

/** @defgroup MediaInfoReader MediaInfoReader
  * @ingroup XMMSServer
  * @brief The mediainfo reader.
  *
  * When a item is added to the playlist the mediainfo reader will
  * start extracting the information from this entry and update it
  * if additional information is found.
  * @{
  */

struct xmms_mediainfo_reader_St {
	GThread *thread;
	GMutex *mutex;
	GCond *cond;

	gboolean running;
	GQueue *queue;
	xmms_playlist_t *playlist;
};

static gpointer xmms_mediainfo_reader_thread (gpointer data);
static void xmms_mediainfo_playlist_changed_cb (xmms_object_t *object, gconstpointer arg, gpointer userdata);


/**
  * Start a new mediainfo reader thread
  */

xmms_mediainfo_reader_t *
xmms_mediainfo_reader_start (xmms_playlist_t *playlist)
{
	xmms_mediainfo_reader_t *mrt;

	g_return_val_if_fail (playlist, NULL);

	mrt = g_new0 (xmms_mediainfo_reader_t, 1);

	mrt->mutex = g_mutex_new ();
	mrt->cond = g_cond_new ();
	mrt->playlist = playlist;
	mrt->queue = g_queue_new ();
	mrt->running = TRUE;
	mrt->thread = g_thread_create (xmms_mediainfo_reader_thread, mrt, TRUE, NULL);

	xmms_object_connect (XMMS_OBJECT (playlist), XMMS_IPC_SIGNAL_PLAYLIST_CHANGED, xmms_mediainfo_playlist_changed_cb, mrt);
	

	return mrt;
}

/**
  * Kill the mediainfo reader thread
  */

void
xmms_mediainfo_reader_stop (xmms_mediainfo_reader_t *mir)
{
	g_mutex_lock (mir->mutex);

	while (g_queue_pop_head (mir->queue))
		;
	mir->running = FALSE;
	g_cond_signal (mir->cond);

	g_mutex_unlock (mir->mutex);

	g_thread_join (mir->thread);

	g_queue_free (mir->queue);
	g_cond_free (mir->cond);
	g_mutex_free (mir->mutex);

	g_free (mir);
}

void
xmms_mediainfo_entry_add (xmms_mediainfo_reader_t *mr, xmms_medialib_entry_t entry)
{
	g_return_if_fail (mr);
	g_return_if_fail (entry);

	g_mutex_lock (mr->mutex);
	g_queue_push_tail (mr->queue, GUINT_TO_POINTER (entry));
	g_cond_signal (mr->cond);
	g_mutex_unlock (mr->mutex);
}

/** @} */

static void
xmms_mediainfo_playlist_changed_cb (xmms_object_t *object, gconstpointer arg, gpointer userdata)
{
	xmms_mediainfo_reader_t *mir = userdata;
	const xmms_object_cmd_arg_t *oarg = arg;
	xmms_playlist_changed_msg_t *chmsg = oarg->retval.plch;

	if (chmsg->type == XMMS_PLAYLIST_CHANGED_ADD) {
		xmms_medialib_entry_t entry = chmsg->id;

		xmms_mediainfo_entry_add (mir, entry);
	}
}

static gpointer
xmms_mediainfo_reader_thread (gpointer data)
{
	xmms_mediainfo_reader_t *mrt = (xmms_mediainfo_reader_t *) data;

	g_mutex_lock (mrt->mutex);

	while (mrt->running) {
		xmms_medialib_entry_t entry;

		XMMS_DBG ("MediainfoThread is idle.");
		g_cond_wait (mrt->cond, mrt->mutex);
		XMMS_DBG ("MediainfoThread is awake!");

		while ((entry = GPOINTER_TO_UINT (g_queue_pop_head (mrt->queue)))) {
			xmms_transport_t *transport;
			xmms_decoder_t *decoder;
			xmms_error_t err;
			const gchar *mime;
			guint lmod = 0;

			xmms_error_reset (&err);

			g_mutex_unlock (mrt->mutex);

			if (xmms_medialib_entry_is_resolved (entry)) {
				lmod = xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD);
			}

			transport = xmms_transport_new ();
			if (!transport) {
				goto cont;
			}

			if (!xmms_transport_open (transport, entry)) {
				xmms_object_unref (transport);
				goto cont;
			}

			if (lmod) {
				guint tmp;
				tmp = xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD);
				if (tmp && lmod >= tmp) {
					xmms_object_unref (transport);
					goto cont;
				}
			}

			mime = xmms_transport_mimetype_get_wait (transport);

			if (!mime) {
				xmms_object_unref (transport);
				goto cont;
			}

			/** @todo enable playlists
			if (xmms_medialib_entry_is_playlist (entry)) {
				plsplugin = xmms_playlist_plugin_new (mime);

				XMMS_DBG ("Playlist!!");
				
				xmms_playlist_plugin_read (plsplugin, mrt->playlist, transport);

				xmms_playlist_plugin_free (plsplugin);
				xmms_object_unref (transport);
				goto cont;
			}
			*/

			xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_MIME, mime);
			decoder = xmms_decoder_new ();
			if (!xmms_decoder_open (decoder, transport)) {
				xmms_object_unref (transport);
				xmms_object_unref (decoder);
				goto cont;
			}

			xmms_decoder_mediainfo_get (decoder, transport);

			xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED, "1");

			xmms_object_unref (transport);
			xmms_object_unref (decoder);

cont:
			g_mutex_lock (mrt->mutex);

		}

	}

	g_mutex_unlock (mrt->mutex);

	return NULL;
}
