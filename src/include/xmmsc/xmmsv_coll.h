/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#include "xmmsc/xmmsc_compiler.h"
#include "xmmsc/xmmsc_stdint.h"
#include "xmmsc/xmmsc_idnumbers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xmmsv_coll_St xmmsv_coll_t;

typedef void (*xmmsv_coll_attribute_foreach_func) (const char *key, const char *value, void *udata);

xmmsv_coll_t *xmmsv_coll_new (xmmsv_coll_type_t type);
xmmsv_coll_t *xmmsv_coll_ref (xmmsv_coll_t *coll);
void xmmsv_coll_unref (xmmsv_coll_t *coll);

void xmmsv_coll_set_idlist (xmmsv_coll_t *coll, unsigned int ids[]);
void xmmsv_coll_add_operand (xmmsv_coll_t *coll, xmmsv_coll_t *op);
void xmmsv_coll_remove_operand (xmmsv_coll_t *coll, xmmsv_coll_t *op);
struct xmmsv_St *xmmsv_coll_operands_get (xmmsv_coll_t *coll);

int xmmsv_coll_idlist_append (xmmsv_coll_t *coll, unsigned int id);
int xmmsv_coll_idlist_insert (xmmsv_coll_t *coll, unsigned int index, unsigned int id);
int xmmsv_coll_idlist_move (xmmsv_coll_t *coll, unsigned int index, unsigned int newindex);
int xmmsv_coll_idlist_remove (xmmsv_coll_t *coll, unsigned int index);
int xmmsv_coll_idlist_clear (xmmsv_coll_t *coll);
int xmmsv_coll_idlist_get_index (xmmsv_coll_t *coll, unsigned int index, uint32_t *val);
int xmmsv_coll_idlist_set_index (xmmsv_coll_t *coll, unsigned int index, uint32_t val);
size_t xmmsv_coll_idlist_get_size (xmmsv_coll_t *coll);

xmmsv_coll_type_t xmmsv_coll_get_type (xmmsv_coll_t *coll);
uint32_t *xmmsv_coll_get_idlist (xmmsv_coll_t *coll);
int xmmsv_coll_operand_list_first (xmmsv_coll_t *coll) XMMS_DEPRECATED;
int xmmsv_coll_operand_list_valid (xmmsv_coll_t *coll) XMMS_DEPRECATED;
int xmmsv_coll_operand_list_entry (xmmsv_coll_t *coll, xmmsv_coll_t **operand) XMMS_DEPRECATED;
int xmmsv_coll_operand_list_next (xmmsv_coll_t *coll) XMMS_DEPRECATED;
int xmmsv_coll_operand_list_save (xmmsv_coll_t *coll) XMMS_DEPRECATED;
int xmmsv_coll_operand_list_restore (xmmsv_coll_t *coll) XMMS_DEPRECATED;
void xmmsv_coll_operand_list_clear (xmmsv_coll_t *coll) XMMS_DEPRECATED;

void xmmsv_coll_attribute_list_first (xmmsv_coll_t *coll) XMMS_DEPRECATED;
int xmmsv_coll_attribute_list_valid (xmmsv_coll_t *coll) XMMS_DEPRECATED;
void xmmsv_coll_attribute_list_entry (xmmsv_coll_t *coll, const char **k, const char **v) XMMS_DEPRECATED;
void xmmsv_coll_attribute_list_next (xmmsv_coll_t *coll) XMMS_DEPRECATED;


void xmmsv_coll_attribute_set (xmmsv_coll_t *coll, const char *key, const char *value);
int xmmsv_coll_attribute_remove (xmmsv_coll_t *coll, const char *key);
int xmmsv_coll_attribute_get (xmmsv_coll_t *coll, const char *key, char **value);
void xmmsv_coll_attribute_foreach (xmmsv_coll_t *coll, xmmsv_coll_attribute_foreach_func func, void *user_data) XMMS_DEPRECATED;

struct xmmsv_St *xmmsv_coll_attributes_get (xmmsv_coll_t *coll);

xmmsv_coll_t *xmmsv_coll_universe (void);



/* compability */
typedef xmmsv_coll_t xmmsc_coll_t;

#define xmmsc_coll_new xmmsv_coll_new
#define xmmsc_coll_ref xmmsv_coll_ref
#define xmmsc_coll_unref xmmsv_coll_unref

#define xmmsc_coll_set_idlist xmmsv_coll_set_idlist
#define xmmsc_coll_add_operand xmmsv_coll_add_operand
#define xmmsc_coll_remove_operand xmmsv_coll_remove_operand

#define xmmsc_coll_idlist_append xmmsv_coll_idlist_append
#define xmmsc_coll_idlist_insert xmmsv_coll_idlist_insert
#define xmmsc_coll_idlist_move xmmsv_coll_idlist_move
#define xmmsc_coll_idlist_remove xmmsv_coll_idlist_remove
#define xmmsc_coll_idlist_clear xmmsv_coll_idlist_clear
#define xmmsc_coll_idlist_get_index xmmsv_coll_idlist_get_index
#define xmmsc_coll_idlist_set_index xmmsv_coll_idlist_set_index
#define xmmsc_coll_idlist_get_size xmmsv_coll_idlist_get_size

#define xmmsc_coll_get_type xmmsv_coll_get_type
#define xmmsc_coll_get_idlist xmmsv_coll_get_idlist
#define xmmsc_coll_operand_list_first xmmsv_coll_operand_list_first
#define xmmsc_coll_operand_list_valid xmmsv_coll_operand_list_valid
#define xmmsc_coll_operand_list_entry xmmsv_coll_operand_list_entry
#define xmmsc_coll_operand_list_next xmmsv_coll_operand_list_next
#define xmmsc_coll_operand_list_save xmmsv_coll_operand_list_save
#define xmmsc_coll_operand_list_restore xmmsv_coll_operand_list_restore
#define xmmsc_coll_operand_list_clear xmmsv_coll_operand_list_clear

#define xmmsc_coll_attribute_list_first xmmsv_coll_attribute_list_first
#define xmmsc_coll_attribute_list_valid xmmsv_coll_attribute_list_valid
#define xmmsc_coll_attribute_list_entry xmmsv_coll_attribute_list_entry
#define xmmsc_coll_attribute_list_next xmmsv_coll_attribute_list_next


#define xmmsc_coll_attribute_set xmmsv_coll_attribute_set
#define xmmsc_coll_attribute_remove xmmsv_coll_attribute_remove
#define xmmsc_coll_attribute_get xmmsv_coll_attribute_get
#define xmmsc_coll_attribute_foreach xmmsv_coll_attribute_foreach

#define xmmsc_coll_universe xmmsv_coll_universe



#ifdef __cplusplus
}
#endif

#endif
