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

#ifndef __XMMS_COLL_H__
#define __XMMS_COLL_H__

#include "xmmsc/xmmsc_stdint.h"
#include "xmmsc/xmmsc_idnumbers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xmmsc_coll_St xmmsc_coll_t;

typedef void (*xmmsc_coll_attribute_foreach_func) (const char *key, const char *value, void *udata);

xmmsc_coll_t* xmmsc_coll_new (xmmsc_coll_type_t type);
void xmmsc_coll_ref (xmmsc_coll_t *coll);
void xmmsc_coll_unref (xmmsc_coll_t *coll);

void xmmsc_coll_set_idlist (xmmsc_coll_t *coll, unsigned int ids[]);
void xmmsc_coll_add_operand (xmmsc_coll_t *coll, xmmsc_coll_t *op);
void xmmsc_coll_remove_operand (xmmsc_coll_t *coll, xmmsc_coll_t *op);

int xmmsc_coll_idlist_append (xmmsc_coll_t *coll, unsigned int id);
int xmmsc_coll_idlist_insert (xmmsc_coll_t *coll, unsigned int index, unsigned int id);
int xmmsc_coll_idlist_move (xmmsc_coll_t *coll, unsigned int index, unsigned int newindex);
int xmmsc_coll_idlist_remove (xmmsc_coll_t *coll, unsigned int index);
int xmmsc_coll_idlist_clear (xmmsc_coll_t *coll);
int xmmsc_coll_idlist_get_index (xmmsc_coll_t *coll, unsigned int index, uint32_t *val);
int xmmsc_coll_idlist_set_index (xmmsc_coll_t *coll, unsigned int index, uint32_t val);
size_t xmmsc_coll_idlist_get_size (xmmsc_coll_t *coll);

xmmsc_coll_type_t xmmsc_coll_get_type (xmmsc_coll_t *coll);
uint32_t* xmmsc_coll_get_idlist (xmmsc_coll_t *coll);
int xmmsc_coll_operand_list_first (xmmsc_coll_t *coll);
int xmmsc_coll_operand_list_valid (xmmsc_coll_t *coll);
int xmmsc_coll_operand_list_entry (xmmsc_coll_t *coll, xmmsc_coll_t **operand);
int xmmsc_coll_operand_list_next (xmmsc_coll_t *coll);
int xmmsc_coll_operand_list_save (xmmsc_coll_t *coll);
int xmmsc_coll_operand_list_restore (xmmsc_coll_t *coll);
void xmmsc_coll_operand_list_clear (xmmsc_coll_t *coll);

void xmmsc_coll_attribute_list_first (xmmsc_coll_t *coll);
int xmmsc_coll_attribute_list_valid (xmmsc_coll_t *coll);
void xmmsc_coll_attribute_list_entry (xmmsc_coll_t *coll, const char **k, const char **v);
void xmmsc_coll_attribute_list_next (xmmsc_coll_t *coll);


void xmmsc_coll_attribute_set (xmmsc_coll_t *coll, const char *key, const char *value);
int xmmsc_coll_attribute_remove (xmmsc_coll_t *coll, const char *key);
int xmmsc_coll_attribute_get (xmmsc_coll_t *coll, const char *key, char **value);
void xmmsc_coll_attribute_foreach (xmmsc_coll_t *coll, xmmsc_coll_attribute_foreach_func func, void *user_data);

xmmsc_coll_t* xmmsc_coll_universe ();

#ifdef __cplusplus
}
#endif

#endif 
