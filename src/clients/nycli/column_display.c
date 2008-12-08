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
#include "utils.h"

struct column_display_St {
	guint num_cols;
	GArray *cols;
	cli_infos_t *infos;  /* Not really needed, but easier to carry around. */
	gint counter;
	gchar *buffer;       /* Used to render strings. */
};

struct column_def_St {
	const gchar *name;
	union {
		const gchar *string;
		gpointer udata;
	} arg;
	guint size;
	gboolean fixed;
	column_def_align_t align;
	column_display_rendering_f render;

	guint requested_size;
};

static gint
result_to_string (xmmsc_result_t *res, column_def_t *coldef, gchar *buffer)
{
	gint realsize;
	guint uval;
	gint ival;
	const gchar *sval;

	switch (xmmsc_result_get_dict_entry_type (res, coldef->arg.string)) {
	case XMMSC_RESULT_VALUE_TYPE_UINT32:
		xmmsc_result_get_dict_entry_uint (res, coldef->arg.string, &uval);
		realsize = g_snprintf (buffer, coldef->size + 1, "%u", uval);
		break;
	case XMMSC_RESULT_VALUE_TYPE_INT32:
		xmmsc_result_get_dict_entry_int (res, coldef->arg.string, &ival);
		realsize = g_snprintf (buffer, coldef->size + 1, "%d", ival);
		break;
	case XMMSC_RESULT_VALUE_TYPE_STRING:
		xmmsc_result_get_dict_entry_string (res, coldef->arg.string, &sval);
		g_snprintf (buffer, coldef->size + 1, "%s", sval);

		/* count UTF-8 characters, not bytes - FIXME: buggy for Japanese :-/ */
		realsize = g_utf8_strlen (sval, coldef->size + 1);
		break;
	default:
		/* No valid data, display empty value */
		*buffer = '\0';
		realsize = 0;
		break;
	}

	return realsize;
}

static void
print_fixed_width_string (gchar *value, gint width, gint realsize,
                          column_def_align_t align)
{
	/* Value was cut, show an ellispsis */
	if (realsize > width) {
		value[ width - 1 ] = '.';
		value[ width - 2 ] = '.';
	}

	if (align == COLUMN_DEF_ALIGN_LEFT) {
		g_printf (value);
		print_padding (width - realsize);
	} else {
		print_padding (width - realsize);
		g_printf (value);
	}
}

static column_def_t *
column_def_init (const gchar *name, guint size, gboolean fixed,
                 column_def_align_t align, column_display_rendering_f render)
{
	column_def_t *coldef;

	coldef = g_new0 (column_def_t, 1);
	coldef->name = name;
	coldef->size = size;
	coldef->requested_size = size;
	coldef->fixed = fixed;
	coldef->align = align;
	coldef->render = render;

	return coldef;
}

static column_def_t *
column_def_init_with_prop (const gchar *name, const gchar *property, guint size,
                           gboolean fixed, column_def_align_t align,
                           column_display_rendering_f render)
{
	column_def_t *coldef;

	coldef = column_def_init (name, size, fixed, align, render);
	coldef->arg.string = property;

	return coldef;
}

static column_def_t *
column_def_init_with_udata (const gchar *name, gpointer udata, guint size,
                            gboolean fixed, column_def_align_t align,
                            column_display_rendering_f render)
{
	column_def_t *coldef;

	coldef = column_def_init (name, size, fixed, align, render);
	coldef->arg.udata = udata;

	return coldef;
}

/* Free the contents of the column_def (NOT the column_def itself) */
void
column_def_free (column_def_t *coldef)
{
	g_free (coldef);
}

column_display_t *
column_display_init (cli_infos_t *infos)
{
	column_display_t *disp;

	disp = g_new0 (column_display_t, 1);
	disp->infos = infos;
	disp->cols = g_array_new (TRUE, TRUE, sizeof (column_def_t *));
	disp->counter = 0;
	disp->buffer = NULL;

	return disp;
}

void
column_display_add_separator (column_display_t *disp, const gchar *sep)
{
	column_def_t *coldef;
	coldef = column_def_init (sep, strlen (sep), TRUE, COLUMN_DEF_ALIGN_LEFT,
	                          column_display_render_text);
	disp->cols = g_array_append_val (disp->cols, coldef);
}

void
column_display_add_property (column_display_t *disp, const gchar *label,
                             const gchar *prop, guint size, gboolean fixed,
                             column_def_align_t align)
{
	column_def_t *coldef;
	coldef = column_def_init_with_prop (label, prop, size, fixed, align,
	                                    column_display_render_property);
	disp->cols = g_array_append_val (disp->cols, coldef);
}

/* FIXME: custom selector string */
void
column_display_add_special (column_display_t *disp, const gchar *label,
                            gpointer userdata, guint size,
                            column_def_align_t align,
                            column_display_rendering_f render)
{
	column_def_t *coldef;
	coldef = column_def_init_with_udata (label, userdata, size, TRUE, align, render);
	disp->cols = g_array_append_val (disp->cols, coldef);
}

void
column_display_free (column_display_t *disp)
{
	gint i;
	column_def_t *coldef;

	for (i = 0; i < disp->cols->len; ++i) {
		coldef = g_array_index (disp->cols, column_def_t *, i);
		column_def_free (coldef);
	}
	g_array_free (disp->cols, TRUE);

	g_free (disp->buffer);
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
	column_def_t *coldef;

	termwidth = find_terminal_width ();

	/* Display Result head line */
	d = g_printf (_("--[Result]-"));
	for (i = d; i < termwidth; ++i) {
		g_printf ("-");
	}
	g_printf ("\n");

	/* Display column headers */
	for (i = 0; i < disp->cols->len; ++i) {
		coldef = g_array_index (disp->cols, column_def_t *, i);
		realsize = g_snprintf (disp->buffer, coldef->size + 1, coldef->name);
		print_fixed_width_string (disp->buffer, coldef->size, realsize,
		                          coldef->align);
	}
	g_printf ("\n");

}

void
column_display_print_footer (column_display_t *disp)
{
	gint i, d;
	gint termwidth;
	/* FIXME: put elsewhere */
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
	column_def_t *coldef;

	termwidth = find_terminal_width ();
	availchars = termwidth;
	totalchars = 0;

	/* Exclude fixed-size columns */
	for (i = 0; i < disp->cols->len; ++i) {
		coldef = g_array_index (disp->cols, column_def_t *, i);
		if (coldef->fixed) {
			availchars -= coldef->requested_size;
		} else {
			totalchars += coldef->requested_size;
		}
	}

	/* Adapt the size of non-fixed columns */
	ratio = ((double) availchars / totalchars);
	for (i = 0; i < disp->cols->len; ++i) {
		coldef = g_array_index (disp->cols, column_def_t *, i);
		if (!coldef->fixed) {
			coldef->size = coldef->requested_size * ratio;
			availchars -= coldef->requested_size;
		}
	}

	disp->buffer = g_new0 (gchar, termwidth);
}

void
column_display_print (column_display_t *disp, xmmsc_result_t *res)
{
	gint i;
	column_def_t *coldef;

	if (!xmmsc_result_iserror (res)) {
		for (i = 0; i < disp->cols->len; ++i) {
			coldef = g_array_index (disp->cols, column_def_t *, i);
			coldef->render (disp, coldef, res);
		}

		g_printf ("\n");
		disp->counter++;
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

}

/** Display a row counter */
void
column_display_render_position (column_display_t *disp, column_def_t *coldef,
                                xmmsc_result_t *res)
{
	gint realsize;

	realsize = g_snprintf (disp->buffer, coldef->size + 1, "%d",
	                       disp->counter + 1);
	print_fixed_width_string (disp->buffer, coldef->size, realsize,
	                          coldef->align);
}

/**
 * Highlight if the position of the row matches the userdata integer
 * value.
 */
void
column_display_render_highlight (column_display_t *disp, column_def_t *coldef,
                                 xmmsc_result_t *res)
{
	gint highlight = GPOINTER_TO_INT(coldef->arg.udata);

	/* FIXME: Make these customizable */
	if (disp->counter == highlight) {
		g_printf ("->");
	} else {
		g_printf ("  ");
	}
}

void
column_display_render_next (column_display_t *disp, column_def_t *coldef,
                            xmmsc_result_t *res)
{
	gint realsize, curr = GPOINTER_TO_INT(coldef->arg.udata);

	realsize = g_snprintf (disp->buffer, coldef->size + 1, "%+d",
	                       disp->counter - curr);
	print_fixed_width_string (disp->buffer, coldef->size, realsize,
	                          coldef->align);
}

/** Always render the label text. */
void
column_display_render_text (column_display_t *disp, column_def_t *coldef,
                            xmmsc_result_t *res)
{
	const gchar *sep = coldef->name;
	g_printf (sep);
}


/** Render the selected property, possibly shortened. */
void
column_display_render_property (column_display_t *disp, column_def_t *coldef,
                                xmmsc_result_t *res)
{
	gint realsize;

	realsize = result_to_string (res, coldef, disp->buffer);
	print_fixed_width_string (disp->buffer, coldef->size, realsize,
	                          coldef->align);
}
