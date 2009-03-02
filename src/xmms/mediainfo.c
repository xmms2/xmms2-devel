/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#include "xmms/xmms_log.h"
#include "xmms/xmms_ipc.h"
#include "xmmspriv/xmms_mediainfo.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmmspriv/xmms_xform.h"


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
};

static void xmms_mediainfo_reader_stop (xmms_object_t *o);
static gpointer xmms_mediainfo_reader_thread (gpointer data);


/**
  * Start a new mediainfo reader thread
  */

xmms_mediainfo_reader_t *
xmms_mediainfo_reader_start (void)
{
	xmms_mediainfo_reader_t *mrt;

	mrt = xmms_object_new (xmms_mediainfo_reader_t,
	                       xmms_mediainfo_reader_stop);

	xmms_ipc_object_register (XMMS_IPC_OBJECT_MEDIAINFO_READER,
	                          XMMS_OBJECT (mrt));

	xmms_ipc_broadcast_register (XMMS_OBJECT (mrt),
	                             XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS);
	xmms_ipc_signal_register (XMMS_OBJECT (mrt),
	                          XMMS_IPC_SIGNAL_MEDIAINFO_READER_UNINDEXED);

	mrt->mutex = g_mutex_new ();
	mrt->cond = g_cond_new ();
	mrt->running = TRUE;
	mrt->thread = g_thread_create (xmms_mediainfo_reader_thread, mrt, TRUE, NULL);

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

static gpointer
xmms_mediainfo_reader_thread (gpointer data)
{
	GList *goal_format;
	GTimeVal timeval;
	xmms_stream_type_t *f;
	guint num = 0;

	xmms_mediainfo_reader_t *mrt = (xmms_mediainfo_reader_t *) data;

	xmms_object_emit_f (XMMS_OBJECT (mrt),
	                    XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS,
	                    XMMSV_TYPE_INT32,
	                    XMMS_MEDIAINFO_READER_STATUS_RUNNING);


	f = _xmms_stream_type_new (NULL,
	                           XMMS_STREAM_TYPE_MIMETYPE,
	                           "audio/pcm",
	                           XMMS_STREAM_TYPE_END);
	goal_format = g_list_prepend (NULL, f);

	while (mrt->running) {
		xmms_medialib_session_t *session;
		xmmsc_medialib_entry_status_t prev_status;
		guint lmod = 0;
		xmms_medialib_entry_t entry;
		xmms_xform_t *xform;

		session = xmms_medialib_begin_write ();
		entry = xmms_medialib_entry_not_resolved_get (session);
		XMMS_DBG ("got %d as not resolved", entry);

		if (!entry) {
			xmms_medialib_end (session);

			xmms_object_emit_f (XMMS_OBJECT (mrt),
			                    XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS,
			                    XMMSV_TYPE_INT32,
			                    XMMS_MEDIAINFO_READER_STATUS_IDLE);

			g_mutex_lock (mrt->mutex);
			g_cond_wait (mrt->cond, mrt->mutex);
			g_mutex_unlock (mrt->mutex);

			num = 0;

			xmms_object_emit_f (XMMS_OBJECT (mrt),
			                    XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS,
			                    XMMSV_TYPE_INT32,
			                    XMMS_MEDIAINFO_READER_STATUS_RUNNING);
			continue;
		}

		prev_status = xmms_medialib_entry_property_get_int (session, entry, XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS);
		xmms_medialib_entry_status_set (session, entry, XMMS_MEDIALIB_ENTRY_STATUS_RESOLVING);

		lmod = xmms_medialib_entry_property_get_int (session, entry, XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD);

		if (num == 0) {
			xmms_object_emit_f (XMMS_OBJECT (mrt),
			                    XMMS_IPC_SIGNAL_MEDIAINFO_READER_UNINDEXED,
			                    XMMSV_TYPE_INT32,
			                    xmms_medialib_num_not_resolved (session));
			num = 10;
		} else {
			num--;
		}

		xmms_medialib_end (session);
		xform = xmms_xform_chain_setup (entry, goal_format, TRUE);

		if (!xform) {
			if (prev_status == XMMS_MEDIALIB_ENTRY_STATUS_NEW) {
				xmms_medialib_entry_remove (entry);
			} else {
				session = xmms_medialib_begin_write ();
				xmms_medialib_entry_status_set (session, entry, XMMS_MEDIALIB_ENTRY_STATUS_NOT_AVAILABLE);
				xmms_medialib_end (session);
				xmms_medialib_entry_send_update (entry);
			}
			continue;
		}

		xmms_object_unref (xform);
		g_get_current_time (&timeval);

		session = xmms_medialib_begin_write ();
		xmms_medialib_entry_status_set (session, entry, XMMS_MEDIALIB_ENTRY_STATUS_OK);
		xmms_medialib_entry_property_set_int (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_ADDED,
		                                      timeval.tv_sec);
		xmms_medialib_end (session);
		xmms_medialib_entry_send_update (entry);

	}

	return NULL;
}
