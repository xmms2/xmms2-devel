/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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

#include "xmmsc/xmmsv_general.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DictType Dictionaries
 * @ingroup ValueType
 * @{
 */

xmmsv_t *xmmsv_new_dict (void);
int xmmsv_dict_get (xmmsv_t *dictv, const char *key, xmmsv_t **val);
int xmmsv_dict_set (xmmsv_t *dictv, const char *key, xmmsv_t *val);
int xmmsv_dict_remove (xmmsv_t *dictv, const char *key);
int xmmsv_dict_clear (xmmsv_t *dictv);
int xmmsv_dict_get_size (xmmsv_t *dictv);
int xmmsv_dict_has_key (xmmsv_t *dictv, const char *key);

int xmmsv_dict_entry_get_string (xmmsv_t *val, const char *key, const char **r);
int xmmsv_dict_entry_get_int (xmmsv_t *val, const char *key, int32_t *r);
int xmmsv_dict_entry_get_coll (xmmsv_t *val, const char *key, xmmsv_coll_t **coll);

int xmmsv_dict_set_string (xmmsv_t *val, const char *key, const char *el);
int xmmsv_dict_set_int (xmmsv_t *val, const char *key, int32_t el);
int xmmsv_dict_set_coll (xmmsv_t *val, const char *key, xmmsv_coll_t *el);

/* Utility */
xmmsv_type_t xmmsv_dict_entry_get_type (xmmsv_t *val, const char *key);

/**
 * @defgroup DictIterType Iteration
 * @{
 */
typedef void (*xmmsv_dict_foreach_func) (const char *key, xmmsv_t *value, void *user_data);
int xmmsv_dict_foreach (xmmsv_t *dictv, xmmsv_dict_foreach_func func, void *user_data);

typedef struct xmmsv_dict_iter_St xmmsv_dict_iter_t;
int xmmsv_get_dict_iter (const xmmsv_t *val, xmmsv_dict_iter_t **it);
void xmmsv_dict_iter_explicit_destroy (xmmsv_dict_iter_t *it);

int  xmmsv_dict_iter_pair (xmmsv_dict_iter_t *it, const char **key, xmmsv_t **val);
int  xmmsv_dict_iter_valid (xmmsv_dict_iter_t *it);
void xmmsv_dict_iter_first (xmmsv_dict_iter_t *it);
void xmmsv_dict_iter_next (xmmsv_dict_iter_t *it);
int  xmmsv_dict_iter_find (xmmsv_dict_iter_t *it, const char *key);

int  xmmsv_dict_iter_set (xmmsv_dict_iter_t *it, xmmsv_t *val);
int  xmmsv_dict_iter_remove (xmmsv_dict_iter_t *it);

int xmmsv_dict_iter_pair_string (xmmsv_dict_iter_t *it, const char **key, const char **r);
int xmmsv_dict_iter_pair_int (xmmsv_dict_iter_t *it, const char **key, int32_t *r);
int xmmsv_dict_iter_pair_coll (xmmsv_dict_iter_t *it, const char **key, xmmsv_coll_t **r);

int xmmsv_dict_iter_set_string (xmmsv_dict_iter_t *it, const char *elem);
int xmmsv_dict_iter_set_int (xmmsv_dict_iter_t *it, int32_t elem);
int xmmsv_dict_iter_set_coll (xmmsv_dict_iter_t *it, xmmsv_coll_t *elem);
/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif
