/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"

#include "compress.h"

/* this should probably be in some xmms2 header file (maybe xmms_plugin.h) */

#include <gmodule.h>
typedef struct xmms_plugin_St {
	xmms_object_t object;
	GModule *module;
	GList *info_list;

	xmms_plugin_type_t type;
	const gchar *name;
	const gchar *shortname;
	const gchar *description;
	const gchar *version;
} xmms_plugin_t;

xmms_config_property_t *xmms_plugin_config_lookup (xmms_plugin_t *plugin,
                                                   const gchar *key);

/* helps avoid repetitive code when getting int config params */
#define NORMALIZE_CFG_GET_INT(xform, cfgv, param) \
	cfgv = xmms_xform_config_lookup (xform, #param); \
	param = xmms_config_property_get_int (cfgv);

#define NORMALIZE_BOOLEANIZE(var) var = !!var;

static gboolean dirty = FALSE;
static gboolean enabled;
static gboolean use_anticlip;
static int target;
static int max_gain;
static int smooth;
static int buckets;

static gboolean xmms_normalize_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_normalize_init (xmms_xform_t *xform);
static void xmms_normalize_destroy (xmms_xform_t *xform);
static gint xmms_normalize_read (xmms_xform_t *xform, xmms_sample_t *buf,
                                 gint len, xmms_error_t *error);
static void xmms_normalize_config_changed (xmms_object_t *obj,
                                           gconstpointer value,
                                           gpointer udata);

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

XMMS_XFORM_PLUGIN ("normalize",
                   "Volume normalizer",
                   XMMS_VERSION,
                   "Volume normalizer",
                   xmms_normalize_plugin_setup);

static gboolean
xmms_normalize_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;
	xmms_config_property_t *cfgv;
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
		                                            xmms_normalize_config_changed, NULL);
	}

	cfgv = xmms_plugin_config_lookup ((xmms_plugin_t *) xform_plugin, "enabled");
	xmms_config_property_callback_set (cfgv, xmms_normalize_config_changed,
	                                   NULL);
	return TRUE;
}

static gboolean
xmms_normalize_init (xmms_xform_t *xform)
{
	compress_t *compress;
	xmms_config_property_t *cfgv;

	g_return_val_if_fail (xform, FALSE);

	NORMALIZE_CFG_GET_INT (xform, cfgv, enabled);
	NORMALIZE_CFG_GET_INT (xform, cfgv, use_anticlip);
	NORMALIZE_CFG_GET_INT (xform, cfgv, target);
	NORMALIZE_CFG_GET_INT (xform, cfgv, max_gain);
	NORMALIZE_CFG_GET_INT (xform, cfgv, smooth);
	NORMALIZE_CFG_GET_INT (xform, cfgv, buckets);

	/* make booleans either 0 or 1 */
	NORMALIZE_BOOLEANIZE (enabled);
	NORMALIZE_BOOLEANIZE (use_anticlip);

	xmms_xform_outdata_type_copy (xform);

	compress = compress_new (use_anticlip,
	                         target,
	                         max_gain,
	                         smooth,
	                         buckets);

	xmms_xform_private_data_set (xform, compress);

	return TRUE;
}

static void
xmms_normalize_destroy (xmms_xform_t *xform)
{
	xmms_config_property_t *cfgv;

	g_return_if_fail (xform);

	compress_free (xmms_xform_private_data_get (xform));
}

static gint
xmms_normalize_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                     xmms_error_t *error)
{
	compress_t *compress;
	gint read;

	g_return_val_if_fail (xform, -1);

	compress = xmms_xform_private_data_get (xform);

	read = xmms_xform_read (xform, buf, len, error);

	if (dirty) {
		compress_reconfigure (compress,
		                      use_anticlip,
		                      target,
		                      max_gain,
		                      smooth,
		                      buckets);
		dirty = FALSE;
	}

	if (enabled) {
		compress_do (compress, buf, read);
	}

	return read;
}

static void
xmms_normalize_config_changed (xmms_object_t *obj, gconstpointer value,
                               gpointer udata)
{
	const gchar *name;
	xmms_xform_t *xform = udata;

	name = xmms_config_property_get_name ((xmms_config_property_t *) obj);

	if (!g_ascii_strcasecmp (name, "normalize.enabled")) {
		enabled = !!atoi (value);
		return;
	}

	if (!g_ascii_strcasecmp (name, "normalize.use_anticlip")) {
		use_anticlip = !!atoi (value);
	} else if (!g_ascii_strcasecmp (name, "normalize.target")) {
		target = atoi (value);
	} else if (!g_ascii_strcasecmp (name, "normalize.max_gain")) {
		max_gain = atoi (value);
	} else if (!g_ascii_strcasecmp (name, "normalize.smooth")) {
		smooth = atoi (value);
	} else if (!g_ascii_strcasecmp (name, "normalize.buckets")) {
		buckets = atoi (value);
	}

	/* reconfigure needed */
	dirty = TRUE;
}
