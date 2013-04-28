/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#ifndef __COLUMN_DISPLAY_H__
#define __COLUMN_DISPLAY_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>

typedef struct column_display_St column_display_t;
typedef struct column_def_St column_def_t;
typedef gint (*column_display_rendering_f)(column_display_t *disp, column_def_t *coldef, xmmsv_t *val);

typedef enum {
	COLUMN_DEF_ALIGN_LEFT,
	COLUMN_DEF_ALIGN_RIGHT
} column_def_align_t;

typedef enum {
	COLUMN_DEF_SIZE_FIXED,
	COLUMN_DEF_SIZE_RELATIVE,
	COLUMN_DEF_SIZE_AUTO
} column_def_size_t;

column_display_t *column_display_init (void);
void column_display_add_separator (column_display_t *disp, const gchar *sep);
void column_display_add_property (column_display_t *disp, const gchar *label, const gchar *prop, guint size, column_def_size_t size_type, column_def_align_t align);
void column_display_add_format (column_display_t *disp, const gchar *label, const gchar *format, guint size, column_def_size_t size_type, column_def_align_t align);
void column_display_add_special (column_display_t *disp, const gchar *label, void *userdata, guint size, column_def_size_t size_type, column_def_align_t align, column_display_rendering_f render);
void column_display_free (column_display_t *disp);
void column_display_prepare (column_display_t *disp);
void column_display_print (column_display_t *disp, xmmsv_t *res);
void column_display_print_header (column_display_t *disp);
void column_display_print_footer (column_display_t *disp);
void column_display_print_footer_totaltime (column_display_t *disp);

gint column_display_render_position (column_display_t *disp, column_def_t *coldef, xmmsv_t *val);
gint column_display_render_highlight (column_display_t *disp, column_def_t *coldef, xmmsv_t *val);
gint column_display_render_next (column_display_t *disp, column_def_t *coldef, xmmsv_t *val);
gint column_display_render_text (column_display_t *disp, column_def_t *coldef, xmmsv_t *val);
gint column_display_render_time (column_display_t *disp, column_def_t *coldef, xmmsv_t *val);
gint column_display_render_property (column_display_t *disp, column_def_t *coldef, xmmsv_t *val);
gint column_display_render_format (column_display_t *disp, column_def_t *coldef, xmmsv_t *val);

void column_display_set_position (column_display_t *disp, gint pos);

void column_display_set_list_marker (column_display_t *disp, const gchar *marker);

column_display_t *column_display_build (const gchar **columns, const gchar *playlist_marker, gint current_position);

#endif /* __COLUMN_DISPLAY_H__ */
