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

#ifndef __SAMPLE_H__
#define __SAMPLE_H__

#include <glib.h>
#include <xmms/xmms_streamtype.h>
#include <xmmsc/xmmsc_compiler.h>
#include <xmms/xmms_error.h>

G_BEGIN_DECLS

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


typedef gint8 xmms_samples8_t;
#define XMMS_SAMPLES8_MIN -128
#define XMMS_SAMPLES8_MAX 127
typedef guint8 xmms_sampleu8_t;
#define XMMS_SAMPLEU8_MAX 255
typedef gint16 xmms_samples16_t;
#define XMMS_SAMPLES16_MIN -32768
#define XMMS_SAMPLES16_MAX 32767
typedef guint16 xmms_sampleu16_t;
#define XMMS_SAMPLEU16_MAX 65535
typedef gint32 xmms_samples32_t;
#define XMMS_SAMPLES32_MIN (-2147483647L-1)
#define XMMS_SAMPLES32_MAX 2147483647L
typedef guint32 xmms_sampleu32_t;
#define XMMS_SAMPLEU32_MAX 4294967295UL
typedef gfloat xmms_samplefloat_t;
typedef gdouble xmms_sampledouble_t;
typedef void xmms_sample_t;

gint xmms_sample_frame_size_get (const xmms_stream_type_t *st) XMMS_PUBLIC;
gint64 xmms_sample_ms_to_samples (const xmms_stream_type_t *st, gint64 ms) XMMS_PUBLIC;
gint64 xmms_sample_samples_to_ms (const xmms_stream_type_t *st, gint64 samples) XMMS_PUBLIC;
gint64 xmms_sample_samples_to_bytes (const xmms_stream_type_t *st, gint64 samples) XMMS_PUBLIC;
gint64 xmms_sample_bytes_to_samples (const xmms_stream_type_t *st, gint64 bytes, xmms_error_t *error) XMMS_PUBLIC;
gint64 xmms_sample_bytes_to_samples_inexact (const xmms_stream_type_t *st, gint64 bytes) XMMS_PUBLIC;
gint64 xmms_sample_ms_to_bytes (const xmms_stream_type_t *st, gint64 ms) XMMS_PUBLIC;
gint64 xmms_sample_bytes_to_ms (const xmms_stream_type_t *st, gint64 bytes) XMMS_PUBLIC;

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

static inline const gchar *
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

G_END_DECLS

#endif
