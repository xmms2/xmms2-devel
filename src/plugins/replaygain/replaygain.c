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

/**
 * @file
 * Replaygain
 */

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"

#include <math.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>

typedef void (*xmms_replaygain_apply_func_t)
	(void *buf, gint len, gfloat gain);

/**
 * Replaygain modes.
 */
typedef enum {
	XMMS_REPLAYGAIN_MODE_TRACK,
	XMMS_REPLAYGAIN_MODE_ALBUM
	/** @todo implement dynamic replaygain */
} xmms_replaygain_mode_t;

typedef struct xmms_replaygain_data_St {
	xmms_replaygain_mode_t mode;
	gboolean use_anticlip;
	gfloat preamp;
	gfloat gain;
	gboolean has_replaygain;
	gboolean enabled;
	xmms_replaygain_apply_func_t apply;
} xmms_replaygain_data_t;

static const xmms_sample_format_t formats[] = {
	XMMS_SAMPLE_FORMAT_S8,
	XMMS_SAMPLE_FORMAT_U8,
	XMMS_SAMPLE_FORMAT_S16,
	XMMS_SAMPLE_FORMAT_U16,
	XMMS_SAMPLE_FORMAT_S32,
	XMMS_SAMPLE_FORMAT_U32,
	XMMS_SAMPLE_FORMAT_FLOAT,
	XMMS_SAMPLE_FORMAT_DOUBLE,
};

static gboolean xmms_replaygain_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_replaygain_init (xmms_xform_t *xform);
static void xmms_replaygain_destroy (xmms_xform_t *xform);
static gint xmms_replaygain_read (xmms_xform_t *xform, xmms_sample_t *buf,
                                  gint len, xmms_error_t *error);
static gint64 xmms_replaygain_seek (xmms_xform_t *xform, gint64 samples,
                                    xmms_xform_seek_mode_t whence,
                                    xmms_error_t *error);
static void xmms_replaygain_config_changed (xmms_object_t *obj, xmmsv_t *_val, gpointer udata);

static void compute_gain (xmms_xform_t *xform, xmms_replaygain_data_t *data);
static xmms_replaygain_mode_t parse_mode (const char *s);

static void apply_s8 (void *buf, gint len, gfloat gain);
static void apply_u8 (void *buf, gint len, gfloat gain);
static void apply_s16 (void *buf, gint len, gfloat gain);
static void apply_u16 (void *buf, gint len, gfloat gain);
static void apply_s32 (void *buf, gint len, gfloat gain);
static void apply_u32 (void *buf, gint len, gfloat gain);
static void apply_float (void *buf, gint len, gfloat gain);
static void apply_double (void *buf, gint len, gfloat gain);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("replaygain",
                   "Replaygain effect",
                   XMMS_VERSION,
                   "Replaygain effect",
                   xmms_replaygain_plugin_setup);

static gboolean
xmms_replaygain_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;
	gint i;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_replaygain_init;
	methods.destroy = xmms_replaygain_destroy;
	methods.read = xmms_replaygain_read;
	methods.seek = xmms_replaygain_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	for (i = 0; i < G_N_ELEMENTS (formats); i++) {
		xmms_xform_plugin_indata_add (xform_plugin,
		                              XMMS_STREAM_TYPE_MIMETYPE,
		                              "audio/pcm",
		                              XMMS_STREAM_TYPE_FMT_FORMAT,
		                              formats[i],
		                              XMMS_STREAM_TYPE_END);
	}


	xmms_xform_plugin_config_property_register (xform_plugin,
	                                            "mode", "track",
	                                            NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin,
	                                            "use_anticlip", "1",
	                                            NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin,
	                                            "preamp", "6.0",
	                                            NULL, NULL);

	return TRUE;
}

static gboolean
xmms_replaygain_init (xmms_xform_t *xform)
{
	xmms_replaygain_data_t *data;
	xmms_config_property_t *cfgv;
	xmms_sample_format_t fmt;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_replaygain_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_xform_private_data_set (xform, data);

	cfgv = xmms_xform_config_lookup (xform, "mode");
	xmms_config_property_callback_set (cfgv,
	                                   xmms_replaygain_config_changed,
	                                   xform);

	data->mode = parse_mode (xmms_config_property_get_string (cfgv));

	cfgv = xmms_xform_config_lookup (xform, "use_anticlip");
	xmms_config_property_callback_set (cfgv,
	                                   xmms_replaygain_config_changed,
	                                   xform);

	data->use_anticlip = !!xmms_config_property_get_int (cfgv);

	cfgv = xmms_xform_config_lookup (xform, "preamp");
	xmms_config_property_callback_set (cfgv,
	                                   xmms_replaygain_config_changed,
	                                   xform);

	data->preamp = pow (10.0, atof (xmms_config_property_get_string (cfgv)) / 20.0);

	cfgv = xmms_xform_config_lookup (xform, "enabled");
	xmms_config_property_callback_set (cfgv,
	                                   xmms_replaygain_config_changed,
	                                   xform);

	data->enabled = !!xmms_config_property_get_int (cfgv);

	xmms_xform_outdata_type_copy (xform);

	compute_gain (xform, data);

	fmt = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_FORMAT);

	switch (fmt) {
		case XMMS_SAMPLE_FORMAT_S8:
			data->apply = apply_s8;
			break;
		case XMMS_SAMPLE_FORMAT_U8:
			data->apply = apply_u8;
			break;
		case XMMS_SAMPLE_FORMAT_S16:
			data->apply = apply_s16;
			break;
		case XMMS_SAMPLE_FORMAT_U16:
			data->apply = apply_u16;
			break;
		case XMMS_SAMPLE_FORMAT_S32:
			data->apply = apply_s32;
			break;
		case XMMS_SAMPLE_FORMAT_U32:
			data->apply = apply_u32;
			break;
		case XMMS_SAMPLE_FORMAT_FLOAT:
			data->apply = apply_float;
			break;
		case XMMS_SAMPLE_FORMAT_DOUBLE:
			data->apply = apply_double;
			break;
		default:
			/* we shouldn't ever get here, since we told the daemon
			 * earlier about this list of supported formats.
			 */
			g_assert_not_reached ();
			break;
	}

	return TRUE;
}

static void
xmms_replaygain_destroy (xmms_xform_t *xform)
{
	xmms_config_property_t *cfgv;

	g_return_if_fail (xform);

	g_free (xmms_xform_private_data_get (xform));

	cfgv = xmms_xform_config_lookup (xform, "mode");
	xmms_config_property_callback_remove (cfgv,
	                                      xmms_replaygain_config_changed, xform);

	cfgv = xmms_xform_config_lookup (xform, "use_anticlip");
	xmms_config_property_callback_remove (cfgv,
	                                      xmms_replaygain_config_changed, xform);

	cfgv = xmms_xform_config_lookup (xform, "preamp");
	xmms_config_property_callback_remove (cfgv,
	                                      xmms_replaygain_config_changed, xform);

	cfgv = xmms_xform_config_lookup (xform, "enabled");
	xmms_config_property_callback_remove (cfgv,
	                                      xmms_replaygain_config_changed, xform);
}

static gint
xmms_replaygain_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                      xmms_error_t *error)
{
	xmms_replaygain_data_t *data;
	xmms_sample_format_t fmt;
	gint read;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	read = xmms_xform_read (xform, buf, len, error);

	if (!data->has_replaygain || !data->enabled) {
		return read;
	}

	fmt = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_FORMAT);
	len /= xmms_sample_size_get (fmt);

	data->apply (buf, len, data->gain);

	return read;
}

static gint64
xmms_replaygain_seek (xmms_xform_t *xform, gint64 samples,
                      xmms_xform_seek_mode_t whence, xmms_error_t *error)
{
	return xmms_xform_seek (xform, samples, whence, error);
}

static void
xmms_replaygain_config_changed (xmms_object_t *obj, xmmsv_t *_val, gpointer udata)
{
	const gchar *name;
	xmms_xform_t *xform = udata;
	xmms_replaygain_data_t *data;
	gboolean dirty = FALSE;
	const char *value;

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	name = xmms_config_property_get_name ((xmms_config_property_t *) obj);
	value = xmms_config_property_get_string ((xmms_config_property_t *) obj);

	if (!g_ascii_strcasecmp (name, "replaygain.mode")) {
		data->mode = parse_mode (value);
		dirty = TRUE;
	} else if (!g_ascii_strcasecmp (name,
	                                "replaygain.use_anticlip")) {
		data->use_anticlip = !!atoi (value);
		dirty = TRUE;
	} else if (!g_ascii_strcasecmp (name,
	                                "replaygain.preamp")) {
		data->preamp = pow (10.0, atof (value) / 20.0);
		dirty = TRUE;
	} else if (!g_ascii_strcasecmp (name, "replaygain.enabled")) {
		data->enabled = !!atoi (value);
	}

	if (dirty) {
		compute_gain (xform, data);
	}
}

static void
compute_gain (xmms_xform_t *xform, xmms_replaygain_data_t *data)
{
	gfloat s, p;
	const gchar *key_s, *key_p, *tmp;

	if (data->mode == XMMS_REPLAYGAIN_MODE_TRACK) {
		key_s = XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK;
		key_p = XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK;
	} else {
		key_s = XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM;
		key_p = XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM;
	}

	/** @todo should this be ints instead? */
	if (xmms_xform_metadata_get_str (xform, key_s, &tmp)) {
		s = atof (tmp);
	} else {
		s = 1.0;
	}

	if (xmms_xform_metadata_get_str (xform, key_p, &tmp)) {
		p = atof (tmp);
	} else {
		p = 1.0;
	}

	s *= data->preamp;

	if (data->use_anticlip && s * p > 1.0) {
		s = 1.0 / p;
	}

	data->gain = MIN (s, 15.0);

	/* This is NOT a value calculated by some scientist who has
	 * studied the ear for two decades.
	 * If you have a better value holler now, or keep your peace
	 * forever.
	 */
	data->has_replaygain = (fabs (data->gain - 1.0) > 0.001);
}

static xmms_replaygain_mode_t
parse_mode (const char *s)
{
	if (s && !g_ascii_strcasecmp (s, "album")) {
		return XMMS_REPLAYGAIN_MODE_ALBUM;
	} else {
		return XMMS_REPLAYGAIN_MODE_TRACK;
	}
}

static void
apply_s8 (void *buf, gint len, gfloat gain)
{
	xmms_samples8_t *samples = (xmms_samples8_t *) buf;
	gint i;

	for (i = 0; i < len; i++) {
		gfloat sample = samples[i] * gain;
		samples[i] = CLAMP (sample, XMMS_SAMPLES8_MIN,
		                    XMMS_SAMPLES8_MAX);
	}
}

static void
apply_u8 (void *buf, gint len, gfloat gain)
{
	xmms_sampleu8_t *samples = (xmms_sampleu8_t *) buf;
	gint i;

	for (i = 0; i < len; i++) {
		gfloat sample = samples[i] * gain;
		samples[i] = CLAMP (sample, 0, XMMS_SAMPLEU8_MAX);
	}
}

static void
apply_s16 (void *buf, gint len, gfloat gain)
{
	xmms_samples16_t *samples = (xmms_samples16_t *) buf;
	gint i;

	for (i = 0; i < len; i++) {
		gfloat sample = samples[i] * gain;
		samples[i] = CLAMP (sample, XMMS_SAMPLES16_MIN,
		                    XMMS_SAMPLES16_MAX);
	}
}

static void
apply_u16 (void *buf, gint len, gfloat gain)
{
	xmms_sampleu16_t *samples = (xmms_sampleu16_t *) buf;
	gint i;

	for (i = 0; i < len; i++) {
		gfloat sample = samples[i] * gain;
		samples[i] = CLAMP (sample, 0, XMMS_SAMPLEU16_MAX);
	}
}

static void
apply_s32 (void *buf, gint len, gfloat gain)
{
	xmms_samples32_t *samples = (xmms_samples32_t *) buf;
	gint i;

	for (i = 0; i < len; i++) {
		gdouble sample = samples[i] * gain;
		samples[i] = CLAMP (sample, XMMS_SAMPLES32_MIN,
		                    XMMS_SAMPLES32_MAX);
	}
}

static void
apply_u32 (void *buf, gint len, gfloat gain)
{
	xmms_sampleu32_t *samples = (xmms_sampleu32_t *) buf;
	gint i;

	for (i = 0; i < len; i++) {
		gdouble sample = samples[i] * gain;
		samples[i] = CLAMP (sample, 0, XMMS_SAMPLEU32_MAX);
	}
}

static void
apply_float (void *buf, gint len, gfloat gain)
{
	xmms_samplefloat_t *samples = (xmms_samplefloat_t *) buf;
	gint i;

	for (i = 0; i < len; i++) {
		samples[i] *= gain;
	}
}

static void
apply_double (void *buf, gint len, gfloat gain)
{
	xmms_sampledouble_t *samples = (xmms_sampledouble_t *) buf;
	gint i;

	for (i = 0; i < len; i++) {
		samples[i] *= gain;
	}
}

