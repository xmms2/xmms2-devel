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
#include "xmms/xmms_ipc.h"
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
	xmms_object_t object;

	GThread *thread;
	GMutex *mutex;
	GCond *cond;

	gboolean running;
	xmms_playlist_t *playlist;
};

static void xmms_mediainfo_reader_stop (xmms_object_t *o);
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

	mrt = xmms_object_new (xmms_mediainfo_reader_t, xmms_mediainfo_reader_stop);

	xmms_ipc_object_register (XMMS_IPC_OBJECT_MEDIAINFO_READER, 
							  XMMS_OBJECT (mrt));

	xmms_ipc_broadcast_register (XMMS_OBJECT (mrt),
	                             XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS);
	xmms_ipc_signal_register (XMMS_OBJECT (mrt),
							  XMMS_IPC_SIGNAL_MEDIAINFO_READER_UNINDEXED);

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

static void
xmms_mediainfo_reader_stop (xmms_object_t *o)
{
	xmms_mediainfo_reader_t *mir = (xmms_mediainfo_reader_t *) o;
		
	g_mutex_lock (mir->mutex);
	mir->running = FALSE;
	g_cond_signal (mir->cond);
	g_mutex_unlock (mir->mutex);

	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS);
	xmms_ipc_signal_unregister (XMMS_IPC_SIGNAL_MEDIAINFO_READER_UNINDEXED);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_MEDIAINFO_READER);

	g_thread_join (mir->thread);

	g_cond_free (mir->cond);
	g_mutex_free (mir->mutex);
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

	if (val->value.uint32 == XMMS_PLAYLIST_CHANGED_ADD
		|| val->value.uint32 == XMMS_PLAYLIST_CHANGED_INSERT) {
		xmms_mediainfo_reader_wakeup (mir);
	}
}

static gpointer
xmms_mediainfo_reader_thread (gpointer data)
{
	xmms_mediainfo_reader_t *mrt = (xmms_mediainfo_reader_t *) data;

	xmms_object_emit_f (XMMS_OBJECT (mrt),
	                    XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS,
	                    XMMS_OBJECT_CMD_ARG_INT32,
	                    XMMS_MEDIAINFO_READER_STATUS_RUNNING);
	
	while (mrt->running) {
		xmms_transport_t *transport;
		xmms_decoder_t *decoder;
		guint lmod = 0;
		xmms_medialib_entry_t entry;
		xmms_medialib_session_t *session;
		
		session = xmms_medialib_begin ();
		entry = xmms_medialib_entry_not_resolved_get (session);
		
		if (!entry) {
			xmms_medialib_end (session);
			
			xmms_object_emit_f (XMMS_OBJECT (mrt),
			                    XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS,
			                    XMMS_OBJECT_CMD_ARG_INT32,
			                    XMMS_MEDIAINFO_READER_STATUS_IDLE);
			
			g_mutex_lock (mrt->mutex);
			g_cond_wait (mrt->cond, mrt->mutex);
			g_mutex_unlock (mrt->mutex);
			
			xmms_object_emit_f (XMMS_OBJECT (mrt),
					    XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS,
					    XMMS_OBJECT_CMD_ARG_INT32,
					    XMMS_MEDIAINFO_READER_STATUS_RUNNING);
			continue;
		}
		
		lmod = xmms_medialib_entry_property_get_int (session, entry, XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD);
		xmms_object_emit_f (XMMS_OBJECT (mrt),
		                    XMMS_IPC_SIGNAL_MEDIAINFO_READER_UNINDEXED,
		                    XMMS_OBJECT_CMD_ARG_UINT32,
		                    xmms_medialib_num_not_resolved (session));
		xmms_medialib_end (session);
		
		transport = xmms_transport_new ();
		if (!transport) {
			continue;
		}
		
		if (!xmms_transport_open (transport, entry)) {
			session = xmms_medialib_begin_write ();
			xmms_medialib_entry_remove (session, entry);
			xmms_medialib_end (session);
			
			xmms_playlist_remove_by_entry (mrt->playlist, entry);
			xmms_object_unref (transport);
			continue;
		}
		
		xmms_transport_start (transport);
		
		if (lmod) {
			guint tmp;
			session = xmms_medialib_begin_write ();
			tmp = xmms_medialib_entry_property_get_int (session, entry, 
														XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD);
			if (lmod >= tmp) {
				xmms_medialib_entry_property_set_int (session, entry, 
													  XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED, 1);
				xmms_medialib_end (session);
				xmms_transport_stop (transport);
				xmms_object_unref (transport);
				continue;
			}
			xmms_medialib_end (session);
			XMMS_DBG ("Modified on disk!");
		}

		decoder = xmms_decoder_new ();
		if (!xmms_decoder_open (decoder, transport)) {
			session = xmms_medialib_begin_write ();
			xmms_medialib_entry_remove (session, entry);
			xmms_medialib_end (session);
			
			xmms_playlist_remove_by_entry (mrt->playlist, entry);
			xmms_transport_stop (transport);
			xmms_object_unref (transport);
			xmms_object_unref (decoder);
			
			continue;
		}
		
		if (xmms_decoder_init_for_mediainfo (decoder)) {
			if (lmod) {
				session = xmms_medialib_begin_write ();
				xmms_medialib_entry_cleanup (session, entry);
				xmms_medialib_end (session);
			}
			
			xmms_decoder_mediainfo_get (decoder, transport);
		}
	
		session = xmms_medialib_begin_write ();
		xmms_medialib_entry_property_set_int (session, entry, 
											  XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED, 1);
		xmms_medialib_entry_property_set_int (session, entry, 
											  XMMS_MEDIALIB_ENTRY_PROPERTY_ADDED, time(NULL));
		xmms_medialib_end (session);
		
		xmms_transport_stop (transport);
		xmms_object_unref (transport);
		xmms_object_unref (decoder);
		
	}
	
	return NULL;
}
