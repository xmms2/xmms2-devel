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

SETUP (xmmsv_serialization) {
	return 0;
}

CLEANUP () {
	return 0;
}

CASE (test_xmmsv_serialize_null)
{
	CU_ASSERT_PTR_NULL (xmmsv_serialize (NULL));
	CU_ASSERT_PTR_NULL (xmmsv_deserialize (NULL));
}

CASE (test_xmmsv_serialize_none)
{
	xmmsv_t *bin, *value;
	const unsigned char *data;
	unsigned int length;
	const unsigned char expected[] = {
		0x00, 0x00, 0x00, 0x00 /* XMMSV_TYPE_NONE */
	};

	value = xmmsv_new_none ();

	bin = xmmsv_serialize (value);
	xmmsv_unref (value);

	CU_ASSERT_PTR_NOT_NULL (bin);

	CU_ASSERT_TRUE (xmmsv_get_bin (bin, &data, &length));
	CU_ASSERT_EQUAL (length, sizeof (expected));

	CU_ASSERT_EQUAL (memcmp (data, expected, length), 0);

	value = xmmsv_deserialize (bin);
	xmmsv_unref (bin);

	CU_ASSERT_PTR_NOT_NULL (value);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_NONE));

	xmmsv_unref (value);
}

CASE (test_xmmsv_serialize_error)
{
	xmmsv_t *bin, *value;
	const unsigned char *data;
	unsigned int length;
	const char *s;
	const unsigned char expected[] = {
		0x00, 0x00, 0x00, 0x01, /* XMMSV_TYPE_ERROR */
		0x00, 0x00, 0x00, 0x04, /* 4 (length of following bytes) */
		0x66, 0x6f, 0x6f, 0x00  /* "foo\0" */
	};

	value = xmmsv_new_error ("foo");

	bin = xmmsv_serialize (value);
	xmmsv_unref (value);

	CU_ASSERT_PTR_NOT_NULL (bin);

	CU_ASSERT_TRUE (xmmsv_get_bin (bin, &data, &length));
	CU_ASSERT_EQUAL (length, sizeof (expected));

	CU_ASSERT_EQUAL (memcmp (data, expected, length), 0);

	value = xmmsv_deserialize (bin);
	xmmsv_unref (bin);

	CU_ASSERT_PTR_NOT_NULL (value);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_ERROR));

	CU_ASSERT_TRUE (xmmsv_get_error (value, &s));
	CU_ASSERT_STRING_EQUAL (s, "foo");

	xmmsv_unref (value);
}

CASE (test_xmmsv_serialize_small_int)
{
	xmmsv_t *bin, *value;
	const unsigned char *data;
	unsigned int length;
	int32_t i;
	const unsigned char expected[] = {
		0x00, 0x00, 0x00, 0x02, /* XMMSV_TYPE_INT32 */
		0x00, 0x00, 0x00, 0x2a  /* 42 */
	};

	value = xmmsv_new_int (42);

	bin = xmmsv_serialize (value);
	xmmsv_unref (value);

	CU_ASSERT_PTR_NOT_NULL (bin);

	CU_ASSERT_TRUE (xmmsv_get_bin (bin, &data, &length));
	CU_ASSERT_EQUAL_FATAL (length, sizeof (expected));

	CU_ASSERT_EQUAL (memcmp (data, expected, length), 0);

	value = xmmsv_deserialize (bin);
	xmmsv_unref (bin);

	CU_ASSERT_PTR_NOT_NULL (value);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_INT32));

	CU_ASSERT_TRUE (xmmsv_get_int (value, &i));
	CU_ASSERT_EQUAL (i, 42);

	xmmsv_unref (value);
}

CASE (test_xmmsv_serialize_large_int)
{
	xmmsv_t *bin, *value;
	const unsigned char *data;
	unsigned int length;
	int32_t i;
	const unsigned char expected[] = {
		0x00, 0x00, 0x00, 0x02, /* XMMSV_TYPE_INT32 */
		0x00, 0x00, 0x25, 0xc3  /* 9667 */
	};

	value = xmmsv_new_int (9667);

	bin = xmmsv_serialize (value);
	xmmsv_unref (value);

	CU_ASSERT_PTR_NOT_NULL (bin);

	CU_ASSERT_TRUE (xmmsv_get_bin (bin, &data, &length));
	CU_ASSERT_EQUAL (length, sizeof (expected));

	CU_ASSERT_EQUAL (memcmp (data, expected, length), 0);

	value = xmmsv_deserialize (bin);
	xmmsv_unref (bin);

	CU_ASSERT_PTR_NOT_NULL (value);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_INT32));

	CU_ASSERT_TRUE (xmmsv_get_int (value, &i));
	CU_ASSERT_EQUAL (i, 9667);

	xmmsv_unref (value);
}

CASE (test_xmmsv_serialize_string)
{
	xmmsv_t *bin, *value;
	const unsigned char *data;
	unsigned int length;
	const char *s;
	const unsigned char expected[] = {
		0x00, 0x00, 0x00, 0x03, /* XMMSV_TYPE_STRING */
		0x00, 0x00, 0x00, 0x04, /* 4 (length of following bytes) */
		0x66, 0x6f, 0x6f, 0x00  /* "foo\0" */
	};

	value = xmmsv_new_string ("foo");

	bin = xmmsv_serialize (value);
	xmmsv_unref (value);

	CU_ASSERT_PTR_NOT_NULL (bin);

	CU_ASSERT_TRUE (xmmsv_get_bin (bin, &data, &length));
	CU_ASSERT_EQUAL (length, sizeof (expected));

	CU_ASSERT_EQUAL (memcmp (data, expected, length), 0);

	value = xmmsv_deserialize (bin);
	xmmsv_unref (bin);

	CU_ASSERT_PTR_NOT_NULL (value);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_STRING));

	CU_ASSERT_TRUE (xmmsv_get_string (value, &s));
	CU_ASSERT_STRING_EQUAL (s, "foo");

	xmmsv_unref (value);
}

CASE (test_xmmsv_serialize_coll_match)
{
	xmmsv_t *bin, *value, *tmp, *attrs, *operands;
	xmmsv_coll_t *coll, *all_media;
	const unsigned char *data;
	unsigned int length;
	char **s;
	const unsigned char expected[] = {
		0x00, 0x00, 0x00, 0x04, /* XMMSV_TYPE_COLL */
		0x00, 0x00, 0x00, 0x06, /* XMMS_COLLECTION_TYPE_MATCH */
		0x00, 0x00, 0x00, 0x02, /* number of attributes*/

		0x00, 0x00, 0x00, 0x06, /* attr[0] key length */
		0x66, 0x69, 0x65, 0x6c, /* attr[0] key "fiel" */
		0x64, 0x00,             /*              "d\0" */

		0x00, 0x00, 0x00, 0x07, /* attr[0] value length */
		0x61, 0x72, 0x74, 0x69, /* attr[0] value "arti"*/
		0x73, 0x74, 0x00,       /*               "st\0" */

		0x00, 0x00, 0x00, 0x06, /* attr[1] key length */
		0x76, 0x61, 0x6c, 0x75, /* attr[1] key "valu" */
		0x65, 0x00,             /*             "e\0" */

		0x00, 0x00, 0x00, 0x0c, /* attr[1] value length */
		0x2a, 0x73, 0x65, 0x6e, /* attr[1] value "*sen"*/
		0x74, 0x65, 0x6e, 0x63, /*               "tenc" */
		0x65, 0x64, 0x2a, 0x00, /*               "ed*\0" */

		0x00, 0x00, 0x00, 0x00, /* number of idlist entries */
		0x00, 0x00, 0x00, 0x01, /* number of operands */

		0x00, 0x00, 0x00, 0x04, /* operand[0]: type XMMSV_TYPE_COLL */
		0x00, 0x00, 0x00, 0x00, /* operand[0]: coll type (_REFERENCE) */
		0x00, 0x00, 0x00, 0x01, /* number of attributes*/

		0x00, 0x00, 0x00, 0x0a, /* attr[0] key length */
		0x72, 0x65, 0x66, 0x65, /* attr[0] key "refe" */
		0x72, 0x65, 0x6e, 0x63, /*             "renc" */
		0x65, 0x00,             /*             "e\0"  */

		0x00, 0x00, 0x00, 0x0a, /* attr[0] value length */
		0x41, 0x6c, 0x6c, 0x20, /* attr[0] value "All " */
		0x4d, 0x65, 0x64, 0x69, /*               "Medi" */
		0x61, 0x00,             /*               "a\0"  */

		0x00, 0x00, 0x00, 0x00, /* number of idlist entries */
		0x00, 0x00, 0x00, 0x00  /* number of operands */
	};

	coll = xmmsv_coll_new (XMMS_COLLECTION_TYPE_MATCH);

	xmmsv_coll_attribute_set (coll, "field", "artist");
	xmmsv_coll_attribute_set (coll, "value", "*sentenced*");

	all_media = xmmsv_coll_universe ();
	xmmsv_coll_add_operand (coll, all_media);
	xmmsv_coll_unref (all_media);

	value = xmmsv_new_coll (coll);
	xmmsv_coll_unref (coll);

	bin = xmmsv_serialize (value);
	xmmsv_unref (value);

	CU_ASSERT_PTR_NOT_NULL (bin);

	CU_ASSERT_TRUE (xmmsv_get_bin (bin, &data, &length));
	CU_ASSERT_EQUAL (length, sizeof (expected));
	CU_ASSERT_EQUAL (memcmp (data, expected, length), 0);

	value = xmmsv_deserialize (bin);
	xmmsv_unref (bin);

	CU_ASSERT_PTR_NOT_NULL (value);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_COLL));
	CU_ASSERT_TRUE (xmmsv_get_coll (value, &coll));

	CU_ASSERT_EQUAL (xmmsv_coll_get_type (coll), XMMS_COLLECTION_TYPE_MATCH);

	CU_ASSERT_EQUAL (xmmsv_dict_get_size (xmmsv_coll_attributes_get (coll)),
	                 2);

	CU_ASSERT_TRUE (xmmsv_coll_attribute_get (coll, "field", &s));
	CU_ASSERT_STRING_EQUAL (s, "artist");

	CU_ASSERT_TRUE (xmmsv_coll_attribute_get (coll, "value", &s));
	CU_ASSERT_STRING_EQUAL (s, "*sentenced*");

	operands = xmmsv_coll_operands_get (coll);

	CU_ASSERT_EQUAL (xmmsv_list_get_size (operands), 1);

	CU_ASSERT_TRUE (xmmsv_list_get (operands, 0, &tmp));
	CU_ASSERT_TRUE (xmmsv_get_coll (tmp, &all_media));

	CU_ASSERT_EQUAL (xmmsv_coll_get_type (all_media),
	                 XMMS_COLLECTION_TYPE_REFERENCE);

	attrs = xmmsv_coll_attributes_get (all_media);
	CU_ASSERT_EQUAL (xmmsv_dict_get_size (attrs), 1);

	CU_ASSERT_TRUE (xmmsv_coll_attribute_get (all_media, "reference", &s));
	CU_ASSERT_STRING_EQUAL (s, "All Media");

	xmmsv_unref (value);
}

CASE (test_xmmsv_serialize_bin)
{
	xmmsv_t *bin, *value;
	const unsigned char *data, *data2;
	unsigned int length, length2, input_length = 8;
	const unsigned char input[] = "\x01\x02\x03\x04\x00\x01\x02\x03";
	const unsigned char expected[] = {
		0x00, 0x00, 0x00, 0x05, /* XMMSV_TYPE_BIN */
		0x00, 0x00, 0x00, 0x08, /* 8 (length of following bytes) */
		0x01, 0x02, 0x03, 0x04,
		0x00, 0x01, 0x02, 0x03
	};

	value = xmmsv_new_bin (input, input_length);

	bin = xmmsv_serialize (value);
	xmmsv_unref (value);

	CU_ASSERT_PTR_NOT_NULL (bin);

	CU_ASSERT_TRUE (xmmsv_get_bin (bin, &data, &length));
	CU_ASSERT_EQUAL (length, sizeof (expected));

	CU_ASSERT_EQUAL (memcmp (data, expected, length), 0);

	value = xmmsv_deserialize (bin);
	xmmsv_unref (bin);

	CU_ASSERT_PTR_NOT_NULL (value);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_BIN));

	CU_ASSERT_TRUE (xmmsv_get_bin (value, &data2, &length2));

	CU_ASSERT_EQUAL (length2, input_length);
	CU_ASSERT_EQUAL (memcmp (data2, input, input_length), 0);

	xmmsv_unref (value);
}

CASE (test_xmmsv_serialize_list)
{
	xmmsv_t *bin, *value, *item;
	const unsigned char *data;
	unsigned int length;
	int32_t i;
	const char *s;
	const unsigned char expected[] = {
		0x00, 0x00, 0x00, 0x06, /* XMMSV_TYPE_LIST */
		0x00, 0x00, 0x00, 0x02, /* 2 (number of list items) */

		0x00, 0x00, 0x00, 0x02, /* list[0]: XMMSV_TYPE_INT32 */
		0x00, 0x00, 0x00, 0x2a, /* list[0]: 42 */

		0x00, 0x00, 0x00, 0x03, /* list[1]: XMMSV_TYPE_STRING */
		0x00, 0x00, 0x00, 0x04, /* list[1]: 4 (length of following bytes) */
		0x66, 0x6f, 0x6f, 0x00  /* list[1]: "foo\0" */
	};

	value = xmmsv_new_list ();

	item = xmmsv_new_int (42);
	xmmsv_list_append (value, item);
	xmmsv_unref (item);

	item = xmmsv_new_string ("foo");
	xmmsv_list_append (value, item);
	xmmsv_unref (item);

	bin = xmmsv_serialize (value);
	xmmsv_unref (value);

	CU_ASSERT_PTR_NOT_NULL (bin);

	CU_ASSERT_TRUE (xmmsv_get_bin (bin, &data, &length));
	CU_ASSERT_EQUAL (length, sizeof (expected));

	CU_ASSERT_EQUAL (memcmp (data, expected, length), 0);

	value = xmmsv_deserialize (bin);
	xmmsv_unref (bin);

	CU_ASSERT_PTR_NOT_NULL (value);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_LIST));
	CU_ASSERT_EQUAL (xmmsv_list_get_size (value), 2);

	CU_ASSERT_TRUE (xmmsv_list_get (value, 0, &item));
	CU_ASSERT_PTR_NOT_NULL (item);
	CU_ASSERT_TRUE (xmmsv_is_type (item, XMMSV_TYPE_INT32));
	CU_ASSERT_TRUE (xmmsv_get_int (item, &i));
	CU_ASSERT_EQUAL (i, 42);

	CU_ASSERT_TRUE (xmmsv_list_get (value, 1, &item));
	CU_ASSERT_PTR_NOT_NULL (item);
	CU_ASSERT_TRUE (xmmsv_is_type (item, XMMSV_TYPE_STRING));
	CU_ASSERT_TRUE (xmmsv_get_string (item, &s));
	CU_ASSERT_STRING_EQUAL (s, "foo");

	xmmsv_unref (value);
}

CASE (test_xmmsv_serialize_dict)
{
	xmmsv_t *bin, *value, *item;
	const unsigned char *data;
	unsigned int length;
	int32_t i;
	const unsigned char expected[] = {
		0x00, 0x00, 0x00, 0x07, /* XMMSV_TYPE_DICT */
		0x00, 0x00, 0x00, 0x02, /* 2 (number of dict items) */

		0x00, 0x00, 0x00, 0x04, /* key[0]: 4 (length of following bytes) */
		0x62, 0x61, 0x72, 0x00, /* key[0]: "bar\0" */

		0x00, 0x00, 0x00, 0x02, /* value[0]: XMMSV_TYPE_INT32 */
		0x00, 0x00, 0x25, 0xc3, /* value[0]: 9667 */

		0x00, 0x00, 0x00, 0x04, /* key[1]: 4 (length of following bytes) */
		0x66, 0x6f, 0x6f, 0x00, /* key[1]: "foo\0" */

		0x00, 0x00, 0x00, 0x02, /* value[1]: XMMSV_TYPE_INT32 */
		0x00, 0x00, 0x00, 0x2a  /* value[1]: 42 */
	};

	value = xmmsv_new_dict ();

	item = xmmsv_new_int (42);
	xmmsv_dict_set (value, "foo", item);
	xmmsv_unref (item);

	item = xmmsv_new_int (9667);
	xmmsv_dict_set (value, "bar", item);
	xmmsv_unref (item);

	bin = xmmsv_serialize (value);
	xmmsv_unref (value);

	CU_ASSERT_PTR_NOT_NULL (bin);

	CU_ASSERT_TRUE (xmmsv_get_bin (bin, &data, &length));
	CU_ASSERT_EQUAL (length, sizeof (expected));

	CU_ASSERT_EQUAL (memcmp (data, expected, length), 0);

	value = xmmsv_deserialize (bin);
	xmmsv_unref (bin);

	CU_ASSERT_PTR_NOT_NULL (value);
	CU_ASSERT_TRUE (xmmsv_is_type (value, XMMSV_TYPE_DICT));
	CU_ASSERT_EQUAL (xmmsv_dict_get_size (value), 2);

	CU_ASSERT_TRUE (xmmsv_dict_get (value, "foo", &item));
	CU_ASSERT_PTR_NOT_NULL (item);
	CU_ASSERT_TRUE (xmmsv_is_type (item, XMMSV_TYPE_INT32));
	CU_ASSERT_TRUE (xmmsv_get_int (item, &i));
	CU_ASSERT_EQUAL (i, 42);

	CU_ASSERT_TRUE (xmmsv_dict_get (value, "bar", &item));
	CU_ASSERT_PTR_NOT_NULL (item);
	CU_ASSERT_TRUE (xmmsv_is_type (item, XMMSV_TYPE_INT32));
	CU_ASSERT_TRUE (xmmsv_get_int (item, &i));
	CU_ASSERT_EQUAL (i, 9667);

	xmmsv_unref (value);
}
