/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2004	Peter Alm, Tobias Rundström, Anders Gustafsson
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

#ifndef __SAMPLE_H__
#define __SAMPLE_H__

typedef enum {
	XMMS_SAMPLE_FORMAT_UNKNOWN,
	XMMS_SAMPLE_FORMAT_S8,
	XMMS_SAMPLE_FORMAT_U8,
	XMMS_SAMPLE_FORMAT_S16,
	XMMS_SAMPLE_FORMAT_U16,
	XMMS_SAMPLE_FORMAT_S32,
	XMMS_SAMPLE_FORMAT_U32,
	XMMS_SAMPLE_FORMAT_FLOAT,
	XMMS_SAMPLE_FORMAT_DOUBLE,
	/* DO NOT CHANGE ORDER! Just add to the end! */
} xmms_sample_format_t;

typedef struct { /* internal? */
	xmms_sample_format_t format;
	guint samplerate;
	guint channels;
} xmms_audio_format_t;

typedef struct xmms_sample_converter_St xmms_sample_converter_t;


typedef gint8 xmms_samples8_t;
typedef guint8 xmms_sampleu8_t;
typedef gint16 xmms_samples16_t;
typedef guint16 xmms_sampleu16_t;
typedef gint32 xmms_samples32_t;
typedef guint32 xmms_sampleu32_t;
typedef gfloat xmms_samplefloat_t;
typedef gdouble xmms_sampledouble_t;
typedef void xmms_sample_t;

typedef guint (*xmms_sample_conv_func_t) (xmms_sample_converter_t *, xmms_sample_t *, guint , xmms_sample_t *);

xmms_audio_format_t *xmms_sample_audioformat_new (xmms_sample_format_t fmt, guint channels, guint rate);
void xmms_sample_audioformat_destroy (xmms_audio_format_t *fmt);

guint xmms_sample_ms_to_samples (xmms_audio_format_t *f, guint milliseconds);
guint xmms_sample_samples_to_ms (xmms_audio_format_t *f, guint samples);
guint xmms_sample_bytes_to_ms (xmms_audio_format_t *f, guint samples);

/* internal? */
void xmms_sample_convert (xmms_sample_converter_t *conv, xmms_sample_t *in, guint len, xmms_sample_t **out, guint *outlen);
xmms_sample_converter_t *xmms_sample_audioformats_coerce (GList *declist, GList *outlist);
xmms_audio_format_t *xmms_sample_converter_get_from (xmms_sample_converter_t *conv);
xmms_audio_format_t *xmms_sample_converter_get_to (xmms_sample_converter_t *conv);


static inline gint
xmms_sample_size_get (xmms_sample_format_t fmt)
{
	switch (fmt) {
	case XMMS_SAMPLE_FORMAT_UNKNOWN:
		return -1;
	case XMMS_SAMPLE_FORMAT_S8:
		return sizeof (xmms_samples8_t);
	case XMMS_SAMPLE_FORMAT_U8:
		return sizeof (xmms_sampleu8_t);
	case XMMS_SAMPLE_FORMAT_S16:
		return sizeof (xmms_samples16_t);
	case XMMS_SAMPLE_FORMAT_U16:
		return sizeof (xmms_sampleu16_t);
	case XMMS_SAMPLE_FORMAT_S32:
		return sizeof (xmms_samples32_t);
	case XMMS_SAMPLE_FORMAT_U32:
		return sizeof (xmms_sampleu32_t);
	case XMMS_SAMPLE_FORMAT_FLOAT:
		return sizeof (xmms_samplefloat_t);
	case XMMS_SAMPLE_FORMAT_DOUBLE:
		return sizeof (xmms_sampledouble_t);
	}
	return -1;
}

static inline gboolean
xmms_sample_signed_get (xmms_sample_format_t fmt)
{
	switch (fmt) {
	case XMMS_SAMPLE_FORMAT_S8:
	case XMMS_SAMPLE_FORMAT_S16:
	case XMMS_SAMPLE_FORMAT_S32:
		return TRUE;
	case XMMS_SAMPLE_FORMAT_UNKNOWN:
	case XMMS_SAMPLE_FORMAT_U8:
	case XMMS_SAMPLE_FORMAT_U16:
	case XMMS_SAMPLE_FORMAT_U32:
	case XMMS_SAMPLE_FORMAT_FLOAT:
	case XMMS_SAMPLE_FORMAT_DOUBLE:
		return FALSE;
	}
	return FALSE;

}

static inline const char *
xmms_sample_name_get (xmms_sample_format_t fmt)
{
	switch (fmt) {
	case XMMS_SAMPLE_FORMAT_UNKNOWN:
		return "Unknown";
	case XMMS_SAMPLE_FORMAT_S8:
		return "S8";
	case XMMS_SAMPLE_FORMAT_U8:
		return "U8";
	case XMMS_SAMPLE_FORMAT_S16:
		return "S16";
	case XMMS_SAMPLE_FORMAT_U16:
		return "U16";
	case XMMS_SAMPLE_FORMAT_S32:
		return "S32";
	case XMMS_SAMPLE_FORMAT_U32:
		return "U32";
	case XMMS_SAMPLE_FORMAT_FLOAT:
		return "FLOAT";
	case XMMS_SAMPLE_FORMAT_DOUBLE:
		return "DOUBLE";
	}
	return "UNKNOWN";
}

#endif
