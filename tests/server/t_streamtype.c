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

static void
destroy_list (gpointer data, gpointer user_data)
{
	xmms_object_t *obj = (xmms_object_t *) data;
	xmms_object_unref (obj);
}

CASE (test_coerce)
{
	xmms_stream_type_t *typ, *from, *to = NULL;
	GList *list = NULL;

	from = _xmms_stream_type_new ("dummy",
	                              XMMS_STREAM_TYPE_URL, "test://",
	                              XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm",
	                              XMMS_STREAM_TYPE_FMT_FORMAT, XMMS_SAMPLE_FORMAT_S32,
	                              XMMS_STREAM_TYPE_FMT_CHANNELS, 2,
	                              XMMS_STREAM_TYPE_FMT_SAMPLERATE, 88200,
	                              XMMS_STREAM_TYPE_END);

	typ = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_URL, "test://",
	                             XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT, XMMS_SAMPLE_FORMAT_S32,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS, 1,
	                             XMMS_STREAM_TYPE_END);
	list = g_list_append (list, typ);

	typ = _xmms_stream_type_new ("dummy",
	                             XMMS_STREAM_TYPE_URL, "test://",
	                             XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT, XMMS_SAMPLE_FORMAT_S32,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS, 2,
	                             XMMS_STREAM_TYPE_END);
	list = g_list_append (list, typ);

	to = xmms_stream_type_coerce (from, list);
	CU_ASSERT_PTR_NOT_NULL (to);

	CU_ASSERT_EQUAL (XMMS_SAMPLE_FORMAT_S32,
	                 xmms_stream_type_get_int (to, XMMS_STREAM_TYPE_FMT_FORMAT));
	CU_ASSERT_EQUAL (88200,
	                 xmms_stream_type_get_int (to, XMMS_STREAM_TYPE_FMT_SAMPLERATE));
	CU_ASSERT_EQUAL (2,
	                 xmms_stream_type_get_int (to, XMMS_STREAM_TYPE_FMT_CHANNELS));

	g_list_foreach (list, destroy_list, NULL);
	g_list_free (list);

	xmms_object_unref (from);
	xmms_object_unref (to);
}

