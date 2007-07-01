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

#ifndef __COLUMN_DISPLAY_H__
#define __COLUMN_DISPLAY_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdlib.h>

#include "main.h"


typedef struct column_display_St column_display_t;
typedef struct column_def_St column_def_t;


column_display_t *column_display_init (cli_infos_t *infos);
void column_display_fill (column_display_t *disp, const gchar **props);
void column_display_fill_default (column_display_t *disp);
void column_display_free (column_display_t *disp);
cli_infos_t *column_display_infos_get (column_display_t *disp);
void column_display_print (column_display_t *disp, xmmsc_result_t *res);
void column_display_print_header (column_display_t *disp);
void column_display_print_footer (column_display_t *disp);


#endif /* __COLUMN_DISPLAY_H__ */
