/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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


#ifndef __XMMSCLIENT_RESULT_H__
#define __XMMSCLIENT_RESULT_H__

#include <xmms/xmmsclient.h>

typedef struct xmmsc_result_St xmmsc_result_t;
typedef void (*xmmsc_result_notifier_t) (xmmsc_result_t *res, void *user_data);

void xmmsc_result_restartable (xmmsc_result_t *res, xmmsc_connection_t *conn, char *signal); 
xmmsc_result_t * xmmsc_result_restart (xmmsc_result_t *res);

void xmmsc_result_ref (xmmsc_result_t *res);
void xmmsc_result_unref (xmmsc_result_t *res);

void xmmsc_result_notifier_set (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data);
void xmmsc_result_wait (xmmsc_result_t *res);

int xmmsc_result_iserror (xmmsc_result_t *res);
const char * xmmsc_result_get_error (xmmsc_result_t *res);

int xmmsc_result_get_int (xmmsc_result_t *res, int *r);
int xmmsc_result_get_uint (xmmsc_result_t *res, unsigned int *r);
int xmmsc_result_get_string (xmmsc_result_t *res, char **r);
int xmmsc_result_get_mediainfo (xmmsc_result_t *res, x_hash_t **r);
int xmmsc_result_get_stringlist (xmmsc_result_t *res, x_list_t **r);
int xmmsc_result_get_intlist (xmmsc_result_t *res, x_list_t **r);
int xmmsc_result_get_uintlist (xmmsc_result_t *res, x_list_t **r);
int xmmsc_result_get_double_array (xmmsc_result_t *res, double **r, int *len);
int xmmsc_result_get_playlist_change (xmmsc_result_t *res, unsigned int *change, unsigned int *id, unsigned int *argument);

#endif
