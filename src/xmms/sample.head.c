/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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
#include <math.h>
#include "xmmspriv/xmms_sample.h"
#include "xmms/xmms_medialib.h"
#include "xmms/xmms_object.h"
#include "xmms/xmms_log.h"

/**
  * @defgroup Sample Sample Converter
  * @ingroup XMMSServer
  * @brief Convert sample formats back and forth.
  * @{
  */

/**
 * The converter module
 */
struct xmms_sample_converter_St {
	xmms_object_t obj;

	xmms_stream_type_t *from;
	xmms_stream_type_t *to;

	gboolean same;
	gboolean resample;

	/* buffer for result */
	guint bufsiz;
	xmms_sample_t *buf;

	guint interpolator_ratio;
	guint decimator_ratio;

	guint offset;

	xmms_sample_t *state;

	xmms_sample_conv_func_t func;

};

static void recalculate_resampler (xmms_sample_converter_t *conv, guint from, guint to);
static xmms_sample_conv_func_t
xmms_sample_conv_get (guint inchannels, xmms_sample_format_t intype,
                      guint outchannels, xmms_sample_format_t outtype,
                      gboolean resample);



static void
xmms_sample_converter_destroy (xmms_object_t *obj)
{
	xmms_sample_converter_t *conv = (xmms_sample_converter_t *) obj;

	g_free (conv->buf);
	g_free (conv->state);
}

xmms_sample_converter_t *
xmms_sample_converter_init (xmms_stream_type_t *from, xmms_stream_type_t *to)
{
	xmms_sample_converter_t *conv = xmms_object_new (xmms_sample_converter_t, xmms_sample_converter_destroy);
	gint fformat, fsamplerate, fchannels;
	gint tformat, tsamplerate, tchannels;

	fformat = xmms_stream_type_get_int (from, XMMS_STREAM_TYPE_FMT_FORMAT);
	fsamplerate = xmms_stream_type_get_int (from, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	fchannels = xmms_stream_type_get_int (from, XMMS_STREAM_TYPE_FMT_CHANNELS);
	tformat = xmms_stream_type_get_int (to, XMMS_STREAM_TYPE_FMT_FORMAT);
	tsamplerate = xmms_stream_type_get_int (to, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	tchannels = xmms_stream_type_get_int (to, XMMS_STREAM_TYPE_FMT_CHANNELS);

	g_return_val_if_fail (tformat != -1, NULL);
	g_return_val_if_fail (tchannels != -1, NULL);
	g_return_val_if_fail (tsamplerate != -1, NULL);

	conv->from = from;
	conv->to = to;

	conv->resample = fsamplerate != tsamplerate;

	conv->func = xmms_sample_conv_get (fchannels, fformat,
	                                   tchannels, tformat,
	                                   conv->resample);

	if (!conv->func) {
		xmms_object_unref (conv);
		xmms_log_error ("Unable to convert from %s/%d/%d to %s/%d/%d.",
		                xmms_sample_name_get (fformat), fsamplerate, fchannels,
		                xmms_sample_name_get (tformat), tsamplerate, tchannels);
		return NULL;
	}

	if (conv->resample)
		recalculate_resampler (conv, fsamplerate, tsamplerate);

	return conv;
}

/**
 * Return the audio format used by the converter as source
 */
xmms_stream_type_t *
xmms_sample_converter_get_from (xmms_sample_converter_t *conv)
{
	g_return_val_if_fail (conv, NULL);

	return conv->from;
}

/**
 * Return the audio format used by the converter as target
 */
xmms_stream_type_t *
xmms_sample_converter_get_to (xmms_sample_converter_t *conv)
{
	g_return_val_if_fail (conv, NULL);

	return conv->to;
}

/**
 */
void
xmms_sample_converter_to_medialib (xmms_sample_converter_t *conv, xmms_medialib_entry_t entry)
{
#if 0
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin_write ();
	xmms_medialib_entry_property_set_str (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_SAMPLEFMT_IN,
	                                      xmms_sample_name_get (conv->from->format));
	xmms_medialib_entry_property_set_int (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_SAMPLERATE_IN,
	                                      conv->from->samplerate);
	xmms_medialib_entry_property_set_int (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_CHANNELS_IN,
	                                      conv->from->channels);

	xmms_medialib_entry_property_set_str (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_SAMPLEFMT_OUT,
	                                      xmms_sample_name_get (conv->to->format));
	xmms_medialib_entry_property_set_int (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_SAMPLERATE_OUT,
	                                      conv->to->samplerate);
	xmms_medialib_entry_property_set_int (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_CHANNELS_OUT,
	                                      conv->to->channels);

	xmms_medialib_end (session);
#endif
}


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

static void
recalculate_resampler (xmms_sample_converter_t *conv, guint from, guint to)
{
	guint a,b;

	/* calculate ratio */
	if (from > to){
		a = from;
		b = to;
	} else {
		b = to;
		a = from;
	}

	while (b != 0) { /* good 'ol euclid is helpful as usual */
		guint t = a % b;
		a = b;
		b = t;
	}

	XMMS_DBG ("Resampling ratio: %d:%d",
	          from / a, to / a);

	conv->interpolator_ratio = to/a;
	conv->decimator_ratio = from/a;

	conv->state = g_malloc0 (xmms_sample_frame_size_get (conv->from));

	/*
	 * calculate filter here
	 *
	 * We don't use no stinkning filter. Maybe we should,
	 * but I'm deaf anyway, I wont hear any difference.
	 */

}


/**
 * do the actual converstion between two audio formats.
 */
void
xmms_sample_convert (xmms_sample_converter_t *conv, xmms_sample_t *in, guint len, xmms_sample_t **out, guint *outlen)
{
	int inusiz, outusiz;
	int olen;
	guint res;

	inusiz = xmms_sample_frame_size_get (conv->from);

	g_return_if_fail (len % inusiz == 0);

	if (conv->same) {
		*outlen = len;
		*out = in;
		return;
	}

	len /= inusiz;

	outusiz = xmms_sample_frame_size_get (conv->to);

	if (conv->resample) {
		olen = (len * conv->interpolator_ratio / conv->decimator_ratio) * outusiz + outusiz;
	} else {
		olen = len * outusiz;
	}
	if (olen > conv->bufsiz) {
		void *t;
		t = g_realloc (conv->buf, olen);
		g_assert (t); /* XXX */
		conv->buf = t;
		conv->bufsiz = olen;
	}

	res = conv->func (conv, in, len, conv->buf);

	*outlen = res * outusiz;
	*out = conv->buf;

}

gint64
xmms_sample_convert_scale (xmms_sample_converter_t *conv, gint64 samples)
{
	/* this isn't 100% accurate, we should take care
	   of rounding here and set conv->offset, but noone
	   will notice, except when reading this comment :) */

	if (!conv->resample)
		return samples;
	return samples * conv->decimator_ratio / conv->interpolator_ratio;
}

gint64
xmms_sample_convert_rev_scale (xmms_sample_converter_t *conv, gint64 samples)
{
	if (!conv->resample)
		return samples;
	return samples * conv->interpolator_ratio / conv->decimator_ratio;
}

void
xmms_sample_convert_reset (xmms_sample_converter_t *conv)
{
	if (conv->resample) {
		conv->offset = 0;
		memset (conv->state, 0, xmms_sample_frame_size_get (conv->from));
	}
}

/**
 * @}
 */
