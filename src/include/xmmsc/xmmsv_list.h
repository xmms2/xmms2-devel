/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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


#ifndef __XMMSV_LIST_H__
#define __XMMSV_LIST_H__

#include <xmmsc/xmmsv_general.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ListType Lists
 * @ingroup ValueType
 * @{
 */

typedef int (*xmmsv_list_compare_func_t)(xmmsv_t **, xmmsv_t **);

xmmsv_t *xmmsv_new_list (void) XMMS_PUBLIC;

int xmmsv_list_get (xmmsv_t *listv, int pos, xmmsv_t **val) XMMS_PUBLIC;
int xmmsv_list_set (xmmsv_t *listv, int pos, xmmsv_t *val) XMMS_PUBLIC;
int xmmsv_list_append (xmmsv_t *listv, xmmsv_t *val) XMMS_PUBLIC;
int xmmsv_list_insert (xmmsv_t *listv, int pos, xmmsv_t *val) XMMS_PUBLIC;
int xmmsv_list_remove (xmmsv_t *listv, int pos) XMMS_PUBLIC;
int xmmsv_list_move (xmmsv_t *listv, int old_pos, int new_pos) XMMS_PUBLIC;
int xmmsv_list_clear (xmmsv_t *listv) XMMS_PUBLIC;
int xmmsv_list_sort (xmmsv_t *listv, xmmsv_list_compare_func_t comparator) XMMS_PUBLIC;
int xmmsv_list_get_size (xmmsv_t *listv) XMMS_PUBLIC;
int xmmsv_list_restrict_type (xmmsv_t *listv, xmmsv_type_t type) XMMS_PUBLIC;
int xmmsv_list_has_type (xmmsv_t *listv, xmmsv_type_t type) XMMS_PUBLIC;
int xmmsv_list_get_type (xmmsv_t *listv, xmmsv_type_t *type) XMMS_PUBLIC;
int xmmsv_list_index_of (xmmsv_t *listv, xmmsv_t *val) XMMS_PUBLIC;

int xmmsv_list_get_string (xmmsv_t *v, int pos, const char **val) XMMS_PUBLIC;
int xmmsv_list_get_int32 (xmmsv_t *v, int pos, int32_t *val) XMMS_PUBLIC;
int xmmsv_list_get_int64 (xmmsv_t *v, int pos, int64_t *val) XMMS_PUBLIC;
int xmmsv_list_get_float (xmmsv_t *v, int pos, float *val) XMMS_PUBLIC;
int xmmsv_list_get_coll (xmmsv_t *v, int pos, xmmsv_t **val) XMMS_PUBLIC XMMS_DEPRECATED;

int xmmsv_list_set_string (xmmsv_t *v, int pos, const char *val) XMMS_PUBLIC;
int xmmsv_list_set_int (xmmsv_t *v, int pos, int64_t val) XMMS_PUBLIC;
int xmmsv_list_set_float (xmmsv_t *v, int pos, float val) XMMS_PUBLIC;
int xmmsv_list_set_coll (xmmsv_t *v, int pos, xmmsv_t *val) XMMS_PUBLIC XMMS_DEPRECATED;

int xmmsv_list_insert_string (xmmsv_t *v, int pos, const char *val) XMMS_PUBLIC;
int xmmsv_list_insert_int (xmmsv_t *v, int pos, int64_t val) XMMS_PUBLIC;
int xmmsv_list_insert_float (xmmsv_t *v, int pos, float val) XMMS_PUBLIC;
int xmmsv_list_insert_coll (xmmsv_t *v, int pos, xmmsv_t *val) XMMS_PUBLIC XMMS_DEPRECATED;

int xmmsv_list_append_string (xmmsv_t *v, const char *val) XMMS_PUBLIC;
int xmmsv_list_append_int (xmmsv_t *v, int64_t val) XMMS_PUBLIC;
int xmmsv_list_append_float (xmmsv_t *v, float val) XMMS_PUBLIC;
int xmmsv_list_append_coll (xmmsv_t *v, xmmsv_t *val) XMMS_PUBLIC XMMS_DEPRECATED;

xmmsv_t *xmmsv_list_flatten (xmmsv_t *list, int depth) XMMS_PUBLIC;

/**
 * @defgroup ListIterType Iteration
 * @{
 */

typedef void (*xmmsv_list_foreach_func) (xmmsv_t *value, void *user_data);
int xmmsv_list_foreach (xmmsv_t *listv, xmmsv_list_foreach_func func, void* user_data) XMMS_PUBLIC;

typedef struct xmmsv_list_iter_St xmmsv_list_iter_t;
int xmmsv_get_list_iter (const xmmsv_t *val, xmmsv_list_iter_t **it) XMMS_PUBLIC;
void xmmsv_list_iter_explicit_destroy (xmmsv_list_iter_t *it) XMMS_PUBLIC;

int  xmmsv_list_iter_entry (xmmsv_list_iter_t *it, xmmsv_t **val) XMMS_PUBLIC;
int  xmmsv_list_iter_valid (xmmsv_list_iter_t *it) XMMS_PUBLIC;
void xmmsv_list_iter_first (xmmsv_list_iter_t *it) XMMS_PUBLIC;
void xmmsv_list_iter_last (xmmsv_list_iter_t *it) XMMS_PUBLIC;
void xmmsv_list_iter_next (xmmsv_list_iter_t *it) XMMS_PUBLIC;
void xmmsv_list_iter_prev (xmmsv_list_iter_t *it) XMMS_PUBLIC;
int  xmmsv_list_iter_seek (xmmsv_list_iter_t *it, int pos) XMMS_PUBLIC;
int  xmmsv_list_iter_tell (const xmmsv_list_iter_t *it) XMMS_PUBLIC;
xmmsv_t *xmmsv_list_iter_get_parent (const xmmsv_list_iter_t *it) XMMS_PUBLIC;

int  xmmsv_list_iter_set (xmmsv_list_iter_t *it, xmmsv_t *val) XMMS_PUBLIC;
int  xmmsv_list_iter_insert (xmmsv_list_iter_t *it, xmmsv_t *val) XMMS_PUBLIC;
int  xmmsv_list_iter_remove (xmmsv_list_iter_t *it) XMMS_PUBLIC;

int xmmsv_list_iter_entry_string (xmmsv_list_iter_t *it, const char **val) XMMS_PUBLIC;
int xmmsv_list_iter_entry_int32 (xmmsv_list_iter_t *it, int32_t *val) XMMS_PUBLIC;
int xmmsv_list_iter_entry_int64 (xmmsv_list_iter_t *it, int64_t *val) XMMS_PUBLIC;
int xmmsv_list_iter_entry_float (xmmsv_list_iter_t *it, float *val) XMMS_PUBLIC;
int xmmsv_list_iter_entry_coll (xmmsv_list_iter_t *it, xmmsv_t **val) XMMS_PUBLIC XMMS_DEPRECATED;

int xmmsv_list_iter_insert_string (xmmsv_list_iter_t *it, const char *val) XMMS_PUBLIC;
int xmmsv_list_iter_insert_int (xmmsv_list_iter_t *it, int64_t val) XMMS_PUBLIC;
int xmmsv_list_iter_insert_float (xmmsv_list_iter_t *it, float val) XMMS_PUBLIC;
int xmmsv_list_iter_insert_coll (xmmsv_list_iter_t *it, xmmsv_t *val) XMMS_PUBLIC XMMS_DEPRECATED;
/** @} */

/* legacy aliases */
#if XMMSV_USE_INT64 == 1
#define xmmsv_list_get_int xmmsv_list_get_int64
#define xmmsv_list_iter_entry_int xmmsv_list_iter_entry_int64
#else
#define xmmsv_list_get_int xmmsv_list_get_int32
#define xmmsv_list_iter_entry_int xmmsv_list_iter_entry_int32
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif
