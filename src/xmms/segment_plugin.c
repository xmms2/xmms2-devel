/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
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

#include <stdlib.h>
#include <string.h>
#include <xmms/xmms_sample.h>
#include <xmmspriv/xmms_xform.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_error.h>

/*
 * Data structure
 */
typedef struct xmms_segment_data_St {
	gint64 start_bytes;
	gint64 current_bytes;
	gint64 stop_bytes;
} xmms_segment_data_t;


/*
 * Function prototypes
 */

static gboolean xmms_segment_init (xmms_xform_t *xform);
static void xmms_segment_destroy (xmms_xform_t *xform);
static gboolean xmms_segment_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_segment_read (xmms_xform_t *xform,
                               xmms_sample_t *buf,
                               gint len,
                               xmms_error_t *error);
static gint64 xmms_segment_seek (xmms_xform_t *xform,
                                 gint64 samples,
                                 xmms_xform_seek_mode_t whence,
                                 xmms_error_t *error);


/*
 * Plugin header
 */

static gboolean
xmms_segment_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_segment_init;
	methods.destroy = xmms_segment_destroy;
	methods.read = xmms_segment_read;
	methods.seek = xmms_segment_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/pcm",
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gboolean
xmms_segment_init (xmms_xform_t *xform)
{
	const gchar *nptr;
	char *endptr;
	gint startms;
	gint stopms;
	const gchar *metakey;
	xmms_error_t error;
	xmms_segment_data_t *data;
	xmms_stream_type_t *ot;
	gint64 samples;

	g_return_val_if_fail (xform, FALSE);

	xmms_xform_outdata_type_copy (xform);

	/* get startms */
	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_STARTMS;
	if (!xmms_xform_metadata_get_str (xform, metakey, &nptr)) {
		return TRUE;
	}

	startms = (gint) strtol (nptr, &endptr, 10);
	if (*endptr != '\0') {
		XMMS_DBG ("\"startms\" has garbage!");
		return TRUE;
	}

	/* get stopms */
	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_STOPMS;
	if (xmms_xform_metadata_get_str (xform, metakey, &nptr)) {
		stopms = (gint) strtol (nptr, &endptr, 10);
		if (*endptr != '\0') {
			xmms_log_info ("\"stopms\" has garbage, ignoring");
			stopms = INT_MAX;
		}
	} else {
		/* This is the last track, stopms is the playback duration */
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		if (!xmms_xform_metadata_get_int (xform, metakey, &stopms)) {
			XMMS_DBG ("\"duration\" doesnt exist, ignore stopms.");
			/* ignore stopms by setting it to the maximum value */
			stopms = INT_MAX;
		}
	}

	/* set the correct duration */
	if (stopms != INT_MAX) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		xmms_xform_metadata_set_int (xform, metakey, stopms - startms);
	}

	/* some calculation */
	ot = xmms_xform_outtype_get (xform);

	/* allocate and set data */
	data = g_new0 (xmms_segment_data_t, 1);
	data->current_bytes = data->start_bytes = xmms_sample_ms_to_bytes (ot, startms);
	data->stop_bytes = xmms_sample_ms_to_bytes (ot, stopms);

	xmms_xform_private_data_set (xform, data);

	/* Now seek to startms */

	samples = xmms_sample_ms_to_samples (ot, startms);
	xmms_xform_seek (xform, samples, XMMS_XFORM_SEEK_SET, &error);

	return TRUE;
}

static void
xmms_segment_destroy (xmms_xform_t *xform)
{
	xmms_segment_data_t *data;

	data = xmms_xform_private_data_get (xform);
	if (data)
		g_free (data);
}

static gint
xmms_segment_read (xmms_xform_t *xform,
                   xmms_sample_t *buf,
                   gint len,
                   xmms_error_t *error)
{
	xmms_segment_data_t *data;
	gint res;

	data = xmms_xform_private_data_get (xform);

	if (data && (data->current_bytes + len >= data->stop_bytes)) {
		len = data->stop_bytes - data->current_bytes;
	}

	res = xmms_xform_read (xform, buf, len, error);
	if (data && (res > 0)) {
		data->current_bytes += res;
	}

	return res;
}

static gint64
xmms_segment_seek (xmms_xform_t *xform,
                   gint64 samples,
                   xmms_xform_seek_mode_t whence,
                   xmms_error_t *error)
{
	xmms_segment_data_t *data;
	xmms_stream_type_t *ot;
	gint64 res;
	gint64 tmp;

	data = xmms_xform_private_data_get (xform);
	ot = xmms_xform_outtype_get (xform);

	if (!data) {
		return xmms_xform_seek (xform, samples, whence, error);
	}

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);

	if (samples < 0 ||
	    samples > xmms_sample_bytes_to_samples (ot, data->stop_bytes
	                                                - data->start_bytes)) {
		xmms_error_set (error,
		                XMMS_ERROR_INVAL,
		                "Seeking out of range");
		return -1;
	}

	tmp = xmms_sample_bytes_to_samples (ot, data->start_bytes);
	res = xmms_xform_seek (xform,
	                       samples + tmp,
	                       whence,
	                       error);
	data->current_bytes = xmms_sample_samples_to_bytes (ot, res);
	return res - tmp;
}

XMMS_XFORM_BUILTIN_DEFINE (segment,
                           "Segment Effect",
                           XMMS_VERSION,
                           "Handling segment information specified by startms/stopms",
                           xmms_segment_plugin_setup);
