/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 */

#include <glib.h>
#include <glib/gi18n.h>

#include <xmmsclient/xmmsclient.h>

#include "cli_context.h"
#include "command.h"
#include "commands.h"
#include "configuration.h"
#include "currently_playing.h"
#include "status.h"
#include "utils.h"
#include "xmmscall.h"

gboolean
cli_play (cli_context_t *ctx, command_t *cmd)
{
	XMMS_CALL (xmmsc_playback_start, cli_context_xmms_sync (ctx));
	return FALSE;
}

gboolean
cli_pause (cli_context_t *ctx, command_t *cmd)
{
	XMMS_CALL (xmmsc_playback_pause, cli_context_xmms_sync (ctx));
	return FALSE;
}

gboolean
cli_toggle (cli_context_t *ctx, command_t *cmd)
{
	guint status = cli_context_playback_status (ctx);

	if (status == XMMS_PLAYBACK_STATUS_PLAY) {
		XMMS_CALL (xmmsc_playback_pause, cli_context_xmms_sync (ctx));
	} else {
		XMMS_CALL (xmmsc_playback_start, cli_context_xmms_sync (ctx));
	}
	return FALSE;
}

gboolean
cli_stop (cli_context_t *ctx, command_t *cmd)
{
	XMMS_CALL (xmmsc_playback_stop, cli_context_xmms_sync (ctx));
	return FALSE;
}

gboolean
cli_seek (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	command_arg_time_t t;

	if (!command_arg_time_get (cmd, 0, &t)) {
		g_printf (_("Error: failed to parse the time argument!\n"));
		return FALSE;
	}

	if (t.type == COMMAND_ARG_TIME_OFFSET) {
		XMMS_CALL (xmmsc_playback_seek_ms, conn, t.value.offset * 1000, XMMS_PLAYBACK_SEEK_CUR);
	} else {
		XMMS_CALL (xmmsc_playback_seek_ms, conn, t.value.pos * 1000, XMMS_PLAYBACK_SEEK_SET);
	}

	return FALSE;
}

gboolean
cli_current (cli_context_t *ctx, command_t *cmd)
{
	status_entry_t *status;
	const gchar *format;
	gint refresh;

	if (!command_flag_int_get (cmd, "refresh", &refresh)) {
		refresh = 0;
	}

	if (!command_flag_string_get (cmd, "format", &format)) {
		configuration_t *config = cli_context_config (ctx);
		format = configuration_get_string (config, "STATUS_FORMAT");
	}

	status = currently_playing_init (ctx, format, refresh);

	if (refresh > 0) {
		cli_context_status_mode (ctx, status);
	} else {
		status_refresh (status, TRUE, TRUE);
		status_free (status);
	}

	return refresh != 0; /* need I/O if we are refreshing */
}

gboolean
cli_prev (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	gint n;
	gint offset = -1;

	if (command_arg_int_get (cmd, 0, &n)) {
		offset = -n;
	}

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_set_next_rel, conn, offset),
	                 XMMS_CALL_P (xmmsc_playback_tickle, conn));

	return FALSE;
}

gboolean
cli_next (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	gint offset = 1;
	gint n;

	if (command_arg_int_get (cmd, 0, &n)) {
		offset = n;
	}

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_set_next_rel, conn, offset),
	                 XMMS_CALL_P (xmmsc_playback_tickle, conn));

	return FALSE;
}

static void
cli_jump_relative (cli_context_t *ctx, gint inc, xmmsv_t *value)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	xmmsv_list_iter_t *it;
	gint i, plid, id, currpos, plsize;
	xmmsv_t *playlist;

	currpos = cli_context_current_position (ctx);
	playlist = cli_context_active_playlist (ctx);
	plsize = xmmsv_list_get_size (playlist);

	/* If no currpos, start jump from beginning */
	if (currpos < 0) {
		currpos = 0;
	}

	/* magic trick so we can loop in either direction */
	inc += plsize;

	xmmsv_get_list_iter (value, &it);

	/* Loop on the playlist */
	for (i = (currpos + inc) % plsize; i != currpos; i = (i + inc) % plsize) {
		xmmsv_list_iter_first (it);

		xmmsv_list_get_int (playlist, i, &plid);

		/* Loop on the matched media */
		while (xmmsv_list_iter_entry_int (it, &id)) {
			/* If both match, jump! */
			if (plid == id) {
				XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_set_next, conn, i),
				                 XMMS_CALL_P (xmmsc_playback_tickle, conn));
				return;
			}
			xmmsv_list_iter_next (it);
		}
	}

	/* No matching media found, don't jump */
	g_printf (_("No media matching the pattern in the playlist!\n"));
}

gboolean
cli_jump (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	xmmsv_t *query;
	gboolean backward = FALSE;
	playlist_positions_t *positions;

	command_flag_boolean_get (cmd, "backward", &backward);

	/* Select by positions */
	if (command_arg_positions_get (cmd, 0, &positions, cli_context_current_position (ctx))) {
		gint pos;
		if (playlist_positions_get_single (positions, &pos)) {
			XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_set_next, conn, pos),
			                 XMMS_CALL_P (xmmsc_playback_tickle, conn));
		} else {
			g_printf (_("Cannot jump to several positions!\n"));
		}
		playlist_positions_free (positions);
		/* Select by pattern */
	} else if (command_arg_pattern_get (cmd, 0, &query, TRUE)) {
		query = xmmsv_coll_intersect_with_playlist (query, XMMS_ACTIVE_PLAYLIST);
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_ids, conn, query, NULL, 0, 0),
		                 FUNC_CALL_P (cli_jump_relative, ctx, (backward ? -1 : 1), XMMS_PREV_VALUE));
		xmmsv_unref (query);
	}

	return FALSE;
}
