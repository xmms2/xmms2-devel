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

#include "utils.h"

#include "cli_infos.h"
#include "cli_cache.h"
#include "column_display.h"

static void coll_int_attribute_set (xmmsc_coll_t *coll, const char *key, gint value);
static xmmsc_coll_t *coll_make_reference (const char *name, xmmsc_coll_namespace_t ns);
static void coll_copy_attributes (const char *key, const char *value, void *udata);
static xmmsc_coll_t *coll_copy_retype (xmmsc_coll_t *coll, xmmsc_coll_type_t type);
static void coll_print_config (xmmsc_coll_t *coll, const char *name);

static gint compare_uint (gconstpointer a, gconstpointer b, gpointer userdata);

/* Dumps a propdict on stdout */
static void
propdict_dump (const void *vkey, xmmsc_result_value_type_t type,
               const void *value, const char *vsource, void *udata)
{
	const gchar *source = (const char *) vsource;
	const gchar *key = (const char *) vkey;

	switch (type) {
	case XMMSC_RESULT_VALUE_TYPE_UINT32:
	case XMMSC_RESULT_VALUE_TYPE_INT32:
		g_printf (_("[%s] %s = %u\n"), source, key, XPOINTER_TO_INT (value));
		break;
	case XMMSC_RESULT_VALUE_TYPE_STRING:
		/* FIXME: special handling for url, guess charset, see common.c:print_entry */
		g_printf (_("[%s] %s = %s\n"), source, key, (gchar *) value);
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

/* Compare two uint like strcmp */
static gint
compare_uint (gconstpointer a, gconstpointer b, gpointer userdata)
{
	guint va = *((guint *)a);
	guint vb = *((guint *)b);

	if (va < vb) {
		return -1;
	} else if (va > vb) {
		return 1;
	} else {
		return 0;
	}
}


/* Dummy callback that resets the action status as finished. */
void
done (xmmsc_result_t *res, cli_infos_t *infos)
{
	if (xmmsc_result_iserror (res)) {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (res);
}

void
coldisp_finalize (xmmsc_result_t *res, column_display_t *coldisp)
{
	column_display_print_footer (coldisp);
	column_display_free (coldisp);
}

void
tickle (xmmsc_result_t *res, cli_infos_t *infos)
{
	xmmsc_result_t *res2;

	if (!xmmsc_result_iserror (res)) {
		res2 = xmmsc_playback_tickle (infos->sync);
		xmmsc_result_wait (res2);
		done (res2, infos);
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

void
entry_print_status (xmmsc_result_t *res, cli_infos_t *infos)
{
	gchar *artist;
	gchar *title;

	/* FIXME: ad-hoc display, use richer parser */
	if (!xmmsc_result_iserror (res)) {
		if (xmmsc_result_get_dict_entry_string (res, "artist", &artist)
		    && xmmsc_result_get_dict_entry_string (res, "title", &title)) {
			g_printf (_("Playing: %s - %s\n"), artist, title);
		} else {
			g_printf (_("Error getting metadata!\n"));
		}
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (res);
}

void
id_print_info (xmmsc_result_t *res, guint id)
{
	if (!xmmsc_result_iserror (res)) {
		xmmsc_result_propdict_foreach (res, propdict_dump, NULL);
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

void
list_print_info (xmmsc_result_t *res, cli_infos_t *infos)
{
	xmmsc_result_t *infores = NULL;
	guint id;
	gboolean first = true;

	if (!xmmsc_result_iserror (res)) {
		while (xmmsc_result_list_valid (res)) {
			if (xmmsc_result_get_uint (res, &id)) {
				infores = xmmsc_medialib_get_info (infos->sync, id);
				xmmsc_result_wait (infores);

				if (!first) {
					g_printf ("\n");
				} else {
					first = false;
				}
				id_print_info (infores, id);
			}
			xmmsc_result_list_next (res);
		}

	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (res);
}

void
id_print_row (xmmsc_result_t *res, column_display_t *coldisp)
{
	column_display_print (coldisp, res);
	xmmsc_result_unref (res);
}

void
list_print_row (xmmsc_result_t *res, column_display_t *coldisp)
{
	/* FIXME: w00t at code copy-paste, please modularize */
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
				infores = xmmsc_medialib_get_info (infos->sync, id);
				xmmsc_result_wait (infores);
				column_display_print (coldisp, infores);
			}
			xmmsc_result_list_next (res);
			i++;
		}

	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	column_display_print_footer (coldisp);
	column_display_free (coldisp);
	cli_infos_loop_resume (infos);

	xmmsc_result_unref (res);
}

void
list_print_playlists (xmmsc_result_t *res, cli_infos_t *infos, gboolean all)
{
	gchar *s;

	if (!xmmsc_result_iserror (res)) {
		while (xmmsc_result_list_valid (res)) {
			/* Skip hidden playlists if all is FALSE*/
			if (xmmsc_result_get_string (res, &s) && ((*s != '_') || all)) {
				/* Highlight active playlist */
				if (strcmp (s, infos->cache->active_playlist_name) == 0) {
					g_printf ("* %s\n", s);
				} else {
					g_printf ("  %s\n", s);
				}
			}
			xmmsc_result_list_next (res);
		}
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (res);
}

/* Abstract jump, use inc to choose the direction. */
static void
list_jump_rel (xmmsc_result_t *res, cli_infos_t *infos, gint inc)
{
	guint i;
	guint id;
	xmmsc_result_t *jumpres = NULL;

	gint currpos;
	gint plsize;
	GArray *playlist;

	currpos = infos->cache->currpos;
	plsize = infos->cache->active_playlist->len;
	playlist = infos->cache->active_playlist;

	if (!xmmsc_result_iserror (res) && xmmsc_result_list_valid (res)) {

		inc += plsize; /* magic trick so we can loop in either direction */

		/* Loop on the playlist */
		for (i = (currpos + inc) % plsize; i != currpos; i = (i + inc) % plsize) {

			/* Loop on the matched media */
			for (xmmsc_result_list_first (res);
			     xmmsc_result_list_valid (res);
			     xmmsc_result_list_next (res)) {

				/* If both match, jump! */
				if (xmmsc_result_get_uint (res, &id)
				    && g_array_index (playlist, guint, i) == id) {
					jumpres = xmmsc_playlist_set_next (infos->sync, i);
					xmmsc_result_wait (jumpres);
					tickle (jumpres, infos);
					goto finish;
				}
			}
		}
	}

	finish:

	/* No matching media found, don't jump */
	if (!jumpres) {
		g_printf (_("No media matching the pattern in the playlist!\n"));
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
list_jump_back (xmmsc_result_t *res, cli_infos_t *infos)
{
	list_jump_rel (res, infos, -1);
}

void
list_jump (xmmsc_result_t *res, cli_infos_t *infos)
{
	list_jump_rel (res, infos, 1);
}

void
add_list (xmmsc_result_t *matching, cli_infos_t *infos,
             gchar *playlist, gint pos)

{
	/* FIXME: w00t at code copy-paste, please modularize */
	xmmsc_result_t *insres;
	guint id;
	gint offset;

	offset = 0;

	if (xmmsc_result_iserror (matching) || !xmmsc_result_is_list (matching)) {
		g_printf (_("Error retrieving the media matching the pattern!\n"));
	} else {
		/* Loop on the matched media */
		for (xmmsc_result_list_first (matching);
		     xmmsc_result_list_valid (matching);
		     xmmsc_result_list_next (matching)) {

			if (xmmsc_result_get_uint (matching, &id)) {
				insres = xmmsc_playlist_insert_id (infos->sync, playlist,
				                                   pos + offset, id);
				xmmsc_result_unref (insres);
				offset++;
			}
		}
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (matching);
}

void
move_entries (xmmsc_result_t *matching, cli_infos_t *infos,
                 gchar *playlist, gint pos)
{
	xmmsc_result_t *movres, *lisres;
	guint id, curr;
	gint inc;
	gboolean up;
	GTree *list;

	if (xmmsc_result_iserror (matching) || !xmmsc_result_is_list (matching)) {
		g_printf (_("Error retrieving the media matching the pattern!\n"));
	} else {
		lisres = xmmsc_playlist_list_entries (infos->sync, playlist);
		xmmsc_result_wait (lisres);

		if (xmmsc_result_iserror (lisres) || !xmmsc_result_is_list (lisres)) {
			g_printf (_("Error retrieving playlist entries\n"));
			goto finish;
		}

		list = g_tree_new_full (compare_uint, NULL, g_free, NULL);

		for (xmmsc_result_list_first (matching);
		     xmmsc_result_list_valid (matching);
		     xmmsc_result_list_next (matching)) {
			
			if (xmmsc_result_get_uint (matching, &id)) {
				guint *tid;
				tid = g_new (guint, 1);
				*tid = id;
				g_tree_insert (list, tid, tid);
			}
		}

		curr = 0;
		inc = 0;
		up = TRUE;
		for (xmmsc_result_list_first (lisres);
		     xmmsc_result_list_valid (lisres);
		     xmmsc_result_list_next (lisres)) {
			if (curr == pos) {
				inc = 0;
				up = FALSE;
			}
			if (xmmsc_result_get_uint (lisres, &id) &&
			    g_tree_lookup (list, &id) != NULL) {
				if (up) {
					movres = xmmsc_playlist_move_entry (infos->sync,
					                                    playlist, curr-inc, pos);
				} else {
					movres = xmmsc_playlist_move_entry (infos->sync,
					                                    playlist, curr+inc, pos);
				}
				xmmsc_result_wait (movres);
				xmmsc_result_unref (movres);
				inc++;
			}
			curr++;
		}
		g_tree_destroy (list);
	}

    finish:

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (matching);
	xmmsc_result_unref (lisres);
}

void
remove_cached_list (xmmsc_result_t *matching, cli_infos_t *infos)
{
	/* FIXME: w00t at code copy-paste, please modularize */
	xmmsc_result_t *rmres;
	guint plid, id;
	gint plsize;
	GArray *playlist;
	gint i;

	plsize = infos->cache->active_playlist->len;
	playlist = infos->cache->active_playlist;

	if (xmmsc_result_iserror (matching) || !xmmsc_result_is_list (matching)) {
		g_printf (_("Error retrieving the media matching the pattern!\n"));
	} else {
		/* Loop on the playlist (backward, easier to remove) */
		for (i = plsize - 1; i >= 0; i--) {
			plid = g_array_index (playlist, guint, i);

			/* Loop on the matched media */
			for (xmmsc_result_list_first (matching);
			     xmmsc_result_list_valid (matching);
			     xmmsc_result_list_next (matching)) {

				/* If both match, remove! */
				if (xmmsc_result_get_uint (matching, &id) && plid == id) {
					rmres = xmmsc_playlist_remove_entry (infos->sync, NULL, i);
					xmmsc_result_wait (rmres);
					xmmsc_result_unref (rmres);
					break;
				}
			}
		}
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (matching);
}

void
remove_list (xmmsc_result_t *matchres, xmmsc_result_t *plistres,
                cli_infos_t *infos, gchar *playlist)
{
	/* FIXME: w00t at code copy-paste, please modularize */
	xmmsc_result_t *rmres;
	guint plid, id, i;
	gint offset;

	if (xmmsc_result_iserror (matchres) || !xmmsc_result_is_list (matchres)) {
		g_printf (_("Error retrieving the media matching the pattern!\n"));
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (matchres));
	} else if (xmmsc_result_iserror (plistres) || !xmmsc_result_is_list (plistres)) {
		g_printf (_("Error retrieving the playlist!\n"));
	} else {
		/* FIXME: Can we use a GList to remove more safely in the rev order? */
		offset = 0;
		i = 0;

		/* Loop on the playlist */
		for (xmmsc_result_list_first (plistres);
		     xmmsc_result_list_valid (plistres);
		     xmmsc_result_list_next (plistres)) {

			if (!xmmsc_result_get_uint (plistres, &plid)) {
				plid = 0;  /* failed to get id, should not happen */
			}

			/* Loop on the matched media */
			for (xmmsc_result_list_first (matchres);
			     xmmsc_result_list_valid (matchres);
			     xmmsc_result_list_next (matchres)) {

				/* If both match, jump! */
				if (xmmsc_result_get_uint (matchres, &id) && plid == id) {
					rmres = xmmsc_playlist_remove_entry (infos->sync, playlist,
					                                     i - offset);
					xmmsc_result_wait (rmres);
					xmmsc_result_unref (rmres);
					offset++;
					break;
				}
			}

			i++;
		}
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (matchres);
	xmmsc_result_unref (plistres);
}

void
copy_playlist (xmmsc_result_t *res, cli_infos_t *infos, gchar *playlist)
{
	xmmsc_result_t *saveres;
	xmmsc_coll_t *coll;

	if (xmmsc_result_get_collection (res, &coll)) {
		saveres = xmmsc_coll_save (infos->sync, coll, playlist,
		                           XMMS_COLLECTION_NS_PLAYLISTS);
		xmmsc_result_wait (saveres);
		done (saveres, infos);
	} else {
		g_printf (_("Cannot find the playlist to copy!\n"));
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
configure_playlist (xmmsc_result_t *res, cli_infos_t *infos, gchar *playlist,
                       gint history, gint upcoming, xmmsc_coll_type_t type,
                       gchar *input)
{
	xmmsc_result_t *saveres;
	xmmsc_coll_t *coll;
	xmmsc_coll_t *newcoll;

	if (xmmsc_result_get_collection (res, &coll)) {
		if (type >= 0 && xmmsc_coll_get_type (coll) != type) {
			newcoll = coll_copy_retype (coll, type);
			xmmsc_coll_unref (coll);
			coll = newcoll;
		}
		if (history >= 0) {
			coll_int_attribute_set (coll, "history", history);
		}
		if (upcoming >= 0) {
			coll_int_attribute_set (coll, "upcoming", upcoming);
		}
		if (input) {
			/* Replace previous operand. */
			newcoll = coll_make_reference (input, XMMS_COLLECTION_NS_COLLECTIONS);
			xmmsc_coll_operand_list_clear (coll);
			xmmsc_coll_add_operand (coll, newcoll);
			xmmsc_coll_unref (newcoll);
		}

		saveres = xmmsc_coll_save (infos->sync, coll, playlist,
		                           XMMS_COLLECTION_NS_PLAYLISTS);
		xmmsc_result_wait (saveres);
		done (saveres, infos);
	} else {
		g_printf (_("Cannot find the playlist to configure!\n"));
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
playlist_print_config (xmmsc_result_t *res, cli_infos_t *infos,
                          gchar *playlist)
{
	xmmsc_coll_t *coll;

	if (xmmsc_result_get_collection (res, &coll)) {
		coll_print_config (coll, playlist);
	} else {
		g_printf (_("Invalid playlist!\n"));
	}

	cli_infos_loop_resume (infos);

	xmmsc_result_unref (res);
}

gboolean
playlist_exists (cli_infos_t *infos, gchar *playlist)
{
	gboolean retval = FALSE;
	xmmsc_result_t *res;

	res = xmmsc_coll_get (infos->sync, playlist, XMMS_COLLECTION_NS_PLAYLISTS);
	xmmsc_result_wait (res);

	if (!xmmsc_result_iserror (res)) {
		retval = TRUE;
	}

	xmmsc_result_unref (res);

	return retval;
}

static void
coll_int_attribute_set (xmmsc_coll_t *coll, const char *key, gint value)
{
	gchar buf[MAX_INT_VALUE_BUFFER_SIZE + 1];

	g_snprintf (buf, MAX_INT_VALUE_BUFFER_SIZE, "%d", value);
	xmmsc_coll_attribute_set (coll, key, buf);
}

static xmmsc_coll_t *
coll_make_reference (const char *name, xmmsc_coll_namespace_t ns)
{
	xmmsc_coll_t *ref;

	ref = xmmsc_coll_new (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsc_coll_attribute_set (ref, "reference", name);
	xmmsc_coll_attribute_set (ref, "namespace", ns);

	return ref;
}

static void
coll_copy_attributes (const char *key, const char *value, void *udata)
{
	xmmsc_coll_attribute_set ((xmmsc_coll_t *) udata, key, value);
}

static xmmsc_coll_t *
coll_copy_retype (xmmsc_coll_t *coll, xmmsc_coll_type_t type)
{
	xmmsc_coll_t *copy;
	gint idlistsize;
	gint i;
	guint id;

	copy = xmmsc_coll_new (type);

	idlistsize = xmmsc_coll_idlist_get_size (coll);
	for (i = 0; i < idlistsize; i++) {
		xmmsc_coll_idlist_get_index (coll, i, &id);
		xmmsc_coll_idlist_append (copy, id);
	}

	xmmsc_coll_attribute_foreach (coll, coll_copy_attributes, copy);

	return copy;
}

static void
coll_print_config (xmmsc_coll_t *coll, const char *name)
{
	xmmsc_coll_t *op;
	xmmsc_coll_type_t type;
	gchar *upcoming = NULL;
	gchar *history = NULL;
	gchar *input = NULL;
	gchar *input_ns = NULL;

	type = xmmsc_coll_get_type (coll);

	xmmsc_coll_attribute_get (coll, "upcoming", &upcoming);
	xmmsc_coll_attribute_get (coll, "history", &history);

	g_printf (_("name: %s\n"), name);

	switch (type) {
	case XMMS_COLLECTION_TYPE_IDLIST:
		g_printf (_("type: list\n"));
		break;
	case XMMS_COLLECTION_TYPE_QUEUE:
		g_printf (_("type: queue\n"));
		g_printf (_("history: %s\n"), history);
		break;
	case XMMS_COLLECTION_TYPE_PARTYSHUFFLE:
		xmmsc_coll_operand_list_first (coll);
		if (xmmsc_coll_operand_list_entry (coll, &op)) {
			xmmsc_coll_attribute_get (op, "reference", &input);
			xmmsc_coll_attribute_get (op, "namespace", &input_ns);
		}

		g_printf (_("type: pshuffle\n"));
		g_printf (_("history: %s\n"), history);
		g_printf (_("upcoming: %s\n"), upcoming);
		g_printf (_("input: %s/%s\n"), input_ns, input);
		break;
	default:
		g_printf (_("type: unknown!\n"));
		break;
	}
}
