/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include "callbacks.h"

#include "cli_infos.h"
#include "column_display.h"


/* Dumps a propdict on stdout */
static void
propdict_dump (const void *key, xmmsc_result_value_type_t type,
               const void *value, const char *source, void *udata)
{
	switch (type) {
	case XMMSC_RESULT_VALUE_TYPE_UINT32:
		g_printf (_("[%s] %s = %u\n"), source, key, (uint32_t*) value);
		break;
	case XMMSC_RESULT_VALUE_TYPE_INT32:
		g_printf (_("[%s] %s = %d\n"), source, key, (int32_t*) value);
		break;
	case XMMSC_RESULT_VALUE_TYPE_STRING:
		g_printf (_("[%s] %s = %s\n"), source, key, (gchar*) value);
		break;
	case XMMSC_RESULT_VALUE_TYPE_DICT:
		g_printf (_("[%s] %s = <dict>\n"), source, key);
		break;
	case XMMSC_RESULT_VALUE_TYPE_PROPDICT:
		g_printf (_("[%s] %s = <propdict>\n"), source, key);
		break;
	case XMMSC_RESULT_VALUE_TYPE_COLL:
		g_printf (_("[%s] %s = <coll>\n"), source, key);
		break;
	case XMMSC_RESULT_VALUE_TYPE_BIN:
		g_printf (_("[%s] %s = <bin>\n"), source, key);
		break;
	case XMMSC_RESULT_VALUE_TYPE_NONE:
		g_printf (_("[%s] %s = <unknown>\n"), source, key);
		break;
	}
}


/* Dummy callback that resets the action status as finished. */
void
cb_done (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	cli_infos_loop_resume (infos);

	xmmsc_result_unref (res);
}

void
cb_coldisp_finalize (xmmsc_result_t *res, void *udata)
{
	column_display_t *coldisp = (column_display_t *) udata;
	column_display_print_footer (coldisp);
	column_display_free (coldisp);
}

void
cb_tickle (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	xmmsc_result_t *res2;

	if (!xmmsc_result_iserror (res)) {
		res2 = xmmsc_playback_tickle (infos->conn);
		xmmsc_result_unref (res2);
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (res);
}

void
cb_id_print_info (xmmsc_result_t *res, void *udata)
{
	guint id = GPOINTER_TO_UINT(udata);

	if (!xmmsc_result_iserror (res)) {
		g_printf (_("<mid=%u>\n"), id);
		xmmsc_result_propdict_foreach (res, propdict_dump, NULL);
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

void
cb_list_print_info (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	xmmsc_result_t *infores = NULL;
	guint id;

	if (!xmmsc_result_iserror (res)) {
		while (xmmsc_result_list_valid (res)) {
			if (infores) {
				xmmsc_result_unref (infores); /* unref previous infores */
			}
			if (xmmsc_result_get_uint (res, &id)) {
				infores = xmmsc_medialib_get_info (infos->conn, id);
				xmmsc_result_notifier_set (infores, cb_id_print_info,
				                           GUINT_TO_POINTER(id));
			}
			xmmsc_result_list_next (res);
		}

	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	if (!infores) {
		/* Done after the last callback */
		xmmsc_result_notifier_set (infores, cb_done, infos);
		xmmsc_result_unref (infores);
	} else {
		/* No resume-callback pending, we're done */
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
cb_id_print_row (xmmsc_result_t *res, void *udata)
{
	column_display_t *coldisp = (column_display_t *) udata;
	column_display_print (coldisp, res);
	xmmsc_result_unref (res);
}

void
cb_list_print_row (xmmsc_result_t *res, void *udata)
{
	/* FIXME: w00t at code copy-paste, please modularize */
	column_display_t *coldisp = (column_display_t *) udata;
	cli_infos_t *infos = column_display_infos_get (coldisp);
	xmmsc_result_t *infores = NULL;
	guint id;
	gint i = 0;

	if (!xmmsc_result_iserror (res)) {
		column_display_prepare (coldisp);
		column_display_print_header (coldisp);
		while (xmmsc_result_list_valid (res)) {
			if (infores) {
				xmmsc_result_unref (infores); /* unref previous infores */
			}
			if (xmmsc_result_get_uint (res, &id)) {
				infores = xmmsc_medialib_get_info (infos->conn, id);
				xmmsc_result_notifier_set (infores, cb_id_print_row, coldisp);
			}
			xmmsc_result_list_next (res);
			i++;
		}

	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	if (infores) {
		/* Done after the last callback */
		xmmsc_result_notifier_set (infores, cb_coldisp_finalize, coldisp);
		xmmsc_result_notifier_set (infores, cb_done, infos);
		xmmsc_result_unref (infores);
	} else {
		/* No resume-callback pending, we're done */
		column_display_print_footer (coldisp);
		column_display_free (coldisp);
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
cb_list_jump (xmmsc_result_t *res, void *udata)
{
	/* FIXME: Jump to first matching result in playlist */
}
