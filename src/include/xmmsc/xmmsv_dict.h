/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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


#ifndef __XMMSV_DICT_H__
#define __XMMSV_DICT_H__

#include <xmmsc/xmmsv_general.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DictType Dictionaries
 * @ingroup ValueType
 * @{
 */

xmmsv_t *xmmsv_new_dict (void) XMMS_PUBLIC;
int xmmsv_dict_get (xmmsv_t *dictv, const char *key, xmmsv_t **val) XMMS_PUBLIC;
int xmmsv_dict_set (xmmsv_t *dictv, const char *key, xmmsv_t *val) XMMS_PUBLIC;
int xmmsv_dict_remove (xmmsv_t *dictv, const char *key) XMMS_PUBLIC;
int xmmsv_dict_clear (xmmsv_t *dictv) XMMS_PUBLIC;
int xmmsv_dict_get_size (xmmsv_t *dictv) XMMS_PUBLIC;
int xmmsv_dict_has_key (xmmsv_t *dictv, const char *key) XMMS_PUBLIC;

int xmmsv_dict_entry_get_string (xmmsv_t *val, const char *key, const char **r) XMMS_PUBLIC;
int xmmsv_dict_entry_get_int32 (xmmsv_t *val, const char *key, int32_t *r) XMMS_PUBLIC;
int xmmsv_dict_entry_get_int64 (xmmsv_t *val, const char *key, int64_t *r) XMMS_PUBLIC;
int xmmsv_dict_entry_get_float (xmmsv_t *val, const char *key, float *r) XMMS_PUBLIC;
int xmmsv_dict_entry_get_coll (xmmsv_t *val, const char *key, xmmsv_t **coll) XMMS_PUBLIC XMMS_DEPRECATED;

int xmmsv_dict_set_string (xmmsv_t *val, const char *key, const char *el) XMMS_PUBLIC;
int xmmsv_dict_set_int (xmmsv_t *val, const char *key, int64_t el) XMMS_PUBLIC;
int xmmsv_dict_set_float (xmmsv_t *val, const char *key, float el) XMMS_PUBLIC;
int xmmsv_dict_set_coll (xmmsv_t *val, const char *key, xmmsv_t *el) XMMS_PUBLIC XMMS_DEPRECATED;

/* Utility */
xmmsv_type_t xmmsv_dict_entry_get_type (xmmsv_t *val, const char *key) XMMS_PUBLIC;

/**
 * @defgroup DictIterType Iteration
 * @{
 */
typedef void (*xmmsv_dict_foreach_func) (const char *key, xmmsv_t *value, void *user_data);
int xmmsv_dict_foreach (xmmsv_t *dictv, xmmsv_dict_foreach_func func, void *user_data) XMMS_PUBLIC;

typedef struct xmmsv_dict_iter_St xmmsv_dict_iter_t;
int xmmsv_get_dict_iter (const xmmsv_t *val, xmmsv_dict_iter_t **it) XMMS_PUBLIC;
void xmmsv_dict_iter_explicit_destroy (xmmsv_dict_iter_t *it) XMMS_PUBLIC;

int  xmmsv_dict_iter_pair (xmmsv_dict_iter_t *it, const char **key, xmmsv_t **val) XMMS_PUBLIC;
int  xmmsv_dict_iter_valid (xmmsv_dict_iter_t *it) XMMS_PUBLIC;
void xmmsv_dict_iter_first (xmmsv_dict_iter_t *it) XMMS_PUBLIC;
void xmmsv_dict_iter_next (xmmsv_dict_iter_t *it) XMMS_PUBLIC;
int  xmmsv_dict_iter_find (xmmsv_dict_iter_t *it, const char *key) XMMS_PUBLIC;

int  xmmsv_dict_iter_set (xmmsv_dict_iter_t *it, xmmsv_t *val) XMMS_PUBLIC;
int  xmmsv_dict_iter_remove (xmmsv_dict_iter_t *it) XMMS_PUBLIC;

int xmmsv_dict_iter_pair_string (xmmsv_dict_iter_t *it, const char **key, const char **r) XMMS_PUBLIC;
int xmmsv_dict_iter_pair_int32 (xmmsv_dict_iter_t *it, const char **key, int32_t *r) XMMS_PUBLIC;
int xmmsv_dict_iter_pair_int64 (xmmsv_dict_iter_t *it, const char **key, int64_t *r) XMMS_PUBLIC;
int xmmsv_dict_iter_pair_float (xmmsv_dict_iter_t *it, const char **key, float *r) XMMS_PUBLIC;
int xmmsv_dict_iter_pair_coll (xmmsv_dict_iter_t *it, const char **key, xmmsv_t **r) XMMS_PUBLIC XMMS_DEPRECATED;

int xmmsv_dict_iter_set_string (xmmsv_dict_iter_t *it, const char *elem) XMMS_PUBLIC;
int xmmsv_dict_iter_set_int (xmmsv_dict_iter_t *it, int64_t elem) XMMS_PUBLIC;
int xmmsv_dict_iter_set_float (xmmsv_dict_iter_t *it, float elem) XMMS_PUBLIC;
int xmmsv_dict_iter_set_coll (xmmsv_dict_iter_t *it, xmmsv_t *elem) XMMS_PUBLIC XMMS_DEPRECATED;
/** @} */

#if XMMSV_USE_INT64 == 1
#define xmmsv_dict_entry_get_int xmmsv_dict_entry_get_int64
#define xmmsv_dict_iter_pair_int xmmsv_dict_iter_pair_int64
#else
#define xmmsv_dict_entry_get_int xmmsv_dict_entry_get_int32
#define xmmsv_dict_iter_pair_int xmmsv_dict_iter_pair_int32
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif
