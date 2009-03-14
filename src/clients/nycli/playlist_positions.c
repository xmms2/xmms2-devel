/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include "playlist_positions.h"

#include <stdlib.h>
#include <string.h>

/** The goal is to parse the following grammar:
 *
 * positions := sequence | curr-context | curr-sequence
 * sequence  := interval | sequence "," interval
 * curr-context  := integer "_" integer | "_" integer | integer "_" | "_"
 * curr-sequence := "+" sequence | "-" sequence
 * interval  := integer | integer "-" integer | "*" "-" integer | integer "-" "*"
 * integer   := digit | integer digit
 * digit     := [0-9]
 */

/* Note: duplicates are excluded during the construction */
struct playlist_positions_St {
	gint cached_currpos; /* Needed for relative positions */
	GList *atoms;       /* Int atoms, in decreasing order */
	GList *intervals;   /* Intervals, in decreasing order */
};

struct interval_St {
	gint start;
	gint end;
};

typedef struct interval_St interval_t;

typedef enum {
	INTERVAL_IS_GREATER,
	ATOM_IS_GREATER
} interval_vs_atom_t;

#define NEXT_POSITION(list, forward) forward ? g_list_previous (list) : g_list_next (list)

static int playlist_positions_count (playlist_positions_t *positions);
static gboolean playlist_positions_parse_token (const gchar *expr, playlist_positions_t *p);
static gboolean playlist_positions_parse_sequence (const gchar *expr, playlist_positions_t *p);
static gboolean playlist_positions_parse_currcontext (const gchar *expr, playlist_positions_t *p);
static gboolean playlist_positions_parse_relsequence (const gchar *expr, playlist_positions_t *p);
static playlist_positions_t *playlist_positions_new ();
static void playlist_positions_add_atom (playlist_positions_t *positions, gint x);
static gboolean playlist_positions_add_interval (playlist_positions_t *positions, gint start, gint end);
static gboolean playlist_positions_intervals_contains_atom (playlist_positions_t *positions, gint x);

static interval_t *interval_new (gint start, gint end);
static void interval_free (interval_t *ival);
static interval_vs_atom_t interval_atom_list_cmp (GList *intervals, GList *atoms, gboolean forward);
static gboolean interval_parse (const gchar *expr, gint *start, gint *end);
static void interval_foreach (interval_t *ival, playlist_positions_func f, gboolean forward, void *userdata);


/* FIXME: not positions vs syntax error! */
/* FIXME: allow forward and backward foreach */
/* FIXME: implement for: add, move, jump, info, search/list? */

gboolean
playlist_positions_parse (const gchar *expr, playlist_positions_t **p,
                          gint currpos)
{
	gchar **tokens;
	playlist_positions_t *positions;
	gint i = 0;
	gboolean success = TRUE;

	/* Empty/NULL string, or invalid chars, don't bother */
	if (!expr || !*expr || strspn (expr, "0123456789,_+- ") != strlen (expr)) {
		return FALSE;
	}

	tokens = g_strsplit (expr, " ", 0);
	positions = playlist_positions_new (currpos);

	while (tokens[i]) {
		if (!playlist_positions_parse_token (tokens[i], positions)) {
			success = FALSE;
			break;
		}
		i++;
	}

	g_strfreev (tokens);

	if (success) {
		*p = positions;
	} else {
		playlist_positions_free (positions);
	}

	return success;
}

void
playlist_positions_foreach (playlist_positions_t *positions,
                            playlist_positions_func f, gboolean forward,
                            void *userdata)
{
	GList *i, *a;
	interval_t *ival;
	int atom;

	if (forward) {
		i = g_list_last (positions->intervals);
		a = g_list_last (positions->atoms);
	} else {
		i = positions->intervals;
		a = positions->atoms;
	}

	while (i || a) {
		switch (interval_atom_list_cmp (i, a, forward)) {
		case INTERVAL_IS_GREATER:
			ival = (interval_t *) i->data;
			interval_foreach (ival, f, forward, userdata);
			i = NEXT_POSITION (i, forward);
			break;
		case ATOM_IS_GREATER:
			atom = GPOINTER_TO_INT (a->data);
			f (atom, userdata);
			a = NEXT_POSITION (a, forward);
			break;
		}
	}
}

gboolean
playlist_positions_get_single (playlist_positions_t *positions, gint *pos)
{
	gboolean retval;

	/* If a single (atom) position, get it */
	if (playlist_positions_count (positions) == 1 && positions->atoms) {
		*pos = GPOINTER_TO_INT (positions->atoms->data);
		retval = TRUE;
	} else {
		retval = FALSE;
	}

	return retval;
}


/* Return the number of positions in the object. */
static int
playlist_positions_count (playlist_positions_t *positions)
{
	GList *n;
	int count = g_list_length (positions->atoms);

	for (n = positions->intervals; n; n = g_list_next (n)) {
		interval_t *ival = (interval_t *) n->data;
		count += ival->end - ival->start;
	}

	return count;
}

static playlist_positions_t *
playlist_positions_new (gint currpos)
{
	playlist_positions_t *positions;

	positions = g_new0 (playlist_positions_t, 1);
	positions->cached_currpos = currpos;

	return positions;
}

void
playlist_positions_free (playlist_positions_t *positions)
{
	GList *n;

	g_list_free (positions->atoms);

	for (n = positions->intervals; n; n = g_list_next (n)) {
		interval_free ((interval_t *) n->data);
	}
	g_list_free (positions->intervals);

	g_free (positions);
}

static gboolean
playlist_positions_parse_token (const gchar *expr, playlist_positions_t *p)
{
	/* Empty string, don't bother */
	if (!*expr) {
		return TRUE;
	}

	if (!playlist_positions_parse_sequence (expr, p) &&
	    !playlist_positions_parse_currcontext (expr, p) &&
	    !playlist_positions_parse_relsequence (expr, p)) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
playlist_positions_parse_sequence (const gchar *expr, playlist_positions_t *p)
{
	gchar **intervals;
	gint i = 0;
	gboolean success = TRUE;
	gint start, end;

	/* Quit on '+' or '-' prefix, which would be a relsequence */
	if (*expr == '+' || *expr == '-') {
		return FALSE;
	}

	intervals = g_strsplit (expr, ",", 0);

	while (intervals[i]) {
		if (interval_parse (intervals[i], &start, &end)) {
			if (start >= 0) {
				/* note: user lists start at 1 */
				if (end >= 0) {
					success = playlist_positions_add_interval (p, start - 1, end - 1);
				} else {
					playlist_positions_add_atom (p, start - 1);
				}
			}
		} else {
			success = FALSE;
			break;
		}
		i++;
	}

	g_strfreev (intervals);

	return success;
}

static gboolean
playlist_positions_parse_relsequence (const gchar *expr, playlist_positions_t *p)
{
	gchar **intervals;
	gint i = 0;
	gboolean success = TRUE;
	gint start, end, currpos;

	/* No '+' or '-' prefix, not a relsequence */
	if (*expr != '+' && *expr != '-') {
		return FALSE;
	}

	intervals = g_strsplit (expr + 1, ",", 0);
	currpos = p->cached_currpos;

	if (currpos < 0) {
		/* FIXME: error, cannot use relative currpos if not set */
		return FALSE;
	}

	while (intervals[i] && success) {
		if (interval_parse (intervals[i], &start, &end)) {
			if (start >= 0) {
				start = (*expr == '+') ? start : -start;
				if (end >= 0) {
					end = (*expr == '+') ? end : -end;
					success = playlist_positions_add_interval (p, currpos + start,
					                                           currpos + end);
				} else {
					playlist_positions_add_atom (p, currpos + start);
				}
			}
		} else {
			success = FALSE;
			break;
		}
		i++;
	}

	g_strfreev (intervals);

	return success;
}

static gboolean
playlist_positions_parse_currcontext (const gchar *expr, playlist_positions_t *p)
{
	gint before, after;
	gchar *underscore;
	gchar *endptr;
	gint currpos;

	/* parse [N]_[M] */

	/* No '_' char, not a currcontext */
	underscore = strchr (expr, '_');
	if (!underscore) {
		return FALSE;
	}

	if (expr == underscore) {
		/* No N value */
		before = 0;
	} else {
		before = strtol (expr, &endptr, 10);
		if (endptr != underscore) {
			/* Incorrect parsing, error! */
			return FALSE;
		}
	}

	if (*(underscore + 1) == '\0') {
		/* No M value */
		after = 0;
	} else {
		after = strtol (underscore + 1, &endptr, 10);
		if (*endptr != '\0') {
			/* Incorrect parsing, error! */
			return FALSE;
		}
	}

	/* Parsed a valid interval! - note: user lists start at 1 */
	currpos = p->cached_currpos;

	if (currpos < 0) {
		/* FIXME: error, cannot use relative currpos if not set */
		return FALSE;
	}

	return playlist_positions_add_interval (p, currpos - before,
	                                        currpos + after);
}

static void
playlist_positions_add_atom (playlist_positions_t *positions, gint x)
{
	GList *n;

	/* Add atom if not already in an interval */
	if (!playlist_positions_intervals_contains_atom (positions, x)) {
		for (n = positions->atoms; n; n = g_list_next (n)) {
			gint curr = GPOINTER_TO_INT (n->data);
			if (curr == x) {
				/* Already present, exit */
				return;
			} else if (curr < x) {
				break;
			}
		}

		/* Found the sorted position, insert the atom */
		positions->atoms = g_list_insert_before (positions->atoms, n,
		                                         GINT_TO_POINTER (x));
	}
}

static gboolean
playlist_positions_add_interval (playlist_positions_t *positions,
                                 gint start, gint end)
{
	GList *n;
	interval_t *ival, *prev_ival;

	/* Invalid interval */
	if (start > end || start < 0) {
		return FALSE;
	}

	if (start == end) {
		playlist_positions_add_atom (positions, start);
		return TRUE;
	}

	/* Merge or add new interval in list */
	prev_ival = NULL;
	for (n = positions->intervals; n; n = g_list_next (n)) {
		ival = (interval_t *) n->data;
		if (start > ival->end) {
			break;
		} else if (end < ival->start) {
			continue;
		} else {
			/* Previous interval already merged, free it and extend current */
			if (prev_ival) {
				ival->end = prev_ival->end;
				interval_free (prev_ival);
				positions->intervals = g_list_remove (positions->intervals,
				                                      prev_ival);
			}

			prev_ival = ival;
		}
	}

	if (prev_ival) {
		/* extend end of merged interval */
		if (end > prev_ival->end) {
			prev_ival->end = end;
		}
		/* extend start of merged interval */
		if (start < prev_ival->start) {
			prev_ival->start = start;
		}
	} else {
		/* Not merged with an existing interval, create a new one */
		positions->intervals = g_list_insert_before (positions->intervals, n,
		                                             interval_new (start, end));
	}

	/* Remove atoms included in the new interval */
	for (n = positions->atoms; n; n = g_list_next (n)) {
		gint curr = GPOINTER_TO_INT (n->data);
		if (curr <= end) {
			if (curr >= start) {
				/* atom in the interval, remove */
				positions->atoms = g_list_delete_link (positions->atoms, n);
			} else {
				/* atom smaller than interval, done */
				break;
			}
		}
	}

	return TRUE;
}

static gboolean
playlist_positions_intervals_contains_atom (playlist_positions_t *positions, gint x)
{
	GList *n;

	for (n = positions->intervals; n; n = g_list_next (n)) {
		interval_t *ival = (interval_t *) n->data;
		if (x >= ival->start) {
			if (x > ival->end) {
				/* x larger than biggest interval */
				return FALSE;
			} else {
				/* x inside current interval */
				return TRUE;
			}
		}

		/* Not in this interval, tried the next (smaller) */
	}

	return FALSE;
}

static interval_t *
interval_new (gint start, gint end)
{
	interval_t *ival;

	ival = g_new0 (interval_t, 1);
	ival->start = start;
	ival->end = end;

	return ival;
}

static void
interval_free (interval_t *ival)
{
	g_free (ival);
}

static void
interval_foreach (interval_t *ival, playlist_positions_func f,
                  gboolean forward, void *userdata)
{
	int i;
	int start, end, inc;

	if (forward) {
		start = ival->start;
		end = ival->end + 1;
		inc = 1;
	} else {
		start = ival->end;
		end = ival->start - 1;
		inc = -1;
	}

	for (i = start; i != end; i+=inc) {
		f (i, userdata);
	}
}

static gboolean
interval_parse (const gchar *expr, gint *start, gint *end)
{
	gchar *endptr;

	*start = -1;
	*end = -1;

	/* Empty string, don't bother */
	if (!*expr) {
		return TRUE;
	}

	*start = strtol (expr, &endptr, 10);

	if (*endptr == '-') {
		/* A real interval, get end */
		*end = strtol (endptr + 1, &endptr, 10);
		if (*endptr != '\0') {
			/* Incorrect parsing of end, error! */
			return FALSE;
		}
	} else if (*endptr != '\0') {
		/* Incorrect parsing of start, error! */
		return FALSE;
	}

	return TRUE;
}

static interval_vs_atom_t
interval_atom_list_cmp (GList *intervals, GList *atoms, gboolean forward)
{
	interval_vs_atom_t res;
	interval_t *ival;
	int atom;

	/* Empty lists lose */
	if (!intervals) {
		return ATOM_IS_GREATER;
	}
	if (!atoms) {
		return INTERVAL_IS_GREATER;
	}

	/* Need to compare first items */
	ival = (interval_t *) intervals->data;
	atom = GPOINTER_TO_INT (atoms->data);

	/* If forward invert comparison result */
	if (forward) {
		return (atom < ival->end ? ATOM_IS_GREATER : INTERVAL_IS_GREATER);
	}

	return (atom > ival->end ? ATOM_IS_GREATER : INTERVAL_IS_GREATER);
}
