/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include <xmmsc/xmmsc_compiler.h>
#include <xmmsc/xmmsc_stdint.h>
#include <xmmsc/xmmsc_idnumbers.h>
#include <xmmsc/xmmsv_general.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef xmmsv_t xmmsv_coll_t XMMS_DEPRECATED;

xmmsv_t *xmmsv_coll_new (xmmsv_coll_type_t type) XMMS_PUBLIC XMMS_DEPRECATED;
xmmsv_t *xmmsv_coll_ref (xmmsv_t *coll) XMMS_PUBLIC XMMS_DEPRECATED;
void xmmsv_coll_unref (xmmsv_t *coll) XMMS_PUBLIC XMMS_DEPRECATED;

void xmmsv_coll_set_idlist (xmmsv_t *coll, int ids[]) XMMS_PUBLIC;
void xmmsv_coll_add_operand (xmmsv_t *coll, xmmsv_t *op) XMMS_PUBLIC;
void xmmsv_coll_remove_operand (xmmsv_t *coll, xmmsv_t *op) XMMS_PUBLIC;
xmmsv_t *xmmsv_coll_operands_get (xmmsv_t *coll) XMMS_PUBLIC;
void xmmsv_coll_operands_set (xmmsv_t *coll, xmmsv_t *operands) XMMS_PUBLIC;

int xmmsv_coll_idlist_append (xmmsv_t *coll, int64_t id) XMMS_PUBLIC;
int xmmsv_coll_idlist_insert (xmmsv_t *coll, int index, int64_t id) XMMS_PUBLIC;
int xmmsv_coll_idlist_move (xmmsv_t *coll, int index, int newindex) XMMS_PUBLIC;
int xmmsv_coll_idlist_remove (xmmsv_t *coll, int index) XMMS_PUBLIC;
int xmmsv_coll_idlist_clear (xmmsv_t *coll) XMMS_PUBLIC;
int xmmsv_coll_idlist_get_index_int32 (xmmsv_t *coll, int index, int32_t *val) XMMS_PUBLIC;
int xmmsv_coll_idlist_get_index_int64 (xmmsv_t *coll, int index, int64_t *val) XMMS_PUBLIC;
int xmmsv_coll_idlist_set_index (xmmsv_t *coll, int index, int64_t val) XMMS_PUBLIC;
int xmmsv_coll_idlist_get_size (xmmsv_t *coll) XMMS_PUBLIC;

int xmmsv_coll_is_type (xmmsv_t *val, xmmsv_coll_type_t t) XMMS_PUBLIC;
xmmsv_coll_type_t xmmsv_coll_get_type (xmmsv_t *coll) XMMS_PUBLIC;
xmmsv_t *xmmsv_coll_idlist_get (xmmsv_t *coll) XMMS_PUBLIC;
void xmmsv_coll_idlist_set (xmmsv_t *coll, xmmsv_t *idlist) XMMS_PUBLIC;

void xmmsv_coll_attribute_set (xmmsv_t *coll, const char *key, const char *value) XMMS_PUBLIC XMMS_DEPRECATED;
void xmmsv_coll_attribute_set_string (xmmsv_t *coll, const char *key, const char *value) XMMS_PUBLIC;
void xmmsv_coll_attribute_set_int (xmmsv_t *coll, const char *key, int64_t value) XMMS_PUBLIC;
void xmmsv_coll_attribute_set_value (xmmsv_t *coll, const char *key, xmmsv_t *value) XMMS_PUBLIC;

int xmmsv_coll_attribute_remove (xmmsv_t *coll, const char *key) XMMS_PUBLIC;

int xmmsv_coll_attribute_get (xmmsv_t *coll, const char *key, const char **value) XMMS_PUBLIC XMMS_DEPRECATED;
int xmmsv_coll_attribute_get_string (xmmsv_t *coll, const char *key, const char **value) XMMS_PUBLIC;
int xmmsv_coll_attribute_get_int32 (xmmsv_t *coll, const char *key, int32_t *value) XMMS_PUBLIC;
int xmmsv_coll_attribute_get_int64 (xmmsv_t *coll, const char *key, int64_t *value) XMMS_PUBLIC;
int xmmsv_coll_attribute_get_value (xmmsv_t *coll, const char *key, xmmsv_t **value) XMMS_PUBLIC;

xmmsv_t *xmmsv_coll_attributes_get (xmmsv_t *coll) XMMS_PUBLIC;
void xmmsv_coll_attributes_set (xmmsv_t *coll, xmmsv_t *attributes) XMMS_PUBLIC;

xmmsv_t *xmmsv_coll_universe (void) XMMS_PUBLIC XMMS_DEPRECATED;

xmmsv_t *xmmsv_coll_add_order_operator (xmmsv_t *coll, xmmsv_t *order) XMMS_PUBLIC;
xmmsv_t *xmmsv_coll_add_order_operators (xmmsv_t *coll, xmmsv_t *order) XMMS_PUBLIC;
xmmsv_t *xmmsv_coll_add_limit_operator (xmmsv_t *coll, int lim_start, int lim_len) XMMS_PUBLIC;


/* compability */
typedef xmmsv_t xmmsc_coll_t;

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

#define xmmsc_coll_attribute_set xmmsv_coll_attribute_set
#define xmmsc_coll_attribute_remove xmmsv_coll_attribute_remove
#define xmmsc_coll_attribute_get xmmsv_coll_attribute_get

#define xmmsc_coll_universe xmmsv_coll_universe

#if XMMSV_USE_INT64 == 1
#define xmmsv_coll_idlist_get_index xmmsv_coll_idlist_get_index_int64
#define xmmsv_coll_attribute_get_int xmmsv_coll_attribute_get_int64
#else
#define xmmsv_coll_idlist_get_index xmmsv_coll_idlist_get_index_int32
#define xmmsv_coll_attribute_get_int xmmsv_coll_attribute_get_int32
#endif

#ifdef __cplusplus
}
#endif

#endif
