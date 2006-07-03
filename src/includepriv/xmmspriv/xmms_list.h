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

#ifndef __X_LIST_H__
#define __X_LIST_H__

#include "xmmsclientpriv/xmmsclient_util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _x_list_t x_list_t;

struct _x_list_t
{
  void * data;
  x_list_t *next;
  x_list_t *prev;
};

/* Doubly linked lists
 */
x_list_t*   x_list_alloc          (void);
void     x_list_free           (x_list_t            *list);
void     x_list_free_1         (x_list_t            *list);
x_list_t*   x_list_append         (x_list_t            *list,
				void *          data);
x_list_t*   x_list_prepend        (x_list_t            *list,
				void *          data);
x_list_t*   x_list_insert         (x_list_t            *list,
				void *          data,
				int              position);
x_list_t*   x_list_insert_sorted  (x_list_t            *list,
				void *          data,
				XCompareFunc      func);
x_list_t*   x_list_insert_before  (x_list_t            *list,
				x_list_t            *sibling,
				void *          data);
x_list_t*   x_list_concat         (x_list_t            *list1,
				x_list_t            *list2);
x_list_t*   x_list_remove         (x_list_t            *list,
				const void *     data);
x_list_t*   x_list_remove_all     (x_list_t            *list,
				const void *     data);
x_list_t*   x_list_remove_link    (x_list_t            *list,
				x_list_t            *llink);
x_list_t*   x_list_delete_link    (x_list_t            *list,
				x_list_t            *link_);
x_list_t*   x_list_reverse        (x_list_t            *list);
x_list_t*   x_list_copy           (x_list_t            *list);
x_list_t*   x_list_nth            (x_list_t            *list,
				unsigned int             n);
x_list_t*   x_list_nth_prev       (x_list_t            *list,
				unsigned int             n);
x_list_t*   x_list_find           (x_list_t            *list,
				const void *     data);
x_list_t*   x_list_find_custom    (x_list_t            *list,
				const void *     data,
				XCompareFunc      func);
int     x_list_position       (x_list_t            *list,
				x_list_t            *llink);
int     x_list_index          (x_list_t            *list,
				const void *     data);
x_list_t*   x_list_last           (x_list_t            *list);
x_list_t*   x_list_first          (x_list_t            *list);
unsigned int    x_list_length         (x_list_t            *list);
void     x_list_foreach        (x_list_t            *list,
				XFunc             func,
				void *          user_data);
void * x_list_nth_data       (x_list_t            *list,
				unsigned int             n);

#define x_list_previous(list)	((list) ? (((x_list_t *)(list))->prev) : NULL)
#define x_list_next(list)	((list) ? (((x_list_t *)(list))->next) : NULL)

#ifdef __cplusplus
}
#endif

#endif /* __X_LIST_H__ */

