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

#include "column_display.h"

struct column_display_St {
	size_t num_cols;
	column_def_t *cols;
	cli_infos_t *infos;  /* Not really needed, but easier to carry around. */
	gint counter;
};

struct column_def_St {
	gchar *name;
	gchar *property;
	guint size;
	guint adapted_size;
	gboolean fixed;
};


/* FIXME: not portable? */
static gint
find_terminal_width () {
	gint columns;
	struct winsize ws;
	char *colstr, *endptr;

	if (!ioctl (STDIN_FILENO, TIOCGWINSZ, &ws)) {
		columns = ws.ws_col;
	} else {
		colstr = getenv ("COLUMNS");
		if (colstr != NULL) {
			columns = strtol (colstr, &endptr, 10);
		}
	}

	/* Default to 80 columns */
	if (columns <= 0) {
		columns = 80;
	}

	return columns;
}

static gint
result_to_string (xmmsc_result_t *res, column_def_t *coldef, gchar *buffer)
{
	gint realsize;
	guint uval;
	gint ival;
	gchar *sval;

	switch (xmmsc_result_get_dict_entry_type (res, coldef->property)) {
	case XMMSC_RESULT_VALUE_TYPE_UINT32:
		xmmsc_result_get_dict_entry_uint (res, coldef->property, &uval);
		realsize = g_snprintf (buffer, coldef->adapted_size + 1, "%u", uval);
		break;
	case XMMSC_RESULT_VALUE_TYPE_INT32:
		xmmsc_result_get_dict_entry_int (res, coldef->property, &ival);
		realsize = g_snprintf (buffer, coldef->adapted_size + 1, "%d", ival);
		break;
	case XMMSC_RESULT_VALUE_TYPE_STRING:
		xmmsc_result_get_dict_entry_string (res, coldef->property, &sval);
		realsize = g_snprintf (buffer, coldef->adapted_size + 1, "%s", sval);
		break;
	default:
		/* No valid data, display empty value */
		realsize = g_snprintf (buffer, coldef->adapted_size + 1, "");
		break;
	}

	return realsize;
}

static void
print_fixed_width_value (gchar *value, gint width, gint realsize)
{
	/* Value was cut, show an ellispsis */
	if (realsize > width) {
		value[ width - 1 ] = '.';
		value[ width - 2 ] = '.';
	}

	g_printf (value);

	/* Pad with spaces */
	if (width > realsize) {
		gint k;
		for (k = realsize; k < width; ++k) {
			g_printf (" ");
		}
	}
}


void
column_def_elem_init (column_def_t *coldef, const gchar *name,
                      const gchar *property, guint size, gboolean fixed)
{
	coldef->name = g_strdup (name);
	coldef->property = g_strdup (property);
	coldef->size = size;
	coldef->adapted_size = size;
	coldef->fixed = fixed;
}

column_def_t *
column_def_init (const gchar *name, const gchar *property, guint size,
                 gboolean fixed)
{
	column_def_t *coldef;

	coldef = g_new0 (column_def_t, 1);
	column_def_elem_init (coldef, name, property, size, fixed);

	return coldef;
}

/* Free the contents of the column_def (NOT the column_def itself) */
void
column_def_elem_free (column_def_t *coldef)
{
	g_free (coldef->name);
	g_free (coldef->property);
}

column_display_t *
column_display_init (cli_infos_t *infos)
{
	column_display_t *disp;

	disp = g_new0 (column_display_t, 1);
	disp->infos = infos;
	disp->num_cols = 0;
	disp->cols = NULL;
	disp->counter = 0;

	return disp;
}

void
column_display_fill (column_display_t *disp, const gchar **props)
{
	gint i;

	/* Free previous coldefs */
	if (disp->num_cols > 0) {
		for (i = 0; i < disp->num_cols; ++i) {
			column_def_elem_free (&disp->cols[i]);
		}
		g_free (disp->cols);
	}

	/* Count columns */
	for (i = 0; props[i]; ++i);

	disp->num_cols = i;
	disp->cols = g_new0 (column_def_t, i);
	for (i = 0; props[i]; ++i) {
		/* FIXME: clever this up */
		if (strcmp (props[i], "id") == 0) {
			column_def_elem_init (&disp->cols[i], props[i], props[i], 5, TRUE);
		} else {
			column_def_elem_init (&disp->cols[i], props[i], props[i], 20, FALSE);
		}
	}
}

void
column_display_fill_default (column_display_t *disp)
{
	const gchar *default_columns[] = { "id", "artist", "album", "title", NULL };
	column_display_fill (disp, default_columns);
}

void
column_display_free (column_display_t *disp)
{
	gint i;

	if (disp->cols) {
		for (i = 0; i < disp->num_cols; ++i) {
			column_def_elem_free (&disp->cols[i]);
		}
		g_free (disp->cols);
	}

	g_free (disp);
}

cli_infos_t *
column_display_infos_get (column_display_t *disp)
{
	return disp->infos;
}

void
column_display_print_header (column_display_t *disp)
{
	gint i, d;
	gint termwidth;
	gint realsize;
	gchar value[40]; /* FIXME: warning overflow? */

	termwidth = find_terminal_width ();

	/* Display Result head line */
	d = g_printf (_("--[Result]-"));
	for (i = d; i < termwidth; ++i) {
		g_printf ("-");
	}
	g_printf ("\n");

	/* Display column headers */
	for (i = 0; i < disp->num_cols; ++i) {
		if (i > 0) {
			g_printf ("|");
		}

		realsize = g_snprintf (value, disp->cols[i].adapted_size + 1, disp->cols[i].name);
		print_fixed_width_value (value, disp->cols[i].adapted_size, realsize);
	}
	g_printf ("\n");

}

void
column_display_print_footer (column_display_t *disp)
{
	gint i, d;
	gint termwidth;
	const gint maxbufsize = 30;
	gchar buf[maxbufsize];

	termwidth = find_terminal_width ();
	d = g_snprintf (buf, maxbufsize, _("-[Count:%6.d]--"), disp->counter);

	for (i = d; i < termwidth; ++i) {
		g_printf ("-");
	}

	g_printf ("%s\n", buf);
}

void
column_display_prepare (column_display_t *disp)
{
	gint termwidth, availchars, totalchars;
	gint i;
	double ratio;

	termwidth = find_terminal_width ();
	availchars = termwidth;
	totalchars = 0;

	/* Exclude fixed-size columns */
	for (i = 0; i < disp->num_cols; ++i) {
		if (disp->cols[i].fixed) {
			availchars -= disp->cols[i].size;
		} else {
			totalchars += disp->cols[i].size;
		}
	}
	availchars -= disp->num_cols - 1; /* exclude separators */

	/* Adapt the size of non-fixed columns */
	ratio = ((double) availchars / totalchars);
	for (i = 0; i < disp->num_cols; ++i) {
		if (!disp->cols[i].fixed) {
			disp->cols[i].adapted_size = disp->cols[i].size * ratio;
			availchars -= disp->cols[i].adapted_size;
		}
	}
}

void
column_display_print (column_display_t *disp, xmmsc_result_t *res)
{
	if (!xmmsc_result_iserror (res)) {
		gint i;

		for (i = 0; i < disp->num_cols; ++i) {
			gint realsize, printsize;
			gchar value[40]; /* FIXME: warning overflow? */

			/* Display separator and fixed-size value */
			if (i > 0) {
				g_printf ("|");
			}

			realsize = result_to_string (res, &disp->cols[i], value);
			print_fixed_width_value (value, disp->cols[i].adapted_size, realsize);
		}

		g_printf ("\n");
		disp->counter++;
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

}
