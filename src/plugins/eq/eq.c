/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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
 * Equalizer-effect
 */

#include "xmms/xmms_defs.h"
#include "xmms/xmms_effectplugin.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"


#include <math.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>

static void xmms_eq_new (xmms_effect_t *effect);
static gboolean xmms_eq_format_set (xmms_effect_t *effect, xmms_audio_format_t *fmt);
static void xmms_eq_process (xmms_effect_t *effect, xmms_sample_t *buf, guint len);

#define MAX_CHANNELS 2
typedef struct xmms_eq_filter_St {
	gdouble a[3];
	gdouble b[3];
	gdouble input[MAX_CHANNELS][2], output[MAX_CHANNELS][2];
} xmms_eq_filter_t;

#define XMMS_EQ_BANDS 10
#define XMMS_EQ_Q 1.4142

typedef struct xmms_eq_priv_St {
	xmms_eq_filter_t filters[XMMS_EQ_BANDS];
	gdouble gains[XMMS_EQ_BANDS];
	xmms_config_value_t *configvals[XMMS_EQ_BANDS];
	guint channels;
} xmms_eq_priv_t;

static gdouble freqs[XMMS_EQ_BANDS] = { 0.0007142857,
					0.0014285714,
					0.0028344671,
					0.0056689342,
					0.0113378684,
					0.0226757369,
					0.0453514739,
					0.0907029478,
					0.1814058956,
					0.3628117913 };

/**
 * Calculate a filter with specified gain and relative center frequence
 */
static void
xmms_eq_calc_filter (xmms_eq_filter_t *filter, gdouble gain, gdouble relfreq)
{
	gdouble omega, sn, cs, alpha, temp1, temp2, A;

	A = sqrt (gain);
	omega = 2 * M_PI * relfreq;

	sn = sin (omega);
	cs = cos (omega);
	alpha = sn / (2.0 * XMMS_EQ_Q);
	temp1 = alpha * A;
	temp2 = alpha / A;

	filter->a[0] = 1.0 / (1.0 + temp2);
	filter->a[1] = (-2.0 * cs) * filter->a[0];
	filter->a[2] = (1.0 - temp2) * filter->a[0];
	filter->b[0] = (1.0 + temp1) * filter->a[0];
	filter->b[1] = filter->a[1];
	filter->b[2] = (1.0 - temp1) * filter->a[0];

}


xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;
	gchar buf[16];
	gint i;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_EFFECT, "equalizer",
					    "Equalizer effect " XMMS_VERSION,
					    "Equalizer effect");


	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_eq_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FORMAT_SET, xmms_eq_format_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_PROCESS, xmms_eq_process);

	for (i = 0; i < XMMS_EQ_BANDS; i++) {
		g_snprintf (buf, sizeof (buf), "gain%d", i);
		xmms_plugin_config_value_register (plugin, buf, "1.0", NULL, NULL);
	}

	return plugin;
}

static void
xmms_eq_configval_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_config_value_t *val = (xmms_config_value_t *)object;
	xmms_effect_t *effect = userdata;
	xmms_eq_priv_t *priv;
	const gchar *name, *ptr;
	gint i;

	priv = xmms_effect_private_data_get (effect);

	g_return_if_fail (priv);

	name = xmms_config_value_name_get (val);

	XMMS_DBG ("configval changed! %s => %f", name,
		  xmms_config_value_float_get (val));

	/* we are passed the full config key, not just the last token,
	 * which makes this code kinda ugly.
	 * fix when bug 97 has been resolved
	 */
	ptr = strrchr (name, '.');
	i = atoi (ptr + 5);

	XMMS_DBG ("changing filter #%d", i);

	priv->gains[i] = xmms_config_value_float_get (val);

	xmms_eq_calc_filter (&priv->filters[i], 
			     priv->gains[i], freqs[i]);

}

static void
xmms_eq_new (xmms_effect_t *effect) {
	xmms_eq_priv_t *priv;
	gint i;

	priv = g_new0 (xmms_eq_priv_t, 1);

	g_return_if_fail (priv);

	xmms_effect_private_data_set (effect, priv);

	for (i = 0; i < XMMS_EQ_BANDS; i++) {
		gchar buf[20];

		g_snprintf (buf, sizeof (buf), "gain%d", i);

		priv->configvals[i] =  xmms_plugin_config_lookup (xmms_effect_plugin_get (effect), buf);
		g_return_if_fail (priv->configvals[i]);

		xmms_config_value_callback_set (priv->configvals[i], 
						xmms_eq_configval_changed, 
						(gpointer) effect);

		priv->gains[i] = xmms_config_value_float_get (priv->configvals[i]);

		xmms_eq_calc_filter (&priv->filters[i], priv->gains[i], freqs[i]);
	}

}

static gboolean
xmms_eq_format_set (xmms_effect_t *effect, xmms_audio_format_t *fmt)
{
	xmms_eq_priv_t *priv = xmms_effect_private_data_get (effect);

	g_return_val_if_fail (priv, FALSE);

	priv->channels = fmt->channels;

	/* only support XMMS_SAMPLE_FORMAT_S16 atm */
	return fmt->format == XMMS_SAMPLE_FORMAT_S16 && fmt->channels <= MAX_CHANNELS;
}

static void
xmms_eq_process (xmms_effect_t *effect, xmms_sample_t *buf, guint len)
{
	gint i, j;
	xmms_eq_priv_t *priv = xmms_effect_private_data_get (effect);

	g_return_if_fail (priv);

	len /= xmms_sample_size_get (XMMS_SAMPLE_FORMAT_S16);
	for (i = 0; i < len; i++) {
		xmms_samples16_t *samples = (gint16 *)buf;
		gdouble val,tmp;
		gint chan;
		
		chan = i % priv->channels;

		val = ((gdouble)samples[i]) / ((1<<15) - 1);
		
		for (j=0; j<XMMS_EQ_BANDS; j++) {
			tmp = (priv->filters[j].b[0] * val) + 
				(priv->filters[j].b[1] * priv->filters[j].input[chan][0]) +
				(priv->filters[j].b[2] * priv->filters[j].input[chan][1]) -
				(priv->filters[j].a[1] * priv->filters[j].output[chan][0])-
				(priv->filters[j].a[2] * priv->filters[j].output[chan][1]);
			priv->filters[j].output[chan][1] = priv->filters[j].output[chan][0];
			priv->filters[j].output[chan][0] = tmp;
			priv->filters[j].input[chan][1] = priv->filters[j].input[chan][0];
			priv->filters[j].input[chan][0] = val;
			val = tmp;
		}

		val = CLAMP (val, -1.0, 1.0);

		samples[i] = val * ((1<<15) - 1);
	}
}
