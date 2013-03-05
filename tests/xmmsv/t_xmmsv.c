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

#include "xcu.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <xmmsc/xmmsv.h>

SETUP (xmmsv) {
	return 0;
}

CLEANUP () {
	return 0;
}

CASE (test_xmmsv_type_none)
{
	xmmsv_t *value = xmmsv_new_none ();
	CU_ASSERT_EQUAL (XMMSV_TYPE_NONE, xmmsv_get_type (value));
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_NONE));
	xmmsv_unref (value);
}

CASE (test_xmmsv_type_error)
{
	const char *b, *a = "look behind you! a three headed monkey!";
	xmmsv_t *value;

	value = xmmsv_new_error (a);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_ERROR));

	CU_ASSERT_EQUAL (XMMSV_TYPE_ERROR, xmmsv_get_type (value));
	CU_ASSERT_TRUE (xmmsv_is_error (value));
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_ERROR));
	CU_ASSERT_TRUE (xmmsv_get_error (value, &b));
	CU_ASSERT_STRING_EQUAL (a, b);

	xmmsv_unref (value);

	value = xmmsv_new_int (0);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_INT32));
	CU_ASSERT_FALSE (xmmsv_get_error (value, &b));
	xmmsv_unref (value);
}

CASE (test_xmmsv_type_int32)
{
	int b, a = INT_MAX;
	xmmsv_t *value;

	value = xmmsv_new_int (a);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_INT32));

	CU_ASSERT_EQUAL (XMMSV_TYPE_INT32, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));
	CU_ASSERT_TRUE (xmmsv_get_int (value, &b));
	CU_ASSERT_EQUAL (a, b);

	xmmsv_unref (value);

	value = xmmsv_new_error ("oh noes");
	CU_ASSERT_FALSE (xmmsv_get_int (value, &b));
	xmmsv_unref (value);
}

CASE (test_xmmsv_type_string)
{
	const char *b, *a = "look behind you! a three headed monkey!";
	xmmsv_t *value;

	value = xmmsv_new_string (a);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_STRING));

	CU_ASSERT_EQUAL (XMMSV_TYPE_STRING, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));
	CU_ASSERT_TRUE (xmmsv_get_string (value, &b));
	CU_ASSERT_STRING_EQUAL (a, b);

	xmmsv_unref (value);

	value = xmmsv_new_error ("oh noes");
	CU_ASSERT_FALSE (xmmsv_get_string (value, &b));
	xmmsv_unref (value);
}

CASE (test_xmmsv_type_coll)
{
	xmmsv_t *b;
	xmmsv_t *value;

	value = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_COLL));

	CU_ASSERT_EQUAL (XMMSV_TYPE_COLL, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));
	CU_ASSERT_TRUE (xmmsv_get_coll (value, &b));
	CU_ASSERT_TRUE (value == b);

	xmmsv_unref (value);
}


CASE (test_xmmsv_type_coll_wrong_type)
{
	xmmsv_t *value, *b;

	value = xmmsv_new_error ("oh noes");
	CU_ASSERT_FALSE (xmmsv_get_coll (value, &b));
	xmmsv_unref (value);
}

CASE (test_xmmsv_type_bin)
{
	const unsigned char *b;
	const unsigned char *a =
		(const unsigned char *)"look behind you! a three headed monkey!";
	unsigned int b_len, a_len = strlen ((const char *)a) + 1;
	xmmsv_t *value;

	value = xmmsv_new_bin (a, a_len);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_BIN));

	CU_ASSERT_EQUAL (XMMSV_TYPE_BIN, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));
	CU_ASSERT_TRUE (xmmsv_get_bin (value, &b, &b_len));
	CU_ASSERT_EQUAL (a_len, b_len);
	CU_ASSERT_STRING_EQUAL (a, b);

	xmmsv_unref (value);
}

CASE (test_xmmsv_type_bin_wrong_type)
{
	const unsigned char *b;
	unsigned int b_len;
	xmmsv_t *value;

	value = xmmsv_new_error ("oh noes");
	CU_ASSERT_FALSE (xmmsv_get_bin (value, &b, &b_len));
	xmmsv_unref (value);
}

static void _list_foreach (xmmsv_t *value, void *udata)
{
	CU_ASSERT_EQUAL (xmmsv_get_type (value), XMMSV_TYPE_INT32);
}

CASE (test_xmmsv_type_list)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *value, *tmp;
	int i, j;

	value = xmmsv_new_list ();
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_LIST));

	CU_ASSERT_EQUAL (XMMSV_TYPE_LIST, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));

	tmp = xmmsv_new_int (1);
	CU_ASSERT_FALSE (xmmsv_is_type (tmp, XMMSV_TYPE_LIST));
	xmmsv_unref (tmp);

	for (i = 20; i < 40; i++) {
		tmp = xmmsv_new_int (i);
		CU_ASSERT_TRUE (xmmsv_list_append (value, tmp));
		xmmsv_unref (tmp);
	}

	/* { 20, ... 39 } */
	CU_ASSERT_TRUE (xmmsv_list_get (value, -1, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 39);

	CU_ASSERT_TRUE (xmmsv_list_get (value, 0, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 20);

	CU_ASSERT_TRUE (xmmsv_list_get (value, -20, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 20);

	CU_ASSERT_FALSE (xmmsv_list_get (value, -21, &tmp));

	CU_ASSERT_TRUE (xmmsv_list_remove (value, 0));
	CU_ASSERT_FALSE (xmmsv_list_remove (value, 1000));

	/* { 21, ... 39 } */
	CU_ASSERT_TRUE (xmmsv_list_get (value, 0, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 21);

	CU_ASSERT_TRUE (xmmsv_list_get_int (value, 2, &i));
	CU_ASSERT_EQUAL (i, 23);

	CU_ASSERT_FALSE (xmmsv_list_get (value, 1000, &tmp));

	tmp = xmmsv_new_int (20);
	CU_ASSERT_TRUE (xmmsv_list_insert (value, 0, tmp));
	CU_ASSERT_FALSE (xmmsv_list_insert (value, 1000, tmp));
	xmmsv_unref (tmp);

	/* { 20, ... 39 } */

	for (i = 19; i >= 10; i--) {
		tmp = xmmsv_new_int (i);
		CU_ASSERT_TRUE (xmmsv_list_insert (value, 0, tmp));
		xmmsv_unref (tmp);
	}

	/* { 10, ... 39 } */

	CU_ASSERT_TRUE (xmmsv_get_list_iter (value, &it));

	for (i = 9; i >= 0; i--) {
		tmp = xmmsv_new_int (i);
		CU_ASSERT_TRUE (xmmsv_list_iter_insert (it, tmp));
		xmmsv_unref (tmp);
	}

	/* { 0, ... 39 } */

	CU_ASSERT_FALSE (xmmsv_list_iter_seek (it, 1000));
	CU_ASSERT_TRUE (xmmsv_list_iter_seek (it, -1));
	CU_ASSERT_TRUE (xmmsv_list_iter_entry (it, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &j));
	CU_ASSERT_EQUAL (40 - 1, j);
	CU_ASSERT_TRUE (xmmsv_list_iter_seek (it, 0));

	xmmsv_list_iter_entry_int (it, &j);
	CU_ASSERT_EQUAL (0, j);

	for (i = 0; i < 40; i++) {
		CU_ASSERT_TRUE (xmmsv_list_iter_entry (it, &tmp));
		CU_ASSERT_TRUE (xmmsv_get_int (tmp, &j));
		CU_ASSERT_EQUAL (i, j);

		xmmsv_list_iter_next (it);
	}
	CU_ASSERT_FALSE (xmmsv_list_iter_valid (it));

	xmmsv_list_iter_first (it);

	i = 0;
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_remove (it);
		i++;
	}
	CU_ASSERT_EQUAL (i, 40);

	xmmsv_list_iter_first (it);
	CU_ASSERT_FALSE (xmmsv_list_iter_valid (it));

	for (i = 9; i >= 0; i--) {
		tmp = xmmsv_new_int (i);
		CU_ASSERT_TRUE (xmmsv_list_iter_insert (it, tmp));
		xmmsv_unref (tmp);
	}

	/* make sure that remove -1 works */
	CU_ASSERT_TRUE (xmmsv_list_remove (value, -1));
	/* now last element should be 8 */
	CU_ASSERT_TRUE (xmmsv_list_get (value, -1, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 8);

	CU_ASSERT_TRUE (xmmsv_list_clear (value));
	CU_ASSERT_EQUAL (xmmsv_list_get_size (value), 0);
	CU_ASSERT_TRUE (xmmsv_list_clear (value));
	xmmsv_list_iter_first (it);
	CU_ASSERT_FALSE (xmmsv_list_iter_valid (it));
	CU_ASSERT_FALSE (xmmsv_list_iter_remove (it));

	for (i = 9; i >= 0; i--) {
		tmp = xmmsv_new_int (i);
		CU_ASSERT_TRUE (xmmsv_list_iter_insert (it, tmp));
		xmmsv_unref (tmp);
	}

	CU_ASSERT_TRUE (xmmsv_list_foreach (value, _list_foreach, NULL));

	/* list_iter_last */
	xmmsv_list_iter_last (it);
	CU_ASSERT_TRUE (xmmsv_list_iter_entry (it, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 9);

	/* list_iter_prev */
	for (i = 9, xmmsv_list_iter_last (it);
	    i >= 0;
	    i--, xmmsv_list_iter_prev (it)) {

		CU_ASSERT_TRUE (xmmsv_list_iter_entry (it, &tmp));
		CU_ASSERT_TRUE (xmmsv_get_int (tmp, &j));
		CU_ASSERT_EQUAL (i, j);
	}
	CU_ASSERT_FALSE (xmmsv_list_iter_valid (it));

	xmmsv_list_iter_explicit_destroy (it);

	xmmsv_unref (value);
}

static int
list_compare_int (xmmsv_t **a, xmmsv_t **b)
{
	int va, vb;
	va = vb = -1;
	xmmsv_get_int (*a, &va);
	xmmsv_get_int (*b, &vb);
	return va - vb;
}

static int
list_compare_string (xmmsv_t **a, xmmsv_t **b)
{
	const char *va, *vb;
	va = vb = NULL;
	xmmsv_get_string (*a, &va);
	xmmsv_get_string (*b, &vb);
	return strcmp (va, vb);
}

CASE (test_xmmsv_type_list_sort)
{
	xmmsv_t *list;
	const char *sval;
	int32_t ival;

	list = xmmsv_build_list (XMMSV_LIST_ENTRY_INT (3),
	                         XMMSV_LIST_ENTRY_INT (2),
	                         XMMSV_LIST_ENTRY_INT (4),
	                         XMMSV_LIST_ENTRY_INT (1),
	                         XMMSV_LIST_END);

	xmmsv_list_sort (list, list_compare_int);
	CU_ASSERT_TRUE (xmmsv_list_get_int (list, 0, &ival) && ival == 1);
	CU_ASSERT_TRUE (xmmsv_list_get_int (list, 1, &ival) && ival == 2);
	CU_ASSERT_TRUE (xmmsv_list_get_int (list, 2, &ival) && ival == 3);
	CU_ASSERT_TRUE (xmmsv_list_get_int (list, 3, &ival) && ival == 4);
	xmmsv_unref (list);

	list = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("c"),
	                         XMMSV_LIST_ENTRY_STR ("b"),
	                         XMMSV_LIST_ENTRY_STR ("d"),
	                         XMMSV_LIST_ENTRY_STR ("a"),
	                         XMMSV_LIST_END);

	xmmsv_list_sort (list, list_compare_string);
	CU_ASSERT_TRUE (xmmsv_list_get_string (list, 0, &sval) && *sval == 'a');
	CU_ASSERT_TRUE (xmmsv_list_get_string (list, 1, &sval) && *sval == 'b');
	CU_ASSERT_TRUE (xmmsv_list_get_string (list, 2, &sval) && *sval == 'c');
	CU_ASSERT_TRUE (xmmsv_list_get_string (list, 3, &sval) && *sval == 'd');
	xmmsv_unref (list);
}

CASE (test_xmmsv_type_list_iter_validity) {
	xmmsv_list_iter_t *it;
	xmmsv_t *value, *tmp;
	int i;

	/* create 2 element list */
	value = xmmsv_new_list ();

	tmp = xmmsv_new_int (0);
	CU_ASSERT_TRUE (xmmsv_list_append (value, tmp));
	xmmsv_unref (tmp);

	tmp = xmmsv_new_int (1);
	CU_ASSERT_TRUE (xmmsv_list_append (value, tmp));
	xmmsv_unref (tmp);



	CU_ASSERT_TRUE (xmmsv_get_list_iter (value, &it));

	xmmsv_list_iter_first (it);
	xmmsv_list_iter_next (it);

	CU_ASSERT_TRUE (xmmsv_list_iter_valid (it));
	CU_ASSERT_TRUE (xmmsv_list_iter_entry (it, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 1);

	xmmsv_list_iter_next (it);
	CU_ASSERT_FALSE (xmmsv_list_iter_valid (it));

	xmmsv_list_iter_next (it);
	CU_ASSERT_FALSE (xmmsv_list_iter_valid (it));

	xmmsv_list_iter_prev (it);
	CU_ASSERT_TRUE (xmmsv_list_iter_valid (it));
	CU_ASSERT_TRUE (xmmsv_list_iter_entry (it, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 1);

	xmmsv_list_iter_prev (it);
	CU_ASSERT_TRUE (xmmsv_list_iter_valid (it));

	xmmsv_list_iter_prev (it);
	CU_ASSERT_FALSE (xmmsv_list_iter_valid (it));

	xmmsv_list_iter_prev (it);
	CU_ASSERT_FALSE (xmmsv_list_iter_valid (it));

	xmmsv_list_iter_next (it);
	CU_ASSERT_TRUE (xmmsv_list_iter_valid (it));


	xmmsv_unref (value);
}

CASE (test_xmmsv_type_list_iter_on_non_list) {
        xmmsv_list_iter_t *it;
        xmmsv_t *value;

        value = xmmsv_new_error ("oh noes");
        CU_ASSERT_FALSE (xmmsv_get_list_iter (value, &it));
        xmmsv_unref (value);
}

/* #2103 */
CASE (test_xmmsv_type_list_get_on_empty_list) {
	xmmsv_t *value, *tmp;

	/* xmmsv_list_get shouldn't work on empty list */
	value = xmmsv_new_list ();
	CU_ASSERT_FALSE (xmmsv_list_get (value, 0, &tmp));
	CU_ASSERT_FALSE (xmmsv_list_get (value, -1, &tmp));
	CU_ASSERT_FALSE (xmmsv_list_get (value, 1, &tmp));

	/* but on single element -1 and 0 should work, but 1 fail */
	tmp = xmmsv_new_int (3);
	CU_ASSERT_TRUE (xmmsv_list_append (value, tmp));
	xmmsv_unref (tmp);

	CU_ASSERT_TRUE (xmmsv_list_get (value, 0, &tmp));
	CU_ASSERT_TRUE (xmmsv_list_get (value, -1, &tmp));
	CU_ASSERT_FALSE (xmmsv_list_get (value, 1, &tmp));

	xmmsv_unref (value);

}

CASE (test_xmmsv_type_list_restrict_1) {
	xmmsv_t *value, *tmp;

	/* create 2 element list */
	value = xmmsv_new_list ();

	tmp = xmmsv_new_int (0);
	CU_ASSERT_TRUE (xmmsv_list_append (value, tmp));
	xmmsv_unref (tmp);

	CU_ASSERT_TRUE (xmmsv_list_restrict_type (value, XMMSV_TYPE_INT32));

	tmp = xmmsv_new_string ("x");
	CU_ASSERT_FALSE (xmmsv_list_append (value, tmp));
	xmmsv_unref (tmp);

	tmp = xmmsv_new_int (0);
	CU_ASSERT_TRUE (xmmsv_list_append (value, tmp));
	xmmsv_unref (tmp);

	CU_ASSERT_TRUE (xmmsv_list_clear (value));

	CU_ASSERT_FALSE (xmmsv_list_restrict_type (value, XMMSV_TYPE_STRING));


	xmmsv_unref (value);

}

CASE (test_xmmsv_type_list_restrict_2) {
	xmmsv_t *value, *tmp;

	/* create 2 element list */
	value = xmmsv_new_list ();

	tmp = xmmsv_new_int (0);
	CU_ASSERT_TRUE (xmmsv_list_append (value, tmp));
	xmmsv_unref (tmp);

	CU_ASSERT_FALSE (xmmsv_list_restrict_type (value, XMMSV_TYPE_STRING));

	tmp = xmmsv_new_int (0);
	CU_ASSERT_TRUE (xmmsv_list_append (value, tmp));
	xmmsv_unref (tmp);

	CU_ASSERT_TRUE (xmmsv_list_clear (value));

	CU_ASSERT_TRUE (xmmsv_list_restrict_type (value, XMMSV_TYPE_STRING));

	tmp = xmmsv_new_int (0);
	CU_ASSERT_FALSE (xmmsv_list_append (value, tmp));
	xmmsv_unref (tmp);

	tmp = xmmsv_new_string ("x");
	CU_ASSERT_TRUE (xmmsv_list_append (value, tmp));
	xmmsv_unref (tmp);

	xmmsv_unref (value);

}

static void _dict_foreach (const char *key, xmmsv_t *value, void *udata)
{
	CU_ASSERT_EQUAL (xmmsv_get_type (value), XMMSV_TYPE_INT32);
}

CASE (test_xmmsv_type_dict)
{
	xmmsv_dict_iter_t *it = NULL;
	xmmsv_t *value, *tmp;
	const char *key = NULL;
	int i;

	value = xmmsv_new_dict ();
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_DICT));

	CU_ASSERT_EQUAL (XMMSV_TYPE_DICT, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_DICT));

	tmp = xmmsv_new_int (1);
	CU_ASSERT_FALSE (xmmsv_is_type (tmp, XMMSV_TYPE_DICT));
	xmmsv_unref (tmp);

	tmp = xmmsv_new_int (42);
	CU_ASSERT_TRUE (xmmsv_dict_set (value, "test1", tmp));
	xmmsv_unref (tmp);

	/* { test1 => 42 }*/
	CU_ASSERT_TRUE (xmmsv_dict_get (value, "test1", &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 42);

	CU_ASSERT_FALSE (xmmsv_dict_get (value, "apan tutar i skogen", &tmp));

	tmp = xmmsv_new_int (666);
	CU_ASSERT_TRUE (xmmsv_dict_set (value, "test1", tmp));
	xmmsv_unref (tmp);

	/* { test1 => 666 } */
	tmp = xmmsv_new_int (23);
	CU_ASSERT_TRUE (xmmsv_dict_set (value, "test2", tmp));
	xmmsv_unref (tmp);

	/* { test1 => 666, test2 => 23 } */
	CU_ASSERT_EQUAL (xmmsv_dict_entry_get_type (value, "test1"), XMMSV_TYPE_INT32);
	CU_ASSERT_TRUE (xmmsv_dict_foreach (value, _dict_foreach, NULL));

	CU_ASSERT_TRUE (xmmsv_get_dict_iter (value, &it));

	xmmsv_dict_iter_first (it);
	CU_ASSERT_TRUE (xmmsv_dict_iter_find (it, "test1"));

	tmp = xmmsv_new_int (1337);
	CU_ASSERT_TRUE (xmmsv_dict_iter_set (it, tmp));
	xmmsv_unref (tmp);

	/* Set using the value already stored must work without extern ref.
	 * Allow things like xmmsv_dict_get (..., &val); xmmsv_dict_iter_set (it, val); */
	CU_ASSERT_TRUE (xmmsv_dict_iter_set (it, tmp));

	/* { test1 => 1337, test2 => 23 } */
	CU_ASSERT_TRUE (xmmsv_dict_iter_valid (it));

	CU_ASSERT_TRUE (xmmsv_dict_iter_pair (it, &key, &tmp));
	CU_ASSERT_TRUE (key && strcmp (key, "test1") == 0);
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 1337);

	key = NULL; i = 0;
	CU_ASSERT_TRUE (xmmsv_dict_iter_pair_int (it, &key, &i));
	CU_ASSERT_TRUE (key && strcmp (key, "test1") == 0);
	CU_ASSERT_EQUAL (i, 1337);

	key = NULL; i = 0;
	CU_ASSERT_TRUE (xmmsv_dict_iter_pair_int (it, NULL, &i));
	CU_ASSERT_EQUAL (i, 1337);

	key = NULL; i = 0;
	CU_ASSERT_TRUE (xmmsv_dict_iter_pair_int (it, &key, NULL));
	CU_ASSERT_TRUE (key && strcmp (key, "test1") == 0);

	CU_ASSERT_TRUE (xmmsv_dict_remove (value, "test1"));
	CU_ASSERT_FALSE (xmmsv_dict_remove (value, "test1"));

	/* { test2 => 23 } */
	xmmsv_dict_iter_first (it); /* We have to reset the iterator after modifying
								   the dict with xmmsv_dict_remove */
	CU_ASSERT_TRUE (xmmsv_dict_iter_pair_int (it, &key, &i));
	CU_ASSERT_TRUE (key && strcmp (key, "test2") == 0);
	CU_ASSERT_EQUAL (i, 23);

	CU_ASSERT_TRUE (xmmsv_dict_iter_remove (it));  /* remove "test2" */
	CU_ASSERT_FALSE (xmmsv_dict_iter_remove (it)); /* empty! */

	/* { } */
	tmp = xmmsv_new_int (42);
	CU_ASSERT_TRUE (xmmsv_dict_set (value, "test1", tmp));
	xmmsv_unref (tmp);

	/* { test1 => 42} */
	CU_ASSERT_TRUE (xmmsv_dict_clear (value));
	CU_ASSERT_TRUE (xmmsv_dict_clear (value));

	/* { } */
	tmp = xmmsv_new_int (42);
	CU_ASSERT_TRUE (xmmsv_dict_set (value, "test1", tmp));
	xmmsv_unref (tmp);

	/* { test1 => 42 } */
	xmmsv_unref (value);

	value = xmmsv_new_error ("oh noes");
	CU_ASSERT_FALSE (xmmsv_get_dict_iter (value, &it));
	CU_ASSERT_FALSE (xmmsv_dict_entry_get_type (value, "foo"));
	CU_ASSERT_FALSE (xmmsv_dict_foreach (value, _dict_foreach, NULL));
	xmmsv_unref (value);
}

CASE (test_xmmsv_dict_format) {
	xmmsv_t *val;
	char *buf;
	int r;

	/* We use malloc instead of stack as most tools are better on
	   detecting overruns on heap */
	buf = malloc(255);

	val = xmmsv_build_dict (XMMSV_DICT_ENTRY_STR ("a", "aaaaaaa"),
	                        XMMSV_DICT_ENTRY_STR ("b", "bbbbbbb"),
	                        XMMSV_DICT_ENTRY_INT ("c",  1234567),
	                        XMMSV_DICT_END);

	r = xmmsv_dict_format (buf, 255, "A: ${a} B: ${b} C: ${c}", val);
	CU_ASSERT_STRING_EQUAL (buf, "A: aaaaaaa B: bbbbbbb C: 1234567");
	/* strlen(buf) == 32 */
	CU_ASSERT_EQUAL (32, r);


	memset (buf, 0xff, 255);
	r = xmmsv_dict_format (buf, 27, "A: ${a} B: ${b} C: ${c}", val);
	CU_ASSERT_STRING_EQUAL (buf, "A: aaaaaaa B: bbbbbbb C: 1");
	CU_ASSERT_EQUAL (26, r);
	CU_ASSERT_EQUAL (0xff, (unsigned char)buf[27]);

	xmmsv_unref (val);
	free (buf);

}

CASE (test_xmmsv_list_move) {
	xmmsv_t *l;
	xmmsv_list_iter_t *its[5];
	int i, entry;
	int original[] = {0, 1, 2, 3, 4};
	int changed[]  = {0, 2, 3, 1, 4};

	l = xmmsv_new_list ();

	/* Fill the list */
	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_append_int (l, i));
		CU_ASSERT_TRUE (xmmsv_get_list_iter (l, &its[i]));
		CU_ASSERT_TRUE (xmmsv_list_iter_seek(its[i], i));
	}

	/* list now is {0, 1, 2, 3, 4} (original) */

	/* Check if state is okay */
	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_get_int (l, i, &entry));
		CU_ASSERT_EQUAL (original[i], entry);
	}
	/* Check if iters are still pointing at their element */
	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_iter_entry_int (its[i], &entry));
		CU_ASSERT_EQUAL (i, entry);
	}

	/* Do a move (old < new) */
	CU_ASSERT_TRUE (xmmsv_list_move (l, 1, 3));

	/* list now is { 0, 2, 3, 1, 4 } (changed) */

	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_get_int (l, i, &entry));
		CU_ASSERT_EQUAL (changed[i], entry);
	}
	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_iter_entry_int (its[i], &entry));
		CU_ASSERT_EQUAL (i, entry);
	}

	/* Move back (new < old) */
	CU_ASSERT_TRUE (xmmsv_list_move (l, 3, 1));

	/* list now is {0, 1, 2, 3, 4} (original) */

	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_get_int (l, i, &entry));
		CU_ASSERT_EQUAL (original[i], entry);
	}
	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_iter_entry_int (its[i], &entry));
		CU_ASSERT_EQUAL (i, entry);
	}

	/* Move nothing (new == old) */
	CU_ASSERT_TRUE (xmmsv_list_move (l, 2, 2));

	/* list now is {0, 1, 2, 3, 4} (original) */

	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_get_int (l, i, &entry));
		CU_ASSERT_EQUAL (original[i], entry);
	}
	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_iter_entry_int (its[i], &entry));
		CU_ASSERT_EQUAL (i, entry);
	}

	/* Can't move to inexistent position */
	CU_ASSERT_FALSE (xmmsv_list_move (l, 0, 5));

	/* Can't move from inexistent position */
	CU_ASSERT_FALSE (xmmsv_list_move (l, 5, 0));

	/* Check if counting from end works */
	CU_ASSERT_TRUE (xmmsv_list_move (l, -4, -2));

	/* list now is {0, 2, 3, 1, 4} (changed) */

	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_get_int (l, i, &entry));
		CU_ASSERT_EQUAL (changed[i], entry);
	}
	for (i = 0; i < 5; i++) {
		CU_ASSERT_TRUE (xmmsv_list_iter_entry_int (its[i], &entry));
		CU_ASSERT_EQUAL (i, entry);
	}

	xmmsv_unref (l);
}

CASE (test_xmmsv_type_bitbuffer_one_bit)
{
	xmmsv_t *value;
	int64_t r;

	value = xmmsv_new_bitbuffer ();
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_BITBUFFER));

	CU_ASSERT_EQUAL (XMMSV_TYPE_BITBUFFER, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_bits (value, 1, 0));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_bits (value, 1, 1));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_bits (value, 1, 0));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_bits (value, 1, 0));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_bits (value, 1, 0));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_bits (value, 1, 0));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_bits (value, 1, 1));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_bits (value, 1, 1));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_rewind (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 0);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 1);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 0);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 0);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 0);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 0);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 1);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 1);

	CU_ASSERT_FALSE (xmmsv_bitbuffer_get_bits (value, 1, &r));


	CU_ASSERT_TRUE (xmmsv_bitbuffer_rewind (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 8, &r));
	CU_ASSERT_EQUAL (r, 0x43);

	xmmsv_unref (value);
}

CASE (test_xmmsv_type_bitbuffer_8_bits)
{
	xmmsv_t *value;
	int64_t r;

	value = xmmsv_new_bitbuffer ();
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_BITBUFFER));

	CU_ASSERT_EQUAL (XMMSV_TYPE_BITBUFFER, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_bits (value, 8, 0x43));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_rewind (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 0);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 1);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 0);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 0);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 0);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 0);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 1);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 1, &r));
	CU_ASSERT_EQUAL (r, 1);

	CU_ASSERT_FALSE (xmmsv_bitbuffer_get_bits (value, 1, &r));


	CU_ASSERT_TRUE (xmmsv_bitbuffer_rewind (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 8, &r));
	CU_ASSERT_EQUAL (r, 0x43);

	xmmsv_unref (value);
}

CASE (test_xmmsv_type_bitbuffer)
{
	xmmsv_t *value;
	unsigned char b[4];

	value = xmmsv_new_bitbuffer ();
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_BITBUFFER));

	CU_ASSERT_EQUAL (XMMSV_TYPE_BITBUFFER, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_data (value, (unsigned char *)"test", 4));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_rewind (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_data (value, b, 4));

	CU_ASSERT_EQUAL ('t', b[0]);
	CU_ASSERT_EQUAL ('e', b[1]);
	CU_ASSERT_EQUAL ('s', b[2]);
	CU_ASSERT_EQUAL ('t', b[3]);

	xmmsv_unref (value);
}

CASE (test_xmmsv_type_bitbuffer2)
{
	xmmsv_t *value;
	int64_t r;

	value = xmmsv_new_bitbuffer ();
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_BITBUFFER));

	CU_ASSERT_EQUAL (XMMSV_TYPE_BITBUFFER, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_bits (value, 32, 0x1234));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_rewind (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 32, &r));

	CU_ASSERT_EQUAL (r, 0x1234);

	xmmsv_unref (value);
}

CASE (test_xmmsv_type_bitbuffer_ro)
{
	xmmsv_t *value;
	const unsigned char data[4] = {0x12, 0x23, 0x34, 0x45};
	unsigned char b[4];
	int64_t r;

	value = xmmsv_new_bitbuffer_ro (data, 4);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_BITBUFFER));

	CU_ASSERT_EQUAL (XMMSV_TYPE_BITBUFFER, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));

	CU_ASSERT_FALSE (xmmsv_bitbuffer_put_data (value, (unsigned char *)"test", 4));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_rewind (value));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_data (value, b, 4));
	CU_ASSERT_EQUAL (data[0], b[0]);
	CU_ASSERT_EQUAL (data[1], b[1]);
	CU_ASSERT_EQUAL (data[2], b[2]);
	CU_ASSERT_EQUAL (data[3], b[3]);

	CU_ASSERT_FALSE (xmmsv_bitbuffer_get_data (value, b, 4));

	CU_ASSERT_TRUE (xmmsv_bitbuffer_rewind (value));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 32, &r));

	CU_ASSERT_EQUAL (0x12233445, r);

	CU_ASSERT_TRUE (xmmsv_bitbuffer_rewind (value));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_bits (value, 24, &r));

	CU_ASSERT_EQUAL (0x122334, r);

	xmmsv_unref (value);
}

CASE (test_xmmsv_list_flatten) {
	xmmsv_t *list, *flat, *tmp;
	int l1[] = {0, 1, 2, 3};
	int l2[] = {4, 5, 6, 7};
	int i;
	int32_t ival;

	/* Create a list of two lists */
	list = xmmsv_new_list ();

	tmp = xmmsv_new_list ();
	for (i = 0; i < 4; i++) {
		xmmsv_list_append_int (tmp, l1[i]);
	}
	xmmsv_list_append (list, tmp);
	xmmsv_unref (tmp);

	tmp = xmmsv_new_list ();
	for (i = 0; i < 4; i++) {
		xmmsv_list_append_int (tmp, l2[i]);
	}
	xmmsv_list_append (list, tmp);
	xmmsv_unref (tmp);

	flat = xmmsv_list_flatten (list, 1);
	CU_ASSERT_PTR_NOT_NULL (flat);
	for (i = 0; i < 8; i++) {
		CU_ASSERT (xmmsv_list_get_int (flat, i, &ival));
		if (i < 4) {
			CU_ASSERT (ival == l1[i]);
		} else {
			CU_ASSERT (ival == l2[i - 4]);
		}
	}
	xmmsv_unref (flat);

	flat = xmmsv_list_flatten (list, 2);
	CU_ASSERT_PTR_NULL (flat);

	tmp = xmmsv_new_list ();
	xmmsv_list_append (tmp, list);
	flat = xmmsv_list_flatten (tmp, 2);
	CU_ASSERT_PTR_NOT_NULL (flat);
	for (i = 0; i < 8; i++) {
		CU_ASSERT (xmmsv_list_get_int (flat, i, &ival));
		if (i < 4) {
			CU_ASSERT (ival == l1[i]);
		} else {
			CU_ASSERT (ival == l2[i - 4]);
		}
	}
	xmmsv_unref (flat);
	xmmsv_unref (tmp);
	xmmsv_unref (list);
}

CASE (test_xmmsv_deep_copy)
{
	xmmsv_t *value, *val2, *val_cpy;
	const char *s;
	int i;

	val2 = xmmsv_new_list ();
	value = xmmsv_new_int (7654321);
	xmmsv_list_append (val2, value);
	xmmsv_unref (value);
	value = xmmsv_new_string ("hiya");
	xmmsv_list_append (val2, value);
	xmmsv_unref (value);

	value = xmmsv_build_dict (XMMSV_DICT_ENTRY_STR ("a", "aaaaaaa"),
	                        XMMSV_DICT_ENTRY_STR ("b", ""),
	                        XMMSV_DICT_ENTRY_INT ("c",  1234567),
	                        XMMSV_DICT_END);

	xmmsv_dict_set (value, "list", val2);

	val_cpy = xmmsv_copy (value);
	xmmsv_unref (value);
	xmmsv_unref (val2);

	CU_ASSERT_PTR_NOT_NULL (val_cpy);
	CU_ASSERT_TRUE (xmmsv_dict_get (val_cpy, "a", &value));
	CU_ASSERT_TRUE (xmmsv_get_string (value, &s));
	CU_ASSERT_TRUE (strcmp (s, "aaaaaaa") == 0);

	CU_ASSERT_TRUE (xmmsv_dict_get (val_cpy, "b", &value));
	CU_ASSERT_TRUE (xmmsv_get_string (value, &s));
	CU_ASSERT_TRUE (strcmp (s, "") == 0);

	CU_ASSERT_TRUE (xmmsv_dict_get (val_cpy, "c", &value));
	CU_ASSERT_TRUE (xmmsv_get_int (value, &i));
	CU_ASSERT_EQUAL (i, 1234567);

	CU_ASSERT_TRUE (xmmsv_dict_get (val_cpy, "list", &value));
	CU_ASSERT_TRUE (xmmsv_list_get_int (value, 0, &i));
	CU_ASSERT_EQUAL (i, 7654321);
	CU_ASSERT_TRUE (xmmsv_list_get_string (value, 1, &s));
	CU_ASSERT_EQUAL (strcmp (s, "hiya"), 0);
	xmmsv_unref (val_cpy);
}

CASE (test_xmmsv_deep_copy_bin)
{
	const unsigned char *b;
	const unsigned char *a = (const unsigned char *) "look behind you! a three headed monkey!";
	unsigned int b_len, a_len = strlen ((const char *) a) + 1;
	xmmsv_t *value, *val_cpy;

	value = xmmsv_new_bin (a, a_len);

	val_cpy = xmmsv_copy (value);
	xmmsv_unref (value);
	CU_ASSERT_TRUE (xmmsv_get_bin (val_cpy, &b, &b_len));

	CU_ASSERT_EQUAL (a_len, b_len);
	CU_ASSERT_STRING_EQUAL (a, b);
	xmmsv_unref (val_cpy);
}

CASE (test_xmmsv_deep_copy_bitbuffer)
{
	unsigned char b[4];
	xmmsv_t *value, *val_cpy;
	value = xmmsv_new_bitbuffer ();

	CU_ASSERT_TRUE (xmmsv_bitbuffer_put_data (value, (unsigned char *)"test", 4));

	val_cpy = xmmsv_copy (value);
	xmmsv_unref (value);

	CU_ASSERT_EQUAL (xmmsv_bitbuffer_pos (val_cpy), 32);
	CU_ASSERT_EQUAL (xmmsv_bitbuffer_len (val_cpy), 32);
	CU_ASSERT_TRUE (xmmsv_bitbuffer_rewind (val_cpy));
	CU_ASSERT_TRUE (xmmsv_bitbuffer_get_data (val_cpy, b, 4));

	CU_ASSERT_EQUAL ('t', b[0]);
	CU_ASSERT_EQUAL ('e', b[1]);
	CU_ASSERT_EQUAL ('s', b[2]);
	CU_ASSERT_EQUAL ('t', b[3]);

	xmmsv_unref (val_cpy);
}

CASE (test_xmmsv_deep_copy_collection)
{
	xmmsv_t *u, *a, *b, *copy;

	a = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	b = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	u = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNION);
	xmmsv_coll_add_operand (u, a);
	xmmsv_unref (a);
	xmmsv_coll_add_operand (u, b);
	xmmsv_unref (b);

	copy = xmmsv_copy (u);

	CU_ASSERT_EQUAL (XMMS_COLLECTION_TYPE_UNION, xmmsv_coll_get_type (copy));

	xmmsv_unref (u);
	xmmsv_unref (copy);
}
