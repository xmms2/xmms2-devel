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
	gint total_time;
	gint termwidth;
	gint availchars;
	gchar *buffer;       /* Used to render strings. */
};

struct column_def_St {
	const gchar *name;
	union {
		const gchar *string;
		gpointer udata;
	} arg;
	guint size;
	column_def_size_t size_type;
	column_def_align_t align;
	column_display_rendering_f render;

	guint requested_size;
};


/* FIXME:
   - make columns as narrow as possible (check widest value)
   - no width restriction when sending to a pipe (currently 80)
   - use a GString to accomodate unknown buffer size
   - proper column_width on all platforms
*/


/* Copy a cropped version of str to dest, of maximum length
 * max_len. Returns the number of columns the cropped string consists
 * of. */
static guint
crop_string (gchar *dest, const gchar *str, guint max_len)
{
	gunichar *buffer;
	gunichar *tmp;
	glong ucslen;
	guint columns = 0;
	glong bytes = 0;
	gchar *utf8tmp;
	const gchar *iter;
	const gunichar dot = g_utf8_get_char (".");
	const gunichar space = g_utf8_get_char (" ");

	if (!str || max_len == 0 ) {
		return 0;
	}

	ucslen = sizeof(gunichar) * max_len;
	buffer = g_malloc (ucslen);
	tmp = buffer;

	iter = str;

	/* Copy at most max_len columns of characters from str to buffer */
	while (*iter && columns < max_len) {
		gunichar ch = g_utf8_get_char (iter);
		++columns;
		if (g_unichar_iswide (ch)) {
			++columns;
		}
		if (columns > max_len) {
			*tmp = dot;
			++tmp;
			break;
		}
		*tmp = ch;
		++tmp;
		iter = g_utf8_next_char (iter);
	}

	/* Not EOS, cut it with an ellipsis */
	if (*iter) {
		--tmp;
		if (g_unichar_iswide (*tmp)) {
			*tmp++ = dot;
			*tmp++ = dot;
		}
		else {
			--tmp;
			if (g_unichar_iswide (*tmp)) {
				*tmp++ = dot;
				*tmp++ = dot;
				*tmp++ = space;
			}
			else {
				*tmp++ = dot;
				*tmp++ = dot;
			}
		}
	}

	utf8tmp = g_ucs4_to_utf8 (buffer, tmp - buffer, NULL, &bytes, NULL);

	strncpy (dest, utf8tmp, bytes);
	g_free (utf8tmp);
	*(dest + bytes) = '\0';

	g_free (buffer);
	return columns;
}

static gint
result_to_string (xmmsv_t *val, column_def_t *coldef, gchar *buffer)
{
	gint realsize;
	gint ival;
	const gchar *sval;
	gchar *value;

	switch (xmmsv_dict_entry_get_type (val, coldef->arg.string)) {
	case XMMSV_TYPE_NONE:
		/* Yeah, lots of code duplication, lets fix that */
		value = NULL;
		if (!strcmp (coldef->arg.string, "title")) {
			if (xmmsv_dict_entry_get_string (val, "url", &sval)) {
				value = g_path_get_basename (sval);
				realsize = crop_string (buffer, value,
				                        coldef->size);
				g_free (value);
			}
		}
		if (!value) {
			*buffer = '\0';
			realsize = 0;
		}
		break;
	case XMMSV_TYPE_INT32:
		xmmsv_dict_entry_get_int (val, coldef->arg.string, &ival);
		realsize = g_snprintf (buffer, coldef->size + 1, "%d", ival);
		break;
	case XMMSV_TYPE_STRING:
		xmmsv_dict_entry_get_string (val, coldef->arg.string, &sval);

		/* Use UTF-8 column width, not characters or bytes */
		/* Note: realsize means the number of columns used to
		 * *display* this time, instead of "the numbers of bytes that
		 * would have been written if the buffer was large enough". */
		realsize = crop_string (buffer, sval, coldef->size);
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
                          column_def_align_t align, gchar padchar)
{
	if (align == COLUMN_DEF_ALIGN_LEFT) {
		g_printf (value);
		print_padding (width - realsize, padchar);
	} else {
		print_padding (width - realsize, padchar);
		g_printf (value);
	}
}

static void
print_string_using_coldef (column_display_t *disp, column_def_t *coldef,
                           gint realsize)
{
	switch (coldef->size_type) {
	case COLUMN_DEF_SIZE_FIXED:
	case COLUMN_DEF_SIZE_RELATIVE:
		/* Print fixed, with padding/alignment */
		print_fixed_width_string (disp->buffer, coldef->size, realsize,
		                          coldef->align, ' ');
		break;

	case COLUMN_DEF_SIZE_AUTO:
		/* Just print the string */
		g_printf (disp->buffer);
		break;
	}
}

static column_def_t *
column_def_init (const gchar *name, guint size, column_def_size_t size_type,
                 column_def_align_t align, column_display_rendering_f render)
{
	column_def_t *coldef;

	coldef = g_new0 (column_def_t, 1);
	coldef->name = name;
	coldef->size = size;
	coldef->requested_size = size;
	coldef->size_type = size_type;
	coldef->align = align;
	coldef->render = render;

	return coldef;
}

static column_def_t *
column_def_init_with_prop (const gchar *name, const gchar *property, guint size,
                           column_def_size_t size_type, column_def_align_t align,
                           column_display_rendering_f render)
{
	column_def_t *coldef;

	coldef = column_def_init (name, size, size_type, align, render);
	coldef->arg.string = property;

	return coldef;
}

static column_def_t *
column_def_init_with_udata (const gchar *name, gpointer udata, guint size,
                            column_def_size_t size_type, column_def_align_t align,
                            column_display_rendering_f render)
{
	column_def_t *coldef;

	coldef = column_def_init (name, size, size_type, align, render);
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
	disp->total_time = 0;
	disp->termwidth = find_terminal_width ();
	disp->availchars = 0;
	disp->buffer = NULL;

	return disp;
}

void
column_display_add_separator (column_display_t *disp, const gchar *sep)
{
	column_def_t *coldef;
	coldef = column_def_init (sep, strlen (sep), COLUMN_DEF_SIZE_FIXED,
	                          COLUMN_DEF_ALIGN_LEFT,
	                          column_display_render_text);
	disp->cols = g_array_append_val (disp->cols, coldef);
}

void
column_display_add_property (column_display_t *disp, const gchar *label,
                             const gchar *prop, guint size,
                             column_def_size_t size_type,
                             column_def_align_t align)
{
	column_def_t *coldef;
	coldef = column_def_init_with_prop (label, prop, size, size_type, align,
	                                    column_display_render_property);
	disp->cols = g_array_append_val (disp->cols, coldef);
}

void
column_display_add_format (column_display_t *disp, const gchar *label,
                           const gchar *format, guint size,
                           column_def_size_t size_type,
                           column_def_align_t align)
{
	column_def_t *coldef;
	coldef = column_def_init_with_udata (label, (gpointer) format, size,
	                                     size_type, align,
	                                     column_display_render_format);
	disp->cols = g_array_append_val (disp->cols, coldef);
}

/* FIXME: custom selector string */
void
column_display_add_special (column_display_t *disp, const gchar *label,
                            gpointer userdata, guint size,
                            column_def_size_t size_type,
                            column_def_align_t align,
                            column_display_rendering_f render)
{
	column_def_t *coldef;
	coldef = column_def_init_with_udata (label, userdata, size,
	                                     size_type, align, render);
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
	gint realsize;
	column_def_t *coldef;
	gchar *headstr;

	/* Display Result head line */
	headstr = _("--[Result]-");
	print_fixed_width_string (headstr, disp->termwidth, strlen (headstr),
	                          COLUMN_DEF_ALIGN_LEFT, '-');
	g_printf ("\n");

	/* Display column headers */
	for (i = 0; i < disp->cols->len; ++i) {
		coldef = g_array_index (disp->cols, column_def_t *, i);
		realsize = g_snprintf (disp->buffer, coldef->size + 1, coldef->name);
		print_fixed_width_string (disp->buffer, coldef->size, realsize,
		                          coldef->align, ' ');
	}
	g_printf ("\n");

}

void
column_display_print_footer (column_display_t *disp)
{
	gint realsize;

	realsize = g_snprintf (disp->buffer, disp->termwidth + 1,
	                       _("-[Count:%6.d]--"), disp->counter);

	print_fixed_width_string (disp->buffer, disp->termwidth, realsize,
	                          COLUMN_DEF_ALIGN_RIGHT, '-');
	g_printf ("\n");
}

void
column_display_print_footer_totaltime (column_display_t *disp)
{
	gchar *time = format_time (disp->total_time, TRUE);

	g_printf (_("Total playtime: %s\n"), time);

	g_free (time);
}

/* Compute size for relative columns, allocate buffer, etc.
 * If auto and relative columns are mixed, give a size to auto columns
 * to keep room for them.
 */
void
column_display_prepare (column_display_t *disp)
{
	gint termwidth, availchars, totalchars;
	gint i;
	double ratio;
	column_def_t *coldef;

	availchars = disp->termwidth;
	totalchars = 0;

	/* Exclude fixed-size columns */
	for (i = 0; i < disp->cols->len; ++i) {
		coldef = g_array_index (disp->cols, column_def_t *, i);
		if (coldef->size_type == COLUMN_DEF_SIZE_FIXED) {
			availchars -= coldef->requested_size;
		} else {
			totalchars += coldef->requested_size;
		}
	}

	/* Adapt the size of relative columns */
	ratio = ((double) availchars / totalchars);
	for (i = 0; i < disp->cols->len; ++i) {
		coldef = g_array_index (disp->cols, column_def_t *, i);
		if (coldef->size_type == COLUMN_DEF_SIZE_RELATIVE) {
			coldef->size = coldef->requested_size * ratio;
			availchars -= coldef->size;
		}
	}

	/* Chars available to the auto columns */
	disp->availchars = availchars;
	disp->buffer = g_new0 (gchar, (disp->termwidth * sizeof(gunichar)) + 1);
}

void
column_display_print (column_display_t *disp, xmmsv_t *val)
{
	gint i, colwidth, availchars, millisecs;
	column_def_t *coldef;
	const gchar *err;

	availchars = disp->availchars;

	if (!xmmsv_get_error (val, &err)) {
		for (i = 0; i < disp->cols->len; ++i) {
			coldef = g_array_index (disp->cols, column_def_t *, i);
			if (coldef->size_type == COLUMN_DEF_SIZE_AUTO) {
				coldef->size = availchars;
			}
			colwidth = coldef->render (disp, coldef, val);
			if (coldef->size_type == COLUMN_DEF_SIZE_AUTO) {
				availchars = (availchars >= colwidth ? availchars - colwidth : 0);
			}
		}
		g_printf ("\n");

		if (xmmsv_dict_entry_get_int (val, "duration", &millisecs)) {
			disp->total_time += millisecs;
		}

		disp->counter++;
	} else {
		g_printf (_("Server error: %s\n"), err);
	}
}

/** Display a row counter */
gint
column_display_render_position (column_display_t *disp, column_def_t *coldef,
                                xmmsv_t *val)
{
	gint realsize;

	realsize = g_snprintf (disp->buffer, coldef->size + 1, "%d",
	                       disp->counter + 1);
	print_string_using_coldef (disp, coldef, realsize);

	return realsize;
}

/**
 * Highlight if the position of the row matches the userdata integer
 * value.
 */
gint
column_display_render_highlight (column_display_t *disp, column_def_t *coldef,
                                 xmmsv_t *val)
{
	gint realsize, highlight = GPOINTER_TO_INT(coldef->arg.udata);

	/* FIXME: Make these customizable */
	if (disp->counter == highlight) {
		realsize = g_printf ("->");
	} else {
		realsize = g_printf ("  ");
	}

	return realsize;
}

gint
column_display_render_next (column_display_t *disp, column_def_t *coldef,
                            xmmsv_t *val)
{
	gint realsize, curr = GPOINTER_TO_INT(coldef->arg.udata);

	realsize = g_snprintf (disp->buffer, coldef->size + 1, "%+d",
	                       disp->counter - curr);
	print_string_using_coldef (disp, coldef, realsize);

	return realsize;
}

/** Always render the label text. */
gint
column_display_render_text (column_display_t *disp, column_def_t *coldef,
                            xmmsv_t *val)
{
	gint realsize;
	const gchar *sep = coldef->name;

	realsize = g_printf (sep);

	return realsize;
}

/** Interpret int/uint value as a time */
gint
column_display_render_time (column_display_t *disp, column_def_t *coldef,
                            xmmsv_t *val)
{
	gint realsize;
	guint millisecs;
	gchar *time;
	const gchar *propname = (const gchar *) coldef->arg.udata;

	switch (xmmsv_dict_entry_get_type (val, propname)) {
	case XMMSV_TYPE_INT32:
		xmmsv_dict_entry_get_int (val, propname, &millisecs);
		break;
	default:
		/* Invalid type, don't render anything*/
		return;
	}

	time = format_time (millisecs, FALSE);

	/* We recopy the string to crop it if needed */
/* 	realsize = g_snprintf (disp->buffer, coldef->size + 1, time); */

	/* FIXME: hack, do _not_ crop size. Note that this might break
	 * alignment and overflow termwidth if strlen(time) > coldef->size,
	 * but it's a temporary fix to avoid displaying a wrong time.
	 */
	realsize = g_snprintf (disp->buffer, disp->termwidth + 1, time);
	print_string_using_coldef (disp, coldef, realsize);

	g_free (time);

	return realsize;
}

/** Render the selected property, possibly shortened. */
gint
column_display_render_property (column_display_t *disp, column_def_t *coldef,
                                xmmsv_t *val)
{
	gint realsize;

	realsize = result_to_string (val, coldef, disp->buffer);
	print_string_using_coldef (disp, coldef, realsize);

	return realsize;
}

/** Render the value using the provided format string. */
gint
column_display_render_format (column_display_t *disp, column_def_t *coldef,
                              xmmsv_t *val)
{
	gint realsize;
	const gchar *format = (const gchar *) coldef->arg.udata;

	/* Format the whole string first */
	xmmsc_entry_format (disp->buffer, disp->termwidth, format, val);

	/* then crop (crop_string uses internal buffer, so it's ok that dest=src) */
	realsize = crop_string (disp->buffer, disp->buffer, coldef->size);
	print_string_using_coldef (disp, coldef, realsize);

	return realsize;
}
