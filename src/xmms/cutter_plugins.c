/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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
typedef struct xmms_cutter_data_St {
	gint64 start_bytes;
	gint64 seek_bytes;
	gint64 current_bytes;
	gint64 stop_bytes;
} xmms_cutter_data_t;


/*
 * Function prototypes
 */

static gboolean xmms_segment_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_segment_init (xmms_xform_t *xform);

static gboolean xmms_nibbler_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_nibbler_init (xmms_xform_t *xform);

static gboolean xmms_cutter_init (xmms_xform_t *xform, gint64 start_bytes, gint64 stop_bytes);
static void xmms_cutter_destroy (xmms_xform_t *xform);
static gint xmms_cutter_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *error);
static gint64 xmms_cutter_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *error);


/*
 * Segment specific functions
 */

static gboolean
xmms_segment_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_segment_init;
	methods.destroy = xmms_cutter_destroy;
	methods.read = xmms_cutter_read;
	methods.seek = xmms_cutter_seek;

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
	gint startms, stopms;
	gint64 startbytes, stopbytes;
	const gchar *metakey;
	xmms_stream_type_t *intype;

	g_return_val_if_fail (xform, FALSE);

	xmms_xform_outdata_type_copy (xform);

	/* get startms */
	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_STARTMS;
	if (xmms_xform_metadata_get_str (xform, metakey, &nptr)) {
		startms = (gint) strtol (nptr, &endptr, 10);
		if (*endptr != '\0') {
			XMMS_DBG ("\"startms\" has garbage, ignoring");
			startms = 0;
		}
	} else {
		startms = 0;
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
		stopms = INT_MAX;
	}

	/* some calculation */
	intype = xmms_xform_intype_get (xform);

	startbytes = xmms_sample_ms_to_bytes (intype, startms);
	if (stopms == INT_MAX) {
		stopbytes = G_MAXINT64;
	} else {
		stopbytes = xmms_sample_ms_to_bytes (intype, stopms);
	}

	return xmms_cutter_init (xform, startbytes, stopbytes);
}


/*
 * Nibbler specific functions
 */

static gboolean
xmms_nibbler_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_nibbler_init;
	methods.destroy = xmms_cutter_destroy;
	methods.read = xmms_cutter_read;
	methods.seek = xmms_cutter_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-uncut-pcm",
	                              XMMS_STREAM_TYPE_NAME,
	                              "pcm with startsamples/stopsamples auxdata",
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gboolean
xmms_nibbler_init (xmms_xform_t *xform)
{
	gint64 tmp, startbytes, stopbytes;
	xmms_stream_type_t *intype;

	g_return_val_if_fail (xform, FALSE);
	intype = xmms_xform_intype_get (xform);

	if (xmms_xform_auxdata_get_int64 (xform, "startsamples", &tmp)) {
		startbytes = xmms_sample_samples_to_bytes (intype, tmp);
	} else {
		startbytes = 0;
	}
	if (xmms_xform_auxdata_get_int64 (xform, "stopsamples", &tmp)) {
		stopbytes = xmms_sample_samples_to_bytes (intype, tmp);
	} else {
		stopbytes = G_MAXINT64;
	}

	return xmms_cutter_init (xform, startbytes, stopbytes);
}

/*
 * Shared functions
 */

static gboolean
xmms_cutter_init (xmms_xform_t *xform, gint64 start_bytes, gint64 stop_bytes)
{
	gint fmt, chans, srate;
	xmms_cutter_data_t *data;
	xmms_error_t error;
	gint64 res;

	g_return_val_if_fail (xform, FALSE);

	/* Set outdata_type */
	fmt   = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_FORMAT);
	chans = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_CHANNELS);
	srate = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT, fmt,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS, chans,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE, srate,
	                             XMMS_STREAM_TYPE_END);

	/* Set duration */
	if (stop_bytes != G_MAXINT64) {
		const gchar *metakey;
		gint64 dur;

		if (stop_bytes < start_bytes) {
			xmms_log_error ("Negative duration requested, rejecting.");
			return FALSE;
		}
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		dur = xmms_sample_bytes_to_ms (xmms_xform_outtype_get (xform),
		                               stop_bytes - start_bytes);
		xmms_xform_metadata_set_int (xform, metakey, dur);
	}

	/* allocate and set data */
	data = g_new0 (xmms_cutter_data_t, 1);
	data->start_bytes   = start_bytes;
	data->seek_bytes    = 0;
	data->current_bytes = 0;
	data->stop_bytes    = stop_bytes;

	xmms_xform_private_data_set (xform, data);

	/* Now seek to start of desired data */
	if (data->start_bytes > 0) {
		xmms_error_reset (&error);
		res = xmms_cutter_seek (xform, 0, XMMS_XFORM_SEEK_SET, &error);
		if (res == -1 || xmms_error_iserror (&error)) {
			xmms_log_error ("Couldn't seek, assuming we're at 0. Error is '%s'",
			                xmms_error_message_get (&error));
			data->current_bytes = 0;
			data->seek_bytes = data->start_bytes;
		}
	}

	return TRUE;
}
static void
xmms_cutter_destroy (xmms_xform_t *xform)
{
	xmms_cutter_data_t *data;

	data = xmms_xform_private_data_get (xform);
	if (data)
		g_free (data);
}

static gint
xmms_cutter_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                  xmms_error_t *error)
{
	xmms_cutter_data_t *data;
	gint res;
	gint64 hi, lo;

	g_return_val_if_fail(error, -1);
	g_return_val_if_fail(xform, -1);
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail(data, -1);

	do {
		if (data->current_bytes >= data->stop_bytes) {
			return 0; /* EOF */
		}
		res = xmms_xform_read (xform, buf, len, error);
		if (res == 0 || xmms_error_iserror (error)) {
			return 0; /* EOF */
		}
		lo = MAX(0,   data->seek_bytes - data->current_bytes);
		hi = MIN(res, data->stop_bytes - data->current_bytes);
		data->current_bytes += res;
	} while (lo >= hi);
	memmove (buf, buf + lo, hi-lo);
	return hi-lo;
}

static gint64
xmms_cutter_seek (xmms_xform_t *xform, gint64 samples,
                  xmms_xform_seek_mode_t whence, xmms_error_t *error)
{
	xmms_cutter_data_t *data;
	xmms_stream_type_t *ot;
	gint64 bytes, res, outer_samples;

	g_return_val_if_fail(error, -1);
	g_return_val_if_fail(xform, -1);
	ot = xmms_xform_outtype_get (xform);
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail(data, -1);

	bytes = xmms_sample_samples_to_bytes(ot, samples);
	switch (whence) {
	case XMMS_XFORM_SEEK_CUR:
		bytes += data->current_bytes;
		break;
	case XMMS_XFORM_SEEK_SET:
		bytes += data->start_bytes;
		break;
	case XMMS_XFORM_SEEK_END:
		if (data->stop_bytes == G_MAXINT64) {
			xmms_error_set (error, XMMS_ERROR_INVAL,
			                "Cannot seek relative to end without stop_bytes");
			return -1;
		}
		bytes += data->stop_bytes;
	}

	if (bytes < data->start_bytes || bytes > data->stop_bytes) {
		xmms_error_set (error, XMMS_ERROR_INVAL, "Seeking out of range");
		return -1;
	}

	res = xmms_xform_seek (xform, xmms_sample_bytes_to_samples_inexact (ot, bytes),
	                       XMMS_XFORM_SEEK_SET, error);
	if (res == -1 || xmms_error_iserror (error)) {
		return -1;
	}

	data->current_bytes = xmms_sample_samples_to_bytes (ot, res);
	data->seek_bytes = MAX(bytes, data->current_bytes);

	outer_samples = xmms_sample_bytes_to_samples (ot,
			data->seek_bytes - data->start_bytes, error);
	if (xmms_error_iserror (error)) {
		/* The running assumption is that seek_bytes and start_bytes make sense
		   in samples. If that is not the case something is very wrong. */
		xmms_log_error ("seek_bytes - start_bytes impossible! Please report this!");
		return -1;
	}
	return outer_samples;
}


/*
 * Setup of plugins
 */

XMMS_XFORM_BUILTIN_DEFINE (segment,
                           "Segment Effect",
                           XMMS_VERSION,
                           "Handling segment information specified by startms/stopms",
                           xmms_segment_plugin_setup);

XMMS_XFORM_BUILTIN_DEFINE (nibbler,
                           "Nibbler",
                           XMMS_VERSION,
                           "Discarding padding using auxdata startsamples/stopsamples",
                           xmms_nibbler_plugin_setup);
