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

#include <glib.h>

#include "xmmspriv/xmms_streamtype.h"
#include "xmms/xmms_object.h"
#include "xmms/xmms_sample.h"

SETUP (streamtype) {
	g_thread_init (0);
	return 0;
}

CLEANUP () {
	return 0;
}

CASE (test_create)
{
	xmms_stream_type_t *st;

	st = _xmms_stream_type_new ("dummy",
	                            XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                            XMMS_STREAM_TYPE_END);
	xmms_object_unref (st);
}


CASE (test_getint)
{
	xmms_stream_type_t *st;

	st = _xmms_stream_type_new ("dummy",
	                            XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                            XMMS_STREAM_TYPE_PRIORITY, 10,
	                            XMMS_STREAM_TYPE_END);
	CU_ASSERT_EQUAL (10, xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_PRIORITY));

	xmms_object_unref (st);
}


CASE (test_getstr)
{
	xmms_stream_type_t *st;

	st = _xmms_stream_type_new ("dummy",
	                            XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                            XMMS_STREAM_TYPE_NAME, "TEST",
	                            XMMS_STREAM_TYPE_END);
	CU_ASSERT_STRING_EQUAL ("TEST", xmms_stream_type_get_str (st, XMMS_STREAM_TYPE_NAME));

	xmms_object_unref (st);
}


CASE (test_getstrfail)
{
	xmms_stream_type_t *st;

	st = _xmms_stream_type_new ("dummy",
	                            XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                            XMMS_STREAM_TYPE_NAME, "TEST",
	                            XMMS_STREAM_TYPE_END);
	CU_ASSERT_EQUAL (NULL, xmms_stream_type_get_str (st, XMMS_STREAM_TYPE_URL));

	xmms_object_unref (st);
}


CASE (test_getintfail)
{
	xmms_stream_type_t *st;

	st = _xmms_stream_type_new ("dummy",
	                            XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                            XMMS_STREAM_TYPE_END);
	CU_ASSERT_EQUAL (-1, xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_URL));

	xmms_object_unref (st);
}


CASE (test_getwrongtype)
{
	xmms_stream_type_t *st;

	st = _xmms_stream_type_new ("dummy",
	                            XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                            XMMS_STREAM_TYPE_END);
	CU_ASSERT_EQUAL (-1, xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_MIMETYPE));

	xmms_object_unref (st);
}


CASE (test_getwrongtype2)
{
	xmms_stream_type_t *st;

	st = _xmms_stream_type_new ("dummy",
	                            XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                            XMMS_STREAM_TYPE_PRIORITY, 10,
	                            XMMS_STREAM_TYPE_END);
	CU_ASSERT_EQUAL (NULL, xmms_stream_type_get_str (st, XMMS_STREAM_TYPE_PRIORITY));

	xmms_object_unref (st);
}

CASE (test_match1)
{
	xmms_stream_type_t *st1, *st2;

	st1 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_END);
	st2 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_END);


	CU_ASSERT_TRUE (xmms_stream_type_match (st1, st2));

	xmms_object_unref (st1);
	xmms_object_unref (st2);
}

CASE (test_nomatch1)
{
	xmms_stream_type_t *st1, *st2;

	st1 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_END);
	st2 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/TEST",
	                             XMMS_STREAM_TYPE_END);


	CU_ASSERT_FALSE (xmms_stream_type_match (st1, st2));

	xmms_object_unref (st1);
	xmms_object_unref (st2);

}


CASE (test_match2)
{
	xmms_stream_type_t *st1, *st2;

	st1 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/*",
	                             XMMS_STREAM_TYPE_END);
	st2 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_END);


	CU_ASSERT_TRUE (xmms_stream_type_match (st1, st2));

	xmms_object_unref (st1);
	xmms_object_unref (st2);

}


CASE (test_nomatch2)
{
	xmms_stream_type_t *st1, *st2;

	st1 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_END);
	st2 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/*",
	                             XMMS_STREAM_TYPE_END);


	CU_ASSERT_FALSE (xmms_stream_type_match (st1, st2));

	xmms_object_unref (st1);
	xmms_object_unref (st2);

}



CASE (test_match3)
{
	xmms_stream_type_t *st1, *st2;

	st1 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_URL, "test://",
	                             XMMS_STREAM_TYPE_END);
	st2 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_URL, "test://",
	                             XMMS_STREAM_TYPE_END);


	CU_ASSERT_TRUE (xmms_stream_type_match (st1, st2));

	xmms_object_unref (st1);
	xmms_object_unref (st2);

}

CASE (test_match3b)
{
	xmms_stream_type_t *st1, *st2;

	st1 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_END);
	st2 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_URL, "test://",
	                             XMMS_STREAM_TYPE_END);


	CU_ASSERT_TRUE (xmms_stream_type_match (st1, st2));

	xmms_object_unref (st1);
	xmms_object_unref (st2);

}

CASE (test_nomatch3)
{
	xmms_stream_type_t *st1, *st2;

	st1 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_URL, "test://",
	                             XMMS_STREAM_TYPE_END);
	st2 = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_MIMETYPE, "application/test",
	                             XMMS_STREAM_TYPE_END);


	CU_ASSERT_FALSE (xmms_stream_type_match (st1, st2));

	xmms_object_unref (st1);
	xmms_object_unref (st2);

}

