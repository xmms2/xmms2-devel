/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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
 *  Manages the asynchronous update of playlists' contents.
 *  Currently handles automatic removal/addition of entries in queue and
 *  pshuffle.
 */

#include <xmmspriv/xmms_playlist_updater.h>
#include <xmmspriv/xmms_thread_name.h>
#include <xmms/xmms_log.h>
#include <glib.h>

static void xmms_playlist_updater_destroy (xmms_object_t *object);
static void xmms_playlist_updater_start (xmms_playlist_updater_t *updater);
static void xmms_playlist_updater_stop (xmms_playlist_updater_t *updater);
static gpointer xmms_playlist_updater_loop (xmms_playlist_updater_t *updater);
static void xmms_playlist_updater_need_update (xmms_object_t *object, xmmsv_t *val, gpointer udata);
static gchar *xmms_playlist_updater_pop (xmms_playlist_updater_t *updater);

struct xmms_playlist_updater_St {
	xmms_object_t object;

	xmms_playlist_t *playlist;

	GThread *thread;
	GMutex mutex;
	GCond cond;

	gboolean keep_running;

	const gchar *updating;
	gboolean need_update;
	GList *stack;
};

xmms_playlist_updater_t *
xmms_playlist_updater_init (xmms_playlist_t *playlist)
{
	xmms_playlist_updater_t *updater;

	updater = xmms_object_new (xmms_playlist_updater_t,
	                           xmms_playlist_updater_destroy);

	g_cond_init (&updater->cond);
	g_mutex_init (&updater->mutex);

	xmms_object_ref (playlist);
	updater->playlist = playlist;

	updater->updating = NULL;
	updater->stack = NULL;

	xmms_object_connect (XMMS_OBJECT (playlist),
	                     XMMS_IPC_SIGNAL_COLLECTION_CHANGED,
	                     xmms_playlist_updater_need_update, updater);

	xmms_object_connect (XMMS_OBJECT (playlist),
	                     XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
	                     xmms_playlist_updater_need_update, updater);

	xmms_object_connect (XMMS_OBJECT (playlist),
	                     XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                     xmms_playlist_updater_need_update, updater);

	xmms_playlist_updater_start (updater);

	return updater;
}

static void
xmms_playlist_updater_destroy (xmms_object_t *object)
{
	GList *it;
	xmms_playlist_updater_t *updater = (xmms_playlist_updater_t *) object;

	g_return_if_fail (updater != NULL);

	XMMS_DBG ("Deactivating playlist updater object.");

	xmms_object_disconnect (XMMS_OBJECT (updater->playlist),
	                        XMMS_IPC_SIGNAL_COLLECTION_CHANGED,
	                        xmms_playlist_updater_need_update, updater);

	xmms_object_disconnect (XMMS_OBJECT (updater->playlist),
	                        XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
	                        xmms_playlist_updater_need_update, updater);

	xmms_object_disconnect (XMMS_OBJECT (updater->playlist),
	                        XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                        xmms_playlist_updater_need_update, updater);

	xmms_playlist_updater_stop (updater);

	xmms_object_unref (updater->playlist);

	for (it = updater->stack; it; it = g_list_next (it)) {
		gchar *plname;
		plname = (gchar *) it->data;
		g_free (plname);
	}
	g_list_free (updater->stack);

	g_mutex_clear (&updater->mutex);
	g_cond_clear (&updater->cond);
}

/**
 * Start the updater thread.
 */
static void
xmms_playlist_updater_start (xmms_playlist_updater_t *updater)
{
	g_return_if_fail (updater != NULL);
	g_return_if_fail (updater->thread == NULL);

	updater->keep_running = TRUE;
	updater->thread = g_thread_new ("x2 pls updater",
	                                (GThreadFunc) xmms_playlist_updater_loop,
	                                updater);
}

/**
 * Signal the updater loop to exit and join the updater thread.
 */
static void
xmms_playlist_updater_stop (xmms_playlist_updater_t *updater)
{
	g_return_if_fail (updater != NULL);
	g_return_if_fail (updater->thread != NULL);

	g_mutex_lock (&updater->mutex);

	updater->keep_running = FALSE;
	g_cond_signal (&updater->cond);

	g_mutex_unlock (&updater->mutex);

	g_thread_join (updater->thread);

	updater->thread = NULL;
}

/**
 * Wait until a playlist needs update.
 * @internal
 */
static gpointer
xmms_playlist_updater_loop (xmms_playlist_updater_t *updater)
{
	gchar *plname;

	g_mutex_lock (&updater->mutex);

	while (updater->keep_running) {
		if (!updater->stack) {
			g_cond_wait (&updater->cond, &updater->mutex);
		} else {
			plname = xmms_playlist_updater_pop (updater);
			g_mutex_unlock (&updater->mutex);
			xmms_playlist_update (updater->playlist, plname);
			g_mutex_lock (&updater->mutex);
			g_free (plname);
		}
	}

	g_mutex_unlock (&updater->mutex);

	return NULL;
}

/**
 * Signals callback
 */
static void
xmms_playlist_updater_need_update (xmms_object_t *object, xmmsv_t *val,
                                   gpointer udata)
{
	xmms_playlist_updater_t *updater = (xmms_playlist_updater_t *) udata;
	const gchar *plname, *ns;

	if (xmmsv_dict_entry_get_string (val, "namespace", &ns)) {
		if (g_strcmp0 (ns, XMMS_COLLECTION_NS_PLAYLISTS) != 0) {
			return;
		}
	}

	if (!xmmsv_dict_entry_get_string (val, "name", &plname)) {
		return;
	}

	xmms_playlist_updater_push (updater, plname);
}

static gchar *
xmms_playlist_updater_pop (xmms_playlist_updater_t *updater)
{
	GList *head;
	gchar *plname;

	head = updater->stack;
	plname = (gchar *) head->data;
	updater->stack = g_list_delete_link (updater->stack, head);

	return plname;
}

/**
 * Queue a playlist for update.
 */
void
xmms_playlist_updater_push (xmms_playlist_updater_t *updater,
                            const gchar *plname)
{
	g_return_if_fail (updater);
	g_return_if_fail (plname);

	g_mutex_lock (&updater->mutex);

	/* don't schedule the playlist if it's already scheduled */
	if (!g_list_find_custom (updater->stack, plname, (GCompareFunc) g_strcmp0)) {
		updater->stack = g_list_prepend (updater->stack, g_strdup (plname));
		g_cond_signal (&updater->cond);
	}

	g_mutex_unlock (&updater->mutex);
}
