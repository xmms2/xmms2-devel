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

/** @file Control for the mediainfo thread */

/** @defgroup MediaInfoThread MediaInfoThread
  * @ingroup XMMSServer
  * @brief The mediainfo thread.
  *
  * When a item is added to the playlist the mediainfo thread will
  * start extracting the information from this entry and update it
  * if additional information is found.
  * @{
  */

struct xmms_mediainfo_thread_St {
	GThread *thread;
	GMutex *mutex;
	GCond *cond;

	gboolean running;
	GQueue *queue;
	xmms_playlist_t *playlist;
};

static gpointer xmms_mediainfo_thread_thread (gpointer data);
static void xmms_mediainfo_playlist_changed_cb (xmms_object_t *object, gconstpointer arg, gpointer userdata);


/**
  * Start a new mediainfo thread
  */

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

	xmms_object_connect (XMMS_OBJECT (playlist), XMMS_IPC_SIGNAL_PLAYLIST_CHANGED, xmms_mediainfo_playlist_changed_cb, mtt);
	

	return mtt;
}

/**
  * Kill the mediainfo thread
  */

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

void
xmms_mediainfo_entry_add (xmms_mediainfo_thread_t *mt, xmms_playlist_entry_t *entry)
{
	g_return_if_fail (mt);
	g_return_if_fail (entry);

	g_mutex_lock (mt->mutex);
	xmms_object_ref (XMMS_OBJECT (entry));
	g_queue_push_tail (mt->queue, entry);
	g_cond_signal (mt->cond);
	g_mutex_unlock (mt->mutex);
}

/** @} */

static void
xmms_mediainfo_playlist_changed_cb (xmms_object_t *object, gconstpointer arg, gpointer userdata)
{
	xmms_mediainfo_thread_t *mit = userdata;
	const xmms_object_cmd_arg_t *oarg = arg;
	xmms_playlist_changed_msg_t *chmsg = oarg->retval.plch;

	if (chmsg->type == XMMS_PLAYLIST_CHANGED_ADD) {
		xmms_playlist_entry_t *entry;

		entry = xmms_playlist_get_byid (mit->playlist, chmsg->id);
		if (!entry)
			return;

		xmms_mediainfo_entry_add (mit, entry);

		xmms_object_unref (XMMS_OBJECT (entry));
		
	}
}

static gpointer
xmms_mediainfo_thread_thread (gpointer data)
{
	xmms_mediainfo_thread_t *mtt = (xmms_mediainfo_thread_t *) data;

	g_mutex_lock (mtt->mutex);

	while (mtt->running) {
		xmms_playlist_entry_t *entry;

		XMMS_DBG ("MediainfoThread is idle.");
		g_cond_wait (mtt->cond, mtt->mutex);
		XMMS_DBG ("MediainfoThread is awake!");

		while ((entry = g_queue_pop_head (mtt->queue))) {
			xmms_transport_t *transport;
			xmms_playlist_plugin_t *plsplugin;
			xmms_decoder_t *decoder;
			xmms_error_t err;
			const gchar *mime;
			guint lmod = 0;

			xmms_error_reset (&err);

			g_mutex_unlock (mtt->mutex);

			/* Check if this is in the medialib first.*/
			if (xmms_medialib_entry_get (entry)) {
				const gchar *tmp;

				xmms_playlist_entry_changed (entry);

				tmp = xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_LMOD);
				if (tmp) {
					lmod = atoi (tmp);
				}
			}

			transport = xmms_transport_new ();
			if (!transport || !xmms_transport_open (transport, entry)) {
				goto cont;
			}


			if (lmod) {
				const gchar *tmp;
				tmp = xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_LMOD);
				if (tmp && lmod >= atoi (tmp)) {
					xmms_object_unref (transport);

					goto cont;
				}
			}

			mime = xmms_transport_mimetype_get_wait (transport);

			if (!mime) {
				xmms_object_unref (transport);
				goto cont;
			}

			plsplugin = xmms_playlist_plugin_new (mime);
			if (plsplugin) {

				/** @todo. playlist_plugin_read should return a list
				  of entries. Then we could submit them to the 
				  mediainfo thread again. now we cant...
				 */
				if (!xmms_playlist_entry_id_get (entry))
					goto cont;

				/* This is a playlist file... */
				XMMS_DBG ("Playlist!!");
				
				xmms_playlist_plugin_read (plsplugin, mtt->playlist, transport);

				/* we don't want it in the playlist. */
				xmms_playlist_id_remove (mtt->playlist, xmms_playlist_entry_id_get (entry), &err);

				/* cleanup */
				xmms_playlist_plugin_free (plsplugin);
				xmms_object_unref (transport);
				goto cont;
			}

			if (!xmms_transport_islocal (transport)) {
				xmms_object_unref (transport);
				goto cont;
			}

			xmms_playlist_entry_mimetype_set (entry, mime);
			decoder = xmms_decoder_new ();
			if (!xmms_decoder_open (decoder, transport)) {
				xmms_object_unref (transport);
				xmms_object_unref (decoder);
				goto cont;
			}

			xmms_decoder_mediainfo_get (decoder, transport);
			xmms_playlist_entry_property_set (entry,
							  XMMS_PLAYLIST_ENTRY_PROPERTY_RESOLVED,
							  "1");
			xmms_playlist_entry_changed (entry);

			/* Store this in the database */
			xmms_medialib_entry_store (entry);

			xmms_object_unref (transport);
			xmms_object_unref (decoder);

cont:
			xmms_object_unref (entry);
			g_mutex_lock (mtt->mutex);

		}

	}

	g_mutex_unlock (mtt->mutex);

	return NULL;
}
