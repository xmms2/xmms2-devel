/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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
 * Normalize
 */

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_config.h>
#include <xmms/xmms_log.h>
#include <stdlib.h>

#include "compress.h"

static const struct {
	const gchar *key;
	const gchar *value;
} config_params[] = {
	{"use_anticlip", "1"},
	{"target", "25000"},
	{"max_gain", "32"},
	{"smooth", "8"},
	{"buckets", "400"}
};

typedef struct {
	compress_t *compress;
	gboolean dirty;
	gboolean enabled;
	gboolean use_anticlip;
	int target;
	int max_gain;
	int smooth;
	int buckets;
} xmms_normalize_data_t;

static gboolean xmms_normalize_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_normalize_init (xmms_xform_t *xform);
static void xmms_normalize_destroy (xmms_xform_t *xform);
static gint xmms_normalize_read (xmms_xform_t *xform, xmms_sample_t *buf,
                                 gint len, xmms_error_t *error);
static void xmms_normalize_config_changed (xmms_object_t *obj, xmmsv_t *value, gpointer udata);

XMMS_XFORM_PLUGIN_DEFINE ("normalize",
                          "Volume normalizer",
                          XMMS_VERSION,
                          "Volume normalizer",
                          xmms_normalize_plugin_setup);

static gboolean
xmms_normalize_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;
	unsigned i;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_normalize_init;
	methods.destroy = xmms_normalize_destroy;
	methods.read = xmms_normalize_read;
	methods.seek = xmms_xform_seek; /* we're not using this */

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/pcm",
	                              XMMS_STREAM_TYPE_FMT_FORMAT,
	                              XMMS_SAMPLE_FORMAT_S16,
	                              XMMS_STREAM_TYPE_END);


	for (i = 0; i < G_N_ELEMENTS (config_params); i++) {
		xmms_xform_plugin_config_property_register (xform_plugin,
		                                            config_params[i].key,
		                                            config_params[i].value,
		                                            NULL, NULL);
	}

	return TRUE;
}

static gboolean
xmms_normalize_init (xmms_xform_t *xform)
{
	xmms_config_property_t *cfgv;
	xmms_normalize_data_t *data;
	int i;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_normalize_data_t, 1);

	for (i = 0; i < G_N_ELEMENTS (config_params); i++) {
		cfgv = xmms_xform_config_lookup (xform, config_params[i].key);
		xmms_config_property_callback_set (cfgv,
		                                   xmms_normalize_config_changed,
		                                   data);

		xmms_normalize_config_changed (XMMS_OBJECT (cfgv), NULL, data);
	}

	xmms_xform_outdata_type_copy (xform);

	data->dirty = FALSE;

	data->compress = compress_new (data->use_anticlip,
	                               data->target,
	                               data->max_gain,
	                               data->smooth,
	                               data->buckets);

	xmms_xform_private_data_set (xform, data);

	return TRUE;
}

static void
xmms_normalize_destroy (xmms_xform_t *xform)
{
	xmms_normalize_data_t *data;
	xmms_config_property_t *cfgv;
	int i;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);

	g_return_if_fail (data);

	compress_free (data->compress);

	for (i = 0; i < G_N_ELEMENTS (config_params); i++) {
		cfgv = xmms_xform_config_lookup (xform, config_params[i].key);
		xmms_config_property_callback_remove (cfgv,
		                                      xmms_normalize_config_changed,
		                                      data);
	}

	g_free (data);

}

static gint
xmms_normalize_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                     xmms_error_t *error)
{
	xmms_normalize_data_t *data;
	gint read;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);

	read = xmms_xform_read (xform, buf, len, error);

	if (read > 0) {
		if (data->dirty) {
			compress_reconfigure (data->compress,
			                      data->use_anticlip,
			                      data->target,
			                      data->max_gain,
			                      data->smooth,
			                      data->buckets);
			data->dirty = FALSE;
		}

		compress_do (data->compress, buf, read);
	}

	return read;
}

static void
xmms_normalize_config_changed (xmms_object_t *obj, xmmsv_t *_value, gpointer udata)
{
	xmms_normalize_data_t *data = udata;
	const gchar *name;
	gint value;

	name = xmms_config_property_get_name ((xmms_config_property_t *) obj);
	value = xmms_config_property_get_int ((xmms_config_property_t *) obj);

	if (!g_ascii_strcasecmp (name, "normalize.use_anticlip")) {
		data->use_anticlip = !!value;
	} else if (!g_ascii_strcasecmp (name, "normalize.target")) {
		data->target = value;
	} else if (!g_ascii_strcasecmp (name, "normalize.max_gain")) {
		data->max_gain = value;
	} else if (!g_ascii_strcasecmp (name, "normalize.smooth")) {
		data->smooth = value;
	} else if (!g_ascii_strcasecmp (name, "normalize.buckets")) {
		data->buckets = value;
	}

	/* reconfigure needed */
	data->dirty = TRUE;
}
