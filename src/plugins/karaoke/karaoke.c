/** @file karaoke.c
 *  Voice removal effect plugin
 *
 *  Copyright (C) 2008-2012 XMMS2 Team
 *
 *  Heavily based on DeFX plugin created for XMMS:
 *  DeFX Copyright (C) 2002 Franco Catrin L. <ancelot@directo.cl>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"

#include <stdlib.h>
#include <math.h>
#include <glib.h>

/* Macro to saturate signal */
#define SAT(x) (short) (x>32767?32767:(x<-32768?-32768:x));

typedef struct {
	gboolean enabled;

	/* samplerate and channels */
	gint srate;
	gint channels;

	/* effect level */
	gint level;
	/* filtered mono signal leve */
	gint mono_level;
	/* center band and bandwidth of filter */
	gdouble band, width;
	/* filter coefficients */
	gdouble a, b, c;
	/* saved filter values */
	gdouble y1, y2;
} xmms_karaoke_data_t;

static gboolean xmms_karaoke_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_karaoke_init (xmms_xform_t *xform);
static void xmms_karaoke_destroy (xmms_xform_t *xform);
static void xmms_karaoke_config_changed (xmms_object_t *object, xmmsv_t *d, gpointer userdata);
static gint xmms_karaoke_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                               xmms_error_t *err);
static gint64 xmms_karaoke_seek (xmms_xform_t *xform, gint64 offset,
                                 xmms_xform_seek_mode_t whence,
                                 xmms_error_t *err);
static void xmms_karaoke_update_coeffs (xmms_karaoke_data_t *data);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("karaoke",
                   "Karaoke effect", XMMS_VERSION,
                   "Voice removal effect plugin",
                   xmms_karaoke_plugin_setup);

static gboolean
xmms_karaoke_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_karaoke_init;
	methods.destroy = xmms_karaoke_destroy;
	methods.read = xmms_karaoke_read;
	methods.seek = xmms_karaoke_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_config_property_register (xform_plugin, "level", "32",
	                                            NULL, NULL);

	xmms_xform_plugin_config_property_register (xform_plugin, "mono_level", "26",
	                                            NULL, NULL);

	xmms_xform_plugin_config_property_register (xform_plugin, "band", "100.0",
	                                            NULL, NULL);

	xmms_xform_plugin_config_property_register (xform_plugin, "width", "92.0",
	                                            NULL, NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/pcm",
	                              XMMS_STREAM_TYPE_FMT_FORMAT,
	                              XMMS_SAMPLE_FORMAT_S16,
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gboolean
xmms_karaoke_init (xmms_xform_t *xform)
{
	xmms_karaoke_data_t *priv;
	xmms_config_property_t *config;

	g_return_val_if_fail (xform, FALSE);

	priv = g_new0 (xmms_karaoke_data_t, 1);
	xmms_xform_private_data_set (xform, priv);

	config = xmms_xform_config_lookup (xform, "enabled");
	g_return_val_if_fail (config, FALSE);
	xmms_config_property_callback_set (config, xmms_karaoke_config_changed, priv);
	priv->enabled = !!xmms_config_property_get_int (config);

	config = xmms_xform_config_lookup (xform, "level");
	g_return_val_if_fail (config, FALSE);
	xmms_config_property_callback_set (config, xmms_karaoke_config_changed, priv);
	priv->level = xmms_config_property_get_int (config);

	config = xmms_xform_config_lookup (xform, "mono_level");
	g_return_val_if_fail (config, FALSE);
	xmms_config_property_callback_set (config, xmms_karaoke_config_changed, priv);
	priv->mono_level = xmms_config_property_get_int (config);

	config = xmms_xform_config_lookup (xform, "band");
	g_return_val_if_fail (config, FALSE);
	xmms_config_property_callback_set (config, xmms_karaoke_config_changed, priv);
	priv->band = xmms_config_property_get_float (config);

	config = xmms_xform_config_lookup (xform, "width");
	g_return_val_if_fail (config, FALSE);
	xmms_config_property_callback_set (config, xmms_karaoke_config_changed, priv);
	priv->width = xmms_config_property_get_float (config);

	priv->srate = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	priv->channels = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_CHANNELS);

	xmms_karaoke_update_coeffs (priv);
	xmms_xform_outdata_type_copy (xform);

	return TRUE;
}

static void
xmms_karaoke_destroy (xmms_xform_t *xform)
{
	xmms_karaoke_data_t *data;
	xmms_config_property_t *config;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	config = xmms_xform_config_lookup (xform, "enabled");
	xmms_config_property_callback_remove (config, xmms_karaoke_config_changed, data);

	config = xmms_xform_config_lookup (xform, "level");
	xmms_config_property_callback_remove (config, xmms_karaoke_config_changed, data);

	config = xmms_xform_config_lookup (xform, "mono_level");
	xmms_config_property_callback_remove (config, xmms_karaoke_config_changed, data);

	config = xmms_xform_config_lookup (xform, "band");
	xmms_config_property_callback_remove (config, xmms_karaoke_config_changed, data);

	config = xmms_xform_config_lookup (xform, "width");
	xmms_config_property_callback_remove (config, xmms_karaoke_config_changed, data);

	g_free (data);
}

static void
xmms_karaoke_config_changed (xmms_object_t *object, xmmsv_t *_val, gpointer userdata)
{
	xmms_config_property_t *val;
	xmms_karaoke_data_t *data;
	const gchar *name;

	g_return_if_fail (object);
	g_return_if_fail (userdata);

	val = (xmms_config_property_t *) object;
	data = (xmms_karaoke_data_t *) userdata;

	name = xmms_config_property_get_name (val);

	XMMS_DBG ("config value changed! %s", name);
	/* we are passed the full config key, not just the last token,
	 * which makes this code kinda ugly.
	 * fix when bug 97 has been resolved
	 */
	name = strrchr (name, '.') + 1;

	if (!strcmp (name, "enabled")) {
		data->enabled = !!xmms_config_property_get_int (val);
	} else if (!strcmp (name, "level")) {
		data->level = xmms_config_property_get_int (val);
		data->level = CLAMP (data->level, 0, 32);
	} else if (!strcmp (name, "mono_level")) {
		data->mono_level = xmms_config_property_get_int (val);
		data->mono_level = CLAMP (data->mono_level, 0, 32);
	} else if (!strcmp (name, "band")) {
		data->band = xmms_config_property_get_float (val);
		xmms_karaoke_update_coeffs (data);
	} else if (!strcmp (name, "width")) {
		data->width = xmms_config_property_get_float (val);;
		xmms_karaoke_update_coeffs (data);
	}
}

static gint
xmms_karaoke_read (xmms_xform_t *xform, xmms_sample_t *buffer, gint len,
                   xmms_error_t *error)
{
	xmms_karaoke_data_t *data;
	gint16 *buf = buffer;
	gint l, r, nl, nr, out, tmp;
	gdouble y;
	gint i, ret;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = xmms_xform_read (xform, buffer, len, error);

	if (!data->enabled || data->channels < 2 || ret <= 0) {
		return ret;
	}

	for (i=0; i<(ret/2); i+=data->channels) {
		/* get left and right inputs */
		l = buf[i];
		r = buf[i+1];

		/* Do filtering */
		tmp = (l+r) >> 1;
		y = (data->a*tmp - data->b*data->y1) - data->c*data->y2;
		data->y2 = data->y1;
		data->y1 = y;

		/* filter mono signal */
		out = (int) (y * (data->mono_level/10.0));
		out = SAT (out);
		out = (out*data->level) >> 5;

		/* now cut the center! */
		nl = l - ((r*data->level) >> 5) + out;
		nr = r - ((l*data->level) >> 5) + out;

		/* output filtered signal */
		buf[i]   = SAT (nl);
		buf[i+1] = SAT (nr);
	}

	return ret;
}

static gint64
xmms_karaoke_seek (xmms_xform_t *xform, gint64 offset,
                   xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_karaoke_data_t *data;
	gint64 ret;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = xmms_xform_seek (xform, offset, whence, err);
	if (ret > 0) {
		xmms_karaoke_update_coeffs (data);
	}

	return ret;
}

static void
xmms_karaoke_update_coeffs (xmms_karaoke_data_t *data)
{
	double A, B, C;

	g_return_if_fail (data);

	C = exp (-2 * M_PI * data->width / data->srate);
	B = -4*C / (1+C) * cos (2 * M_PI * data->band / data->srate);
	A = sqrt (1- B*B / (4*C)) * (1-C);

	data->a = A;
	data->b = B;
	data->c = C;

	data->y1 = 0.0;
	data->y2 = 0.0;
}

