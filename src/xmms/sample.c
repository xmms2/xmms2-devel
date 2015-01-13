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


#include <glib.h>
#include <xmms/xmms_sample.h>

/**
 * Get number of bytes used for one sample length worth of music.
 *
 * That is, (size of one sample) * channels.
 */
gint
xmms_sample_frame_size_get (const xmms_stream_type_t *st)
{
	gint format, channels;
	format = xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_FMT_FORMAT);
	channels = xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_FMT_CHANNELS);
	return xmms_sample_size_get (format) * channels;
}

/**
 * convert from milliseconds to samples for this format.
 */
gint64
xmms_sample_ms_to_samples (const xmms_stream_type_t *st, gint64 milliseconds)
{
	gint rate = xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	return (rate * milliseconds) / 1000;
}

/**
 * Convert from samples to milliseconds for this format
 */
gint64
xmms_sample_samples_to_ms (const xmms_stream_type_t *st, gint64 samples)
{
	gint rate = xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	return (samples * 1000) / rate;
}

/**
 * Convert from samples to bytes for this format. This is exact.
 */
gint64
xmms_sample_samples_to_bytes (const xmms_stream_type_t *st, gint64 samples)
{
	return samples * xmms_sample_frame_size_get (st);
}

/**
 * Convert from bytes to samples for this format. This is exact.
 * On error 0 is returned and error is set.
 */
gint64
xmms_sample_bytes_to_samples (const xmms_stream_type_t *st, gint64 bytes,
                              xmms_error_t *error)
{
	gint fs = xmms_sample_frame_size_get (st);
	if (bytes % fs != 0) {
		xmms_error_set (error, XMMS_ERROR_INVAL,
			"xmms_sample_bytes_to_samples with non-integral number of samples!");
		return 0;
	}
	return bytes / fs;
}

/**
 * Convert from bytes to samples for this format. This is not exact.
 */
gint64
xmms_sample_bytes_to_samples_inexact (const xmms_stream_type_t *st, gint64 bytes)
{
	gint fs = xmms_sample_frame_size_get (st);
	return bytes / fs;
}

/**
 * Convert from bytes to milliseconds for this format. This is not exact.
 */
gint64
xmms_sample_bytes_to_ms (const xmms_stream_type_t *st, gint64 bytes)
{
	return xmms_sample_samples_to_ms (st, xmms_sample_bytes_to_samples_inexact (st, bytes));
}

/**
 * Convert from milliseconds to bytes for this format. This is not exact.
 */
gint64
xmms_sample_ms_to_bytes (const xmms_stream_type_t *st, gint64 ms)
{
	return xmms_sample_samples_to_bytes (st, xmms_sample_ms_to_samples (st, ms));
}
