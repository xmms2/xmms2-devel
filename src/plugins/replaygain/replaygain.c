/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2004, 2005	Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include "xmms/xmms_defs.h"
#include "xmms/xmms_effectplugin.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"

#include <math.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>

/**
 * Replaygain modes.
 */
typedef enum {
	XMMS_REPLAYGAIN_MODE_TRACK,
	XMMS_REPLAYGAIN_MODE_ALBUM
	/** @todo implement dynamic replaygain */
} xmms_replaygain_mode_t;

typedef struct xmms_replaygain_data_St {
	xmms_sample_format_t sample_format;
	xmms_replaygain_mode_t mode;
	gboolean use_anticlip;
	gfloat gain;
	gboolean has_replaygain;
	guint current_mlib_entry;
} xmms_replaygain_data_t;

static void xmms_replaygain_new (xmms_effect_t *effect);
static gboolean xmms_replaygain_format_set (xmms_effect_t *effect, xmms_audio_format_t *fmt);
static void xmms_replaygain_current_mlib_entry (xmms_effect_t *effect, xmms_medialib_entry_t entry);
static void xmms_replaygain_process (xmms_effect_t *effect, xmms_sample_t *buf, guint len);
static void xmms_replaygain_destroy (xmms_effect_t *effect);
static void xmms_replaygain_config_changed (xmms_object_t *obj, gconstpointer value, gpointer udata);
static void compute_replaygain (xmms_replaygain_data_t *data);

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_EFFECT,
	                          XMMS_EFFECT_PLUGIN_API_VERSION,
	                          "replaygain",
	                          "Replaygain effect " XMMS_VERSION,
	                          "Replaygain effect");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW,
	                        xmms_replaygain_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FORMAT_SET,
	                        xmms_replaygain_format_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CURRENT_MEDIALIB_ENTRY,
	                        xmms_replaygain_current_mlib_entry);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_PROCESS,
	                        xmms_replaygain_process);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY,
	                        xmms_replaygain_destroy);

	xmms_plugin_config_property_register (plugin, "mode", "track",
	                                      NULL, NULL);
	xmms_plugin_config_property_register (plugin, "use_anticlip", "1",
	                                      NULL, NULL);

	return plugin;
}

static void
xmms_replaygain_new (xmms_effect_t *effect)
{
	xmms_replaygain_data_t *data;
	xmms_plugin_t *plugin;
	xmms_config_property_t *cfgv;

	g_return_if_fail (effect);

	data = g_new0 (xmms_replaygain_data_t, 1);
	g_return_if_fail (data);

	xmms_effect_private_data_set (effect, data);

	plugin = xmms_effect_plugin_get (effect);

	cfgv = xmms_plugin_config_lookup (plugin, "mode");
	xmms_config_property_callback_set (cfgv,
	                                   xmms_replaygain_config_changed,
	                                   effect);

	cfgv = xmms_plugin_config_lookup (plugin, "use_anticlip");
	xmms_config_property_callback_set (cfgv,
	                                   xmms_replaygain_config_changed,
	                                   effect);
}

static void
xmms_replaygain_destroy (xmms_effect_t *effect)
{
	xmms_plugin_t *plugin;
	xmms_config_property_t *cfgv;

	g_return_if_fail (effect);

	g_free (xmms_effect_private_data_get (effect));

	plugin = xmms_effect_plugin_get (effect);

	cfgv = xmms_plugin_config_lookup (plugin, "mode");
	xmms_config_property_callback_remove (cfgv,
	                                      xmms_replaygain_config_changed);

	cfgv = xmms_plugin_config_lookup (plugin, "use_anticlip");
	xmms_config_property_callback_remove (cfgv,
	                                      xmms_replaygain_config_changed);
}

static gboolean
xmms_replaygain_format_set (xmms_effect_t *effect,
                            xmms_audio_format_t *fmt)
{
	xmms_replaygain_data_t *data;

	g_return_val_if_fail (effect, FALSE);
	g_return_val_if_fail (fmt, FALSE);

	data = xmms_effect_private_data_get (effect);
	g_return_val_if_fail (data, FALSE);

	data->sample_format = fmt->format;

	return TRUE;
}

static void
xmms_replaygain_current_mlib_entry (xmms_effect_t *effect,
                                    xmms_medialib_entry_t entry)
{
	xmms_replaygain_data_t *data;
	g_return_if_fail (effect);

	data = xmms_effect_private_data_get (effect);
	g_return_if_fail (data);

	data->current_mlib_entry = entry;

	compute_replaygain (data);
}

static void
xmms_replaygain_process (xmms_effect_t *effect,
                         xmms_sample_t *buf, guint len)
{
	xmms_replaygain_data_t *data;
	guint i;

	g_return_if_fail (effect);

	data = xmms_effect_private_data_get (effect);
	g_return_if_fail (data);

	if (!data->has_replaygain) {
		return;
	}

	len /= xmms_sample_size_get (data->sample_format);

	switch (data->sample_format) {
		case XMMS_SAMPLE_FORMAT_S8:
			for (i = 0; i < len; i++) {
				xmms_samples8_t *samples = (xmms_samples8_t *) buf;
				gfloat sample = samples[i] * data->gain;

				samples[i] = CLAMP (sample, XMMS_SAMPLES8_MIN,
				                    XMMS_SAMPLES8_MAX);
			}

			break;
		case XMMS_SAMPLE_FORMAT_U8:
			for (i = 0; i < len; i++) {
				xmms_sampleu8_t *samples = (xmms_sampleu8_t *) buf;
				gfloat sample = samples[i] * data->gain;

				samples[i] = CLAMP (sample, 0, XMMS_SAMPLEU8_MAX);
			}

			break;
		case XMMS_SAMPLE_FORMAT_S16:
			for (i = 0; i < len; i++) {
				xmms_samples16_t *samples = (xmms_samples16_t *) buf;
				gfloat sample = samples[i] * data->gain;

				samples[i] = CLAMP (sample, XMMS_SAMPLES16_MIN,
				                    XMMS_SAMPLES16_MAX);
			}

			break;
		case XMMS_SAMPLE_FORMAT_U16:
			for (i = 0; i < len; i++) {
				xmms_sampleu16_t *samples = (xmms_sampleu16_t *) buf;
				gfloat sample = samples[i] * data->gain;

				samples[i] = CLAMP (sample, 0, XMMS_SAMPLEU16_MAX);
			}

			break;
		case XMMS_SAMPLE_FORMAT_S32:
			for (i = 0; i < len; i++) {
				xmms_samples32_t *samples = (xmms_samples32_t *) buf;
				gfloat sample = samples[i] * data->gain;

				samples[i] = CLAMP (sample, XMMS_SAMPLES32_MIN,
				                    XMMS_SAMPLES32_MAX);
			}

			break;
		case XMMS_SAMPLE_FORMAT_U32:
			for (i = 0; i < len; i++) {
				xmms_sampleu32_t *samples = (xmms_sampleu32_t *) buf;
				gfloat sample = samples[i] * data->gain;

				samples[i] = CLAMP (sample, 0, XMMS_SAMPLEU32_MAX);
			}

			break;
		case XMMS_SAMPLE_FORMAT_FLOAT:
			for (i = 0; i < len; i++) {
				xmms_samplefloat_t *samples = (xmms_samplefloat_t *) buf;
				samples[i] *= data->gain;
			}

			break;
		case XMMS_SAMPLE_FORMAT_DOUBLE:
			for (i = 0; i < len; i++) {
				xmms_sampledouble_t *samples = (xmms_sampledouble_t *) buf;
				samples[i] *= data->gain;
			}
		default:
			break;
	}
}

static void
xmms_replaygain_config_changed (xmms_object_t *obj, gconstpointer value,
                                gpointer udata)
{
	const gchar *name;
	xmms_effect_t *effect = udata;
	xmms_replaygain_data_t *data;

	data = xmms_effect_private_data_get (effect);
	g_return_if_fail (data);

	name = xmms_config_property_get_name ((xmms_config_property_t *) obj);

	if (!g_ascii_strcasecmp (name, "effect.replaygain.mode")) {
		if (!g_ascii_strcasecmp (value, "album")) {
			data->mode = XMMS_REPLAYGAIN_MODE_ALBUM;
		} else {
			data->mode = XMMS_REPLAYGAIN_MODE_TRACK;
		}
	} else if (!g_ascii_strcasecmp (name,
	                                "effect.replaygain.use_anticlip")) {
		data->use_anticlip = !!atoi (value);
	}

	compute_replaygain (data);
}

static void
compute_replaygain (xmms_replaygain_data_t *data)
{
	gfloat s, p;
	gchar *key_s, *key_p;
	gchar *tmp;
	xmms_medialib_session_t *session;

	if (data->mode == XMMS_REPLAYGAIN_MODE_TRACK) {
		key_s = XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK;
		key_p = XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK;
	} else {
		key_s = XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM;
		key_p = XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM;
	}

	session = xmms_medialib_begin ();

	/** @todo should this be ints instead? */
	tmp = xmms_medialib_entry_property_get_str (session,
	                                            data->current_mlib_entry,
	                                            key_s);
	s = tmp ? atof (tmp) : 1.0;
	g_free (tmp);

	tmp = xmms_medialib_entry_property_get_str (session,
	                                            data->current_mlib_entry,
	                                            key_p);
	p = tmp ? atof (tmp) : 1.0;
	g_free (tmp);

	xmms_medialib_end (session);

	s *= 2; /* 6db pre-amp */

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
