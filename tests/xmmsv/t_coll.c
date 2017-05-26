/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#include "xcu.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <xmmsc/xmmsv_coll.h>
#include <xmmsc/xmmsv.h>

SETUP (coll) {
	return 0;
}

CLEANUP () {
	return 0;
}

CASE (test_coll_simple_types)
{
	xmmsv_t *c;

	c = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	CU_ASSERT_PTR_NOT_NULL (c);
	CU_ASSERT_EQUAL (xmmsv_coll_get_type (c), XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_unref (c);

	c = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNION);
	CU_ASSERT_PTR_NOT_NULL (c);
	CU_ASSERT_EQUAL (xmmsv_coll_get_type (c), XMMS_COLLECTION_TYPE_UNION);
	xmmsv_unref (c);

	c = xmmsv_new_coll (XMMS_COLLECTION_TYPE_INTERSECTION);
	CU_ASSERT_PTR_NOT_NULL (c);
	CU_ASSERT_EQUAL (xmmsv_coll_get_type (c), XMMS_COLLECTION_TYPE_INTERSECTION);
	xmmsv_unref (c);

	c = xmmsv_new_coll (XMMS_COLLECTION_TYPE_COMPLEMENT);
	CU_ASSERT_PTR_NOT_NULL (c);
	CU_ASSERT_EQUAL (xmmsv_coll_get_type (c), XMMS_COLLECTION_TYPE_COMPLEMENT);
	xmmsv_unref (c);

	c = xmmsv_new_coll (XMMS_COLLECTION_TYPE_EQUALS);
	CU_ASSERT_PTR_NOT_NULL (c);
	CU_ASSERT_EQUAL (xmmsv_coll_get_type (c), XMMS_COLLECTION_TYPE_EQUALS);
	xmmsv_unref (c);

	c = xmmsv_new_coll (XMMS_COLLECTION_TYPE_IDLIST);
	CU_ASSERT_PTR_NOT_NULL (c);
	CU_ASSERT_EQUAL (xmmsv_coll_get_type (c), XMMS_COLLECTION_TYPE_IDLIST);
	xmmsv_unref (c);

	c = xmmsv_new_coll (1000);
	CU_ASSERT_PTR_NULL (c);
	if (c)
		xmmsv_unref (c);

	c = xmmsv_new_coll (-1);
	CU_ASSERT_PTR_NULL (c);
	if (c)
		xmmsv_unref (c);
}

CASE (test_coll_universe)
{
	xmmsv_t *c;

	c = xmmsv_coll_universe ();
	CU_ASSERT_PTR_NOT_NULL (c);

	CU_ASSERT_EQUAL (xmmsv_coll_get_type (c), XMMS_COLLECTION_TYPE_UNIVERSE);

	xmmsv_unref (c);
}

/* #2109 */
CASE (test_coll_operands_list_clear)
{
	xmmsv_t *u;

	u = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNION);
	CU_ASSERT_PTR_NOT_NULL (u);

	xmmsv_list_clear (xmmsv_coll_operands_get (u));

	xmmsv_unref (u);
}

CASE (test_coll_operands)
{
	xmmsv_t *u;
	xmmsv_t *ops[10];
	int i;

	u = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNION);
	CU_ASSERT_PTR_NOT_NULL (u);

	for (i = 0; i < 10; i++) {
		ops[i] = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
		CU_ASSERT_PTR_NOT_NULL (ops[i]);
	}

	for (i = 0; i < 10; i++) {
		xmmsv_coll_add_operand (u, ops[i]);
	}

	CU_ASSERT_EQUAL (xmmsv_list_get_size (xmmsv_coll_operands_get (u)), 10);

	for (i = 0; i < 10; i++) {
		xmmsv_unref (ops[i]);
	}

	xmmsv_list_clear (xmmsv_coll_operands_get (u));

	xmmsv_unref (u);
}

CASE (test_coll_operands_list)
{
	xmmsv_t *opl;
	xmmsv_t *u;
	xmmsv_t *t;
	int i;

	u = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNION);
	CU_ASSERT_PTR_NOT_NULL (u);

	opl = xmmsv_coll_operands_get (u);
	CU_ASSERT_PTR_NOT_NULL (opl);

	t = xmmsv_new_int (1);

	CU_ASSERT_FALSE (xmmsv_list_append (opl, t));

	for (i = 0; i < 10; i++) {
		xmmsv_t *o;
		o = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
		CU_ASSERT_PTR_NOT_NULL (o);
		CU_ASSERT_TRUE (xmmsv_list_append (opl, o));
		xmmsv_unref (o);
	}

	CU_ASSERT_EQUAL (xmmsv_list_get_size (xmmsv_coll_operands_get (u)), 10);

	xmmsv_list_clear (xmmsv_coll_operands_get (u));

	xmmsv_unref (u);

	xmmsv_unref (t);
}

CASE (test_coll_attributes)
{
	const char *v;
	xmmsv_t *c;
	int cnt;
	int sum;
	xmmsv_dict_iter_t *it;

	c = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	CU_ASSERT_PTR_NOT_NULL (c);

	xmmsv_coll_attribute_set_string (c, "k1", "v1");
	xmmsv_coll_attribute_set_string (c, "k2", "v2");
	xmmsv_coll_attribute_set_string (c, "k3", "v3");

	CU_ASSERT_TRUE (xmmsv_coll_attribute_get_string (c, "k1", &v));
	CU_ASSERT_STRING_EQUAL (v, "v1");
	CU_ASSERT_TRUE (xmmsv_coll_attribute_get_string (c, "k2", &v));
	CU_ASSERT_STRING_EQUAL (v, "v2");
	CU_ASSERT_TRUE (xmmsv_coll_attribute_get_string (c, "k3", &v));
	CU_ASSERT_STRING_EQUAL (v, "v3");

	/* non existent key */
	CU_ASSERT_FALSE (xmmsv_coll_attribute_get_string (c, "k4", &v));

	cnt = 0;
	sum = 0;
	CU_ASSERT_TRUE (xmmsv_get_dict_iter (xmmsv_coll_attributes_get (c), &it));
	for (xmmsv_dict_iter_first (it);
	     xmmsv_dict_iter_valid (it);
	     xmmsv_dict_iter_next (it)) {
		const char *k;
		const char *v;

		CU_ASSERT_TRUE (xmmsv_dict_iter_pair_string (it, &k, &v));
		CU_ASSERT_EQUAL (k[1], v[1]);
		sum += atoi (k + 1);
		cnt++;
	}
	CU_ASSERT_EQUAL (cnt, 3);
	CU_ASSERT_EQUAL (sum, 1+2+3);

	CU_ASSERT_EQUAL (xmmsv_dict_get_size (xmmsv_coll_attributes_get (c)), 3);

	/* replace */
	xmmsv_coll_attribute_set_string (c, "k2", "v2new");
	CU_ASSERT_TRUE (xmmsv_coll_attribute_get_string (c, "k2", &v));
	CU_ASSERT_STRING_EQUAL (v, "v2new");

	xmmsv_unref (c);
}

CASE (test_coll_idlist)
{
	xmmsv_t *c;
	int32_t v;
	int i;

	c = xmmsv_new_coll (XMMS_COLLECTION_TYPE_IDLIST);

	for (i = 0; i < 100; i++) {
		xmmsv_coll_idlist_append (c, i);
	}

	for (i = 0; i < 100; i++) {
		CU_ASSERT_TRUE (xmmsv_coll_idlist_get_index (c, i, &v));
		CU_ASSERT_EQUAL (i, (int)v);
	}

	CU_ASSERT_FALSE (xmmsv_coll_idlist_get_index (c, 100, &v));

	CU_ASSERT_TRUE (xmmsv_coll_idlist_get_index (c, -1, &v));
	CU_ASSERT_EQUAL (99, (int)v);

	CU_ASSERT_EQUAL (xmmsv_coll_idlist_get_size (c), 100);

	CU_ASSERT_TRUE (xmmsv_coll_idlist_remove (c, 0));
	CU_ASSERT_FALSE (xmmsv_coll_idlist_get_index (c, 99, &v));

	for (i = 0; i < 99; i++) {
		CU_ASSERT_TRUE (xmmsv_coll_idlist_get_index (c, i, &v));
		CU_ASSERT_EQUAL (i + 1, (int)v);
	}

	CU_ASSERT_TRUE (xmmsv_coll_idlist_clear (c));
	CU_ASSERT_FALSE (xmmsv_coll_idlist_get_index (c, 0, &v));

	xmmsv_unref (c);
}
