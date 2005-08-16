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

#include "xmms/xmms_defs.h"
#include "xmms/xmms_log.h"
#include "xmmspriv/xmms_mediainfo.h"
#include "xmmspriv/xmms_transport.h"
#include "xmmspriv/xmms_decoder.h"
#include "xmmspriv/xmms_playlist.h"
#include "xmmspriv/xmms_medialib.h"


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

	mir->running = FALSE;
	g_cond_signal (mir->cond);

	g_mutex_unlock (mir->mutex);

	g_thread_join (mir->thread);

	g_cond_free (mir->cond);
	g_mutex_free (mir->mutex);
	g_free (mir);
}

/**
 * Wake the reader thread and start process the entries.
 */

void
xmms_mediainfo_reader_wakeup (xmms_mediainfo_reader_t *mr)
{
	g_return_if_fail (mr);

	g_mutex_lock (mr->mutex);
	g_cond_signal (mr->cond);
	g_mutex_unlock (mr->mutex);
}

/** @} */

static void
xmms_mediainfo_playlist_changed_cb (xmms_object_t *object, gconstpointer arg, gpointer userdata)
{
	xmms_mediainfo_reader_t *mir = userdata;
	const xmms_object_cmd_arg_t *oarg = arg;
	GHashTable *chmsg = oarg->retval->value.dict;

	xmms_object_cmd_value_t *val = g_hash_table_lookup (chmsg, "type");

	if (!val)
		return;

	if (val->value.uint32 == XMMS_PLAYLIST_CHANGED_ADD) {
		xmms_mediainfo_reader_wakeup (mir);
	}
}

static gpointer
xmms_mediainfo_reader_thread (gpointer data)
{
	xmms_mediainfo_reader_t *mrt = (xmms_mediainfo_reader_t *) data;

	while (mrt->running) {
		xmms_medialib_entry_t entry;

		while ((entry = xmms_medialib_entry_not_resolved_get())) {
			xmms_transport_t *transport;
			xmms_decoder_t *decoder;
			xmms_error_t err;
			guint lmod = 0;

			xmms_error_reset (&err);
			lmod = xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD);

			transport = xmms_transport_new ();
			if (!transport) {
				continue;
			}

			if (!xmms_transport_open (transport, entry)) {
				xmms_medialib_entry_remove (entry);
				xmms_playlist_remove_by_entry (mrt->playlist, entry);
				xmms_object_unref (transport);
				continue;
			}

			xmms_transport_start (transport);

			if (lmod) {
				guint tmp;
				tmp = xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD);
				if (lmod >= tmp) {
					xmms_medialib_entry_property_set_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED, 1);
					xmms_transport_stop (transport);
					xmms_object_unref (transport);
					continue;
				}
				XMMS_DBG ("Modified on disk!");

			}

			decoder = xmms_decoder_new ();
			if (!xmms_decoder_open (decoder, transport)) {
				xmms_medialib_entry_remove (entry);
				xmms_playlist_remove_by_entry (mrt->playlist, entry);
				xmms_transport_stop (transport);
				xmms_object_unref (transport);
				xmms_object_unref (decoder);
				continue;
			}

			if (xmms_decoder_init_for_mediainfo (decoder)) {
				xmms_decoder_mediainfo_get (decoder, transport);
			}

			xmms_medialib_entry_property_set_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED, 1);

			xmms_transport_stop (transport);
			xmms_object_unref (transport);
			xmms_object_unref (decoder);

		}

		g_mutex_lock (mrt->mutex);
		g_cond_wait (mrt->cond, mrt->mutex);
		g_mutex_unlock (mrt->mutex);

	}

	return NULL;
}
