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


void xmmsc_result_ref (xmmsc_result_t *res);
void xmmsc_result_unref (xmmsc_result_t *res);

/**
 * frågan är om det skall vara notifier_set eller notifier_add?
 * kanske kan vara bra att ha flera notifiers ibland...
 */
void xmmsc_result_notifier_set (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data);
void xmmsc_result_wait (xmmsc_result_t *res);

int xmmsc_result_iserror (xmmsc_result_t *res);
const char * xmmsc_result_get_error (xmmsc_result_t *res);

/**
 *
 * ehm? Hur var det med felhantering här?  Vad händer om man gör en
 * xmmsc_result_get_int på en result som innehåller en string?
 *
 */
int xmmsc_result_get_int (xmmsc_result_t *res);
unsigned int xmmsc_result_get_uint (xmmsc_result_t *res);

/* ägs strängen av restult:et eller skall man frigöra den själv? */
char *xmmsc_result_get_string (xmmsc_result_t *res);

/* samma med listan här..
 *
 * och lista av vad?
 * Det är ganska stor skillnad på lista av strängar och
 * lista av strängar eller lista av något annat
 */
x_list_t *xmmsc_result_get_list (xmmsc_result_t *res);
unsigned int *xmmsc_result_get_uint_array (xmmsc_result_t *res);
double *xmmsc_result_get_double_array (xmmsc_result_t *res);
x_hash_t *xmmsc_result_get_mediainfo (xmmsc_result_t *res);

#endif
