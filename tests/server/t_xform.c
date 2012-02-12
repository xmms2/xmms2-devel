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

#include "xmmspriv/xmms_plugin.h"
#include "xmmspriv/xmms_xform.h"
#include "xmmspriv/xmms_config.h"
#include "xmmspriv/xmms_log.h"
#include "xmmspriv/xmms_ipc.h"
#include "xmmspriv/xmms_medialib.h"

static xmms_medialib_t *medialib;

SETUP (xform)
{
	g_thread_init (0);

	xmms_ipc_init ();
	xmms_log_init (0);

	xmms_config_init ("memory://");
	xmms_config_property_register ("medialib.path", "memory://", NULL, NULL);

	medialib = xmms_medialib_init ();

	return 1;
}

CLEANUP ()
{
	xmms_object_unref (medialib);
	xmms_config_shutdown ();
	xmms_ipc_shutdown ();

	return 0;
}

static gboolean xmms_metadata_test_xform_init (xmms_xform_t *xform);
static gboolean xmms_metadata_test_xform_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_metadata_test_coverart (xmms_xform_t *xform, const gchar *key, const gchar *value, gsize length);

XMMS_XFORM_BUILTIN (metadata_test_xform,
                    "metadata test xform",
                    XMMS_VERSION,
                    "metadata test xform",
                    xmms_metadata_test_xform_plugin_setup);

/** These are the properties that we extract from the comments */
static const xmms_xform_metadata_basic_mapping_t basic_mappings[] = {
	{ "title",                     XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE        },
	{ "tracknr",                   XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR      },
	{ "replaygain_track_gain",     XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK   },
	{ "compilation",               XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION  },
};

static const xmms_xform_metadata_mapping_t mappings[] = {
	{ "coverart", xmms_metadata_test_coverart }
};

static gboolean
xmms_metadata_test_xform_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_metadata_test_xform_init;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_metadata_mapper_init (xform_plugin,
	                                        basic_mappings,
	                                        G_N_ELEMENTS (basic_mappings),
	                                        mappings,
	                                        G_N_ELEMENTS (mappings));

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url",
	                              XMMS_STREAM_TYPE_URL, "metadatatest://*",
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}


static gboolean
xmms_metadata_test_coverart (xmms_xform_t *xform, const gchar *key,
                             const gchar *value, gsize length)
{
	XMMS_DBG ("amagad");
	return TRUE;
}

static gboolean
xmms_metadata_test_xform_init (xmms_xform_t *xform)
{
	const gchar *musicbrainz_va_id = "89ad4ac3-39f7-470e-963a-56509c546377";
	const gchar *title, *rpgain;
	gint track, compilation;

	CU_ASSERT_FALSE (xmms_xform_metadata_mapper_match (xform, "missing", "missing", -1));

	/* Basic string mapping */
	CU_ASSERT_TRUE (xmms_xform_metadata_mapper_match (xform, "title", "the title", -1));
	CU_ASSERT_TRUE (xmms_xform_metadata_get_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, &title));
	CU_ASSERT_STRING_EQUAL ("the title", title);

	/* Mapping track number, without total tracks */
	CU_ASSERT_TRUE (xmms_xform_metadata_mapper_match (xform, "tracknr", "1", -1));
	CU_ASSERT_TRUE (xmms_xform_metadata_get_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, &track));
	CU_ASSERT_EQUAL (1, track);

	/* Broken track number */
	CU_ASSERT_FALSE (xmms_xform_metadata_mapper_match (xform, "tracknr", "bad", -1));

	/* Mapping compilation indicator to boolean compilation */
	CU_ASSERT_TRUE (xmms_xform_metadata_mapper_match (xform, "compilation", "1", -1));
	CU_ASSERT_TRUE (xmms_xform_metadata_get_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION, &compilation));
	CU_ASSERT_EQUAL (TRUE, compilation);

	/* Mapping compilation indicator to boolean compilation */
	CU_ASSERT_TRUE (xmms_xform_metadata_mapper_match (xform, "compilation", musicbrainz_va_id, -1));
	CU_ASSERT_TRUE (xmms_xform_metadata_get_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION, &compilation));
	CU_ASSERT_EQUAL (TRUE, compilation);


	/* Mapping replaygain to the format the replaygain xform expects */
	CU_ASSERT_TRUE (xmms_xform_metadata_mapper_match (xform, "replaygain_track_gain", "-14.69", -1));
	CU_ASSERT_TRUE (xmms_xform_metadata_get_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK, &rpgain));
	CU_ASSERT_STRING_EQUAL ("0.18428", rpgain);
	CU_ASSERT_TRUE (xmms_xform_metadata_mapper_match (xform, "replaygain_track_gain", "-14.69 dB", -1));
	CU_ASSERT_TRUE (xmms_xform_metadata_get_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK, &rpgain));
	CU_ASSERT_STRING_EQUAL ("0.18428", rpgain);


	CU_ASSERT_TRUE (xmms_xform_metadata_mapper_match (xform, "coverart", "test", 10));

	xmms_xform_outdata_type_add (xform, XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm", XMMS_STREAM_TYPE_END);

	return TRUE;
}

CASE(test_xform_metadata)
{
	xmms_medialib_session_t *session;
	xmms_stream_type_t *format;
	xmms_xform_t *xform;
	GList *goal_format;

	format = _xmms_stream_type_new (XMMS_STREAM_TYPE_BEGIN,
	                                XMMS_STREAM_TYPE_MIMETYPE,
	                                "audio/pcm",
	                                XMMS_STREAM_TYPE_END);
	goal_format = g_list_prepend (NULL, format);

	xmms_plugin_load (&xmms_builtin_metadata_test_xform, NULL);

	session = xmms_medialib_session_begin (medialib);
	xform = xmms_xform_chain_setup_url_session (medialib, session, 1,
	                                            "metadatatest://", goal_format,
	                                            TRUE);
	xmms_medialib_session_abort (session);
	xmms_object_unref (xform);

	g_list_free (goal_format);
	xmms_object_unref (format);
}
