/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __X_HASH_H__
#define __X_HASH_H__

#include "xutil-int.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _x_hash_t  x_hash_t;

/* Hash tables
 */
x_hash_t* x_hash_new (XHashFunc hash_func, XEqualFunc key_equal_func);
x_hash_t* x_hash_new_full (XHashFunc hash_func, XEqualFunc key_equal_func, XDestroyNotify key_destroy_func, XDestroyNotify value_destroy_func);
void x_hash_destroy (x_hash_t *hash_table);
void x_hash_insert (x_hash_t *hash_table, void *key, void *value);
void x_hash_replace (x_hash_t *hash_table, void *key, void *value);
int x_hash_remove (x_hash_t *hash_table, const void *key);
void *x_hash_lookup (x_hash_t *hash_table, const void *key);
void x_hash_foreach (x_hash_t *hash_table, XHFunc func, void *user_data);
unsigned int x_hash_foreach_remove (x_hash_t *hash_table, XHRFunc func, void *user_data);
unsigned int x_hash_size (x_hash_t *hash_table);

int x_str_equal (const void *v, const void *  v2);
unsigned int x_str_hash (const void *v);
int x_int_equal (const void *v, const void *v2);
unsigned int x_int_hash (const void *v);

unsigned int x_direct_hash (const void *v);
int x_direct_equal (const void *v, const void *v2);

#ifdef __cplusplus
}
#endif

#endif /* __X_HASH_H__ */

