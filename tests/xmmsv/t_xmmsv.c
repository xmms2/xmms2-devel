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
	CU_ASSERT_STRING_EQUAL (a, xmmsv_get_error_old (value));

	xmmsv_unref (value);

	value = xmmsv_new_uint (0);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_UINT32));
	CU_ASSERT_FALSE (xmmsv_get_error (value, &b));
	CU_ASSERT_PTR_NULL (xmmsv_get_error_old (value));
	xmmsv_unref (value);
}

CASE (test_xmmsv_type_uint32)
{
	unsigned int b, a = UINT_MAX;
	xmmsv_t *value;

	value = xmmsv_new_uint (a);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_UINT32));

	CU_ASSERT_EQUAL (XMMSV_TYPE_UINT32, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));
	CU_ASSERT_TRUE (xmmsv_get_uint (value, &b));
	CU_ASSERT_EQUAL (a, b);

	xmmsv_unref (value);

	value = xmmsv_new_error ("oh noes");
	CU_ASSERT_FALSE (xmmsv_get_uint (value, &b));
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
	xmmsv_coll_t *b, *a = xmmsv_coll_universe ();
	xmmsv_t *value;

	value = xmmsv_new_coll (a);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_COLL));
	xmmsv_coll_unref (a);

	CU_ASSERT_EQUAL (XMMSV_TYPE_COLL, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));
	CU_ASSERT_TRUE (xmmsv_get_collection (value, &b));

	xmmsv_unref (value);
}


CASE (test_xmmsv_type_coll_wrong_type)
{
	xmmsv_coll_t *b;
	xmmsv_t *value;

	value = xmmsv_new_error ("oh noes");
	CU_ASSERT_FALSE (xmmsv_get_collection (value, &b));
	xmmsv_unref (value);
}

CASE (test_xmmsv_type_bin)
{
	const unsigned char *b;
	unsigned char *a = "look behind you! a three headed monkey!";
	unsigned int b_len, a_len = strlen (a) + 1;
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
	CU_ASSERT_TRUE (xmmsv_is_list (value));

	tmp = xmmsv_new_int (1);
	CU_ASSERT_FALSE (xmmsv_is_list (tmp));
	CU_ASSERT_FALSE (xmmsv_is_type (tmp, XMMSV_TYPE_LIST));
	xmmsv_unref (tmp);

	for (i = 20; i < 40; i++) {
		tmp = xmmsv_new_int (i);
		CU_ASSERT_TRUE (xmmsv_list_append (value, tmp));
		xmmsv_unref (tmp);
	}

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

	CU_ASSERT_TRUE (xmmsv_list_get (value, 0, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 21);

	CU_ASSERT_FALSE (xmmsv_list_get (value, 1000, &tmp));

	tmp = xmmsv_new_int (20);
	CU_ASSERT_TRUE (xmmsv_list_insert (value, 0, tmp));
	CU_ASSERT_FALSE (xmmsv_list_insert (value, 1000, tmp));
	xmmsv_unref (tmp);

	for (i = 19; i >= 10; i--) {
		tmp = xmmsv_new_int (i);
		CU_ASSERT_TRUE (xmmsv_list_insert (value, 0, tmp));
		xmmsv_unref (tmp);
	}

	CU_ASSERT_TRUE (xmmsv_get_list_iter (value, &it));

	for (i = 9; i >= 0; i--) {
		tmp = xmmsv_new_int (i);
		CU_ASSERT_TRUE (xmmsv_list_iter_insert (it, tmp));
		xmmsv_unref (tmp);
	}

	CU_ASSERT_FALSE (xmmsv_list_iter_goto (it, 1000));
	CU_ASSERT_TRUE (xmmsv_list_iter_goto (it, -1));
	CU_ASSERT_TRUE (xmmsv_list_iter_entry (it, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &j));
	CU_ASSERT_EQUAL (40 - 1, j);
	CU_ASSERT_TRUE (xmmsv_list_iter_goto (it, 0));

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

static void _dict_foreach (const char *key, xmmsv_t *value, void *udata)
{
	CU_ASSERT_EQUAL (xmmsv_get_type (value), XMMSV_TYPE_INT32);
}

CASE (test_xmmsv_type_dict)
{
	xmmsv_dict_iter_t *it = NULL;
	xmmsv_t *value, *tmp;
	const char *key = NULL;
	unsigned int ui;
	int i;

	value = xmmsv_new_dict ();
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_DICT));

	CU_ASSERT_EQUAL (XMMSV_TYPE_DICT, xmmsv_get_type (value));
	CU_ASSERT_FALSE (xmmsv_is_error (value));
	CU_ASSERT_TRUE (xmmsv_is_dict (value));
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_DICT));

	tmp = xmmsv_new_uint (1);
	CU_ASSERT_FALSE (xmmsv_is_dict (tmp));
	xmmsv_unref (tmp);

	tmp = xmmsv_new_uint (42);
	CU_ASSERT_TRUE (xmmsv_dict_set (value, "test1", tmp));
	xmmsv_unref (tmp);

	CU_ASSERT_TRUE (xmmsv_dict_get (value, "test1", &tmp));
	CU_ASSERT_TRUE (xmmsv_get_uint (tmp, &ui));
	CU_ASSERT_EQUAL (ui, 42);

	CU_ASSERT_FALSE (xmmsv_dict_get (value, "apan tutar i skogen", &tmp));

	tmp = xmmsv_new_int (666);
	CU_ASSERT_TRUE (xmmsv_dict_set (value, "test1", tmp));
	xmmsv_unref (tmp);

	tmp = xmmsv_new_int (23);
	CU_ASSERT_TRUE (xmmsv_dict_set (value, "test2", tmp));
	xmmsv_unref (tmp);

	CU_ASSERT_EQUAL (xmmsv_get_dict_entry_type (value, "test1"), XMMSV_TYPE_INT32);
	CU_ASSERT_TRUE (xmmsv_dict_foreach (value, _dict_foreach, NULL));

	CU_ASSERT_TRUE (xmmsv_get_dict_iter (value, &it));

	xmmsv_dict_iter_first (it);
	CU_ASSERT_TRUE (xmmsv_dict_iter_find (it, "test1"));

	tmp = xmmsv_new_int (1337);
	CU_ASSERT_TRUE (xmmsv_dict_iter_set (it, tmp));
	xmmsv_unref (tmp);

	CU_ASSERT_TRUE (xmmsv_dict_iter_valid (it));
	CU_ASSERT_TRUE (xmmsv_dict_iter_pair (it, &key, &tmp));
	CU_ASSERT_TRUE (key && strcmp (key, "test1") == 0);
	CU_ASSERT_TRUE (xmmsv_get_int (tmp, &i));
	CU_ASSERT_EQUAL (i, 1337);

	CU_ASSERT_TRUE (xmmsv_dict_remove (value, "test1"));
	CU_ASSERT_FALSE (xmmsv_dict_remove (value, "test1"));

	CU_ASSERT_TRUE (xmmsv_dict_iter_remove (it));  /* remove "test2" */
	CU_ASSERT_FALSE (xmmsv_dict_iter_remove (it)); /* empty! */

	tmp = xmmsv_new_uint (42);
	CU_ASSERT_TRUE (xmmsv_dict_set (value, "test1", tmp));
	xmmsv_unref (tmp);

	CU_ASSERT_TRUE (xmmsv_dict_clear (value));
	CU_ASSERT_TRUE (xmmsv_dict_clear (value));

	tmp = xmmsv_new_uint (42);
	CU_ASSERT_TRUE (xmmsv_dict_set (value, "test1", tmp));
	xmmsv_unref (tmp);

	xmmsv_unref (value);

	value = xmmsv_new_error ("oh noes");
	CU_ASSERT_FALSE (xmmsv_get_dict_iter (value, &it));
	CU_ASSERT_FALSE (xmmsv_get_dict_entry_type (value, "foo"));
	CU_ASSERT_FALSE (xmmsv_dict_foreach (value, _dict_foreach, NULL));
	xmmsv_unref (value);
}

