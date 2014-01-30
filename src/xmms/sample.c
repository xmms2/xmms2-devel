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


#include <glib.h>
#include <xmms/xmms_sample.h>

/**
 * convert from milliseconds to samples for this format.
 */
guint
xmms_sample_ms_to_samples (const xmms_stream_type_t *st, guint milliseconds)
{
	gint rate;
	rate = xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	return (guint)(((gdouble) rate) * milliseconds / 1000);
}

/**
 * Convert from samples to milliseconds for this format
 */
guint
xmms_sample_samples_to_ms (const xmms_stream_type_t *st, guint samples)
{
	gint rate;
	rate = xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	return (guint) (((gdouble)samples) * 1000.0 / rate);
}

/**
 * Convert from bytes to milliseconds for this format
 */
guint
xmms_sample_bytes_to_ms (const xmms_stream_type_t *st, guint bytes)
{
	guint samples = bytes / xmms_sample_frame_size_get (st);
	return xmms_sample_samples_to_ms (st, samples);
}

gint
xmms_sample_frame_size_get (const xmms_stream_type_t *st)
{
	gint format, channels;
	format = xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_FMT_FORMAT);
	channels = xmms_stream_type_get_int (st, XMMS_STREAM_TYPE_FMT_CHANNELS);
	return xmms_sample_size_get (format) * channels;
}
