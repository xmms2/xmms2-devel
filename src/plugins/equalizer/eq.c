/** @file eq.c
 *  Equalizer effect plugin
 *
 *  Copyright (C) 2006-2009 XMMS2 Team
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
#include "xmms/xmms_config.h"
#include "xmms/xmms_log.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "iir.h"

#define EQ_BANDS_LEGACY 10

static gboolean xmms_eq_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_eq_init (xmms_xform_t *xform);
static void xmms_eq_destroy (xmms_xform_t *xform);
static gint xmms_eq_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                          xmms_error_t *error);
static gint64 xmms_eq_seek (xmms_xform_t *xform, gint64 offset,
                            xmms_xform_seek_mode_t whence, xmms_error_t *err);
static void xmms_eq_gain_changed (xmms_object_t *object, xmmsv_t *_data,
                                  gpointer userdata);
static void xmms_eq_config_changed (xmms_object_t *object, xmmsv_t *data, gpointer userdata);
static gfloat xmms_eq_gain_scale (gfloat gain, gboolean preamp);

typedef struct xmms_equalizer_priv_St {
	guint use_legacy;
	guint extra_filtering;
	guint bands;
	xmms_config_property_t *gain[EQ_MAX_BANDS];
	xmms_config_property_t *legacy[EQ_BANDS_LEGACY];
	gboolean enabled;
} xmms_equalizer_data_t;

XMMS_XFORM_PLUGIN ("equalizer",
                   "Equalizer effect",
                   XMMS_VERSION,
                   "Equalizer effect",
                   xmms_eq_plugin_setup);

static gboolean
xmms_eq_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;
	gchar buf[16];
	gint i;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_eq_init;
	methods.destroy = xmms_eq_destroy;
	methods.read = xmms_eq_read;
	methods.seek = xmms_eq_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_config_property_register (xform_plugin, "bands", "15",
	                                            NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin,
	                                            "extra_filtering", "0", NULL,
	                                            NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "use_legacy",
	                                            "1", NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "preamp", "0.0",
	                                            NULL, NULL);
	for (i=0; i<EQ_BANDS_LEGACY; i++) {
		g_snprintf (buf, sizeof (buf), "legacy%d", i);
		xmms_xform_plugin_config_property_register (xform_plugin, buf, "0.0",
		                                            NULL, NULL);
	}
	for (i=0; i<EQ_MAX_BANDS; i++) {
		g_snprintf (buf, sizeof (buf), "gain%02d", i);
		xmms_xform_plugin_config_property_register (xform_plugin, buf, "0.0",
		                                            NULL, NULL);
	}

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/pcm",
	                              XMMS_STREAM_TYPE_FMT_FORMAT,
	                              XMMS_SAMPLE_FORMAT_S16,
	                              XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                              48000,
	                              XMMS_STREAM_TYPE_END);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/pcm",
	                              XMMS_STREAM_TYPE_FMT_FORMAT,
	                              XMMS_SAMPLE_FORMAT_S16,
	                              XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                              44100,
	                              XMMS_STREAM_TYPE_END);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/pcm",
	                              XMMS_STREAM_TYPE_FMT_FORMAT,
	                              XMMS_SAMPLE_FORMAT_S16,
	                              XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                              22050,
	                              XMMS_STREAM_TYPE_END);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/pcm",
	                              XMMS_STREAM_TYPE_FMT_FORMAT,
	                              XMMS_SAMPLE_FORMAT_S16,
	                              XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                              11025,
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gboolean
xmms_eq_init (xmms_xform_t *xform)
{
	xmms_equalizer_data_t *priv;
	xmms_config_property_t *config;
	gint i, j, srate;
	gfloat gain;

	g_return_val_if_fail (xform, FALSE);

	priv = g_new0 (xmms_equalizer_data_t, 1);
	g_return_val_if_fail (priv, FALSE);

	xmms_xform_private_data_set (xform, priv);

	config = xmms_xform_config_lookup (xform, "enabled");
	g_return_val_if_fail (config, FALSE);
	xmms_config_property_callback_set (config, xmms_eq_config_changed, priv);
	priv->enabled = !!xmms_config_property_get_int (config);

	config = xmms_xform_config_lookup (xform, "bands");
	g_return_val_if_fail (config, FALSE);
	xmms_config_property_callback_set (config, xmms_eq_config_changed, priv);
	priv->bands = xmms_config_property_get_int (config);

	config = xmms_xform_config_lookup (xform, "extra_filtering");
	g_return_val_if_fail (config, FALSE);
	xmms_config_property_callback_set (config, xmms_eq_config_changed, priv);
	priv->extra_filtering = xmms_config_property_get_int (config);

	config = xmms_xform_config_lookup (xform, "use_legacy");
	g_return_val_if_fail (config, FALSE);
	xmms_config_property_callback_set (config, xmms_eq_config_changed, priv);
	priv->use_legacy = xmms_config_property_get_int (config);

	config = xmms_xform_config_lookup (xform, "preamp");
	g_return_val_if_fail (config, FALSE);
	xmms_config_property_callback_set (config, xmms_eq_gain_changed, priv);
	gain = xmms_config_property_get_float (config);

	for (i=0; i<EQ_CHANNELS; i++) {
		set_preamp (i, xmms_eq_gain_scale (gain, TRUE));
	}

	for (i=0; i<EQ_BANDS_LEGACY; i++) {
		gchar buf[16];

		g_snprintf (buf, sizeof (buf), "legacy%d", i);
		config = xmms_xform_config_lookup (xform, buf);
		g_return_val_if_fail (config, FALSE);

		priv->legacy[i] = config;
		xmms_config_property_callback_set (config, xmms_eq_gain_changed, priv);

		gain = xmms_config_property_get_float (config);
		if (priv->use_legacy) {
			for (j = 0; j < EQ_CHANNELS; j++) {
				set_gain (i, j, xmms_eq_gain_scale (gain, FALSE));
			}
		}
	}

	for (i=0; i<EQ_MAX_BANDS; i++) {
		gchar buf[16];

		g_snprintf (buf, sizeof (buf), "gain%02d", i);
		config = xmms_xform_config_lookup (xform, buf);
		g_return_val_if_fail (config, FALSE);

		priv->gain[i] = config;
		xmms_config_property_callback_set (config, xmms_eq_gain_changed, priv);

		gain = xmms_config_property_get_float (config);
		if (!priv->use_legacy) {
			for (j = 0; j < EQ_CHANNELS; j++) {
				set_gain (i, j, xmms_eq_gain_scale (gain, FALSE));
			}
		}
	}

	init_iir ();
	srate = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	if (priv->use_legacy) {
		config_iir (srate, EQ_BANDS_LEGACY, 1);
	} else {
		config_iir (srate, priv->bands, 0);
	}

	xmms_xform_outdata_type_copy (xform);

	XMMS_DBG ("Equalizer initialized successfully!");

	return TRUE;
}

static void
xmms_eq_destroy (xmms_xform_t *xform)
{
	xmms_config_property_t *config;
	gpointer priv;
	gchar buf[16];
	gint i;

	g_return_if_fail (xform);

	priv = xmms_xform_private_data_get (xform);

	config = xmms_xform_config_lookup (xform, "enabled");
	xmms_config_property_callback_remove (config, xmms_eq_config_changed, priv);

	config = xmms_xform_config_lookup (xform, "bands");
	xmms_config_property_callback_remove (config, xmms_eq_config_changed, priv);

	config = xmms_xform_config_lookup (xform, "extra_filtering");
	xmms_config_property_callback_remove (config, xmms_eq_config_changed, priv);

	config = xmms_xform_config_lookup (xform, "use_legacy");
	xmms_config_property_callback_remove (config, xmms_eq_config_changed, priv);

	config = xmms_xform_config_lookup (xform, "preamp");
	xmms_config_property_callback_remove (config, xmms_eq_gain_changed, priv);

	for (i=0; i<EQ_BANDS_LEGACY; i++) {
		g_snprintf (buf, sizeof (buf), "legacy%d", i);
		config = xmms_xform_config_lookup (xform, buf);
		xmms_config_property_callback_remove (config, xmms_eq_gain_changed, priv);
	}

	for (i=0; i<EQ_MAX_BANDS; i++) {
		g_snprintf (buf, sizeof (buf), "gain%02d", i);
		config = xmms_xform_config_lookup (xform, buf);
		xmms_config_property_callback_remove (config, xmms_eq_gain_changed, priv);
	}

	g_free (priv);
}

static gint
xmms_eq_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
              xmms_error_t *error)
{
	xmms_equalizer_data_t *priv;
	gint read, chan;

	g_return_val_if_fail (xform, -1);

	priv = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (priv, -1);

	read = xmms_xform_read (xform, buf, len, error);
	chan = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_CHANNELS);
	if (read > 0 && priv->enabled) {
		iir (buf, read, chan, priv->extra_filtering);
	}

	return read;
}

static gint64
xmms_eq_seek (xmms_xform_t *xform, gint64 offset, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	return xmms_xform_seek (xform, offset, whence, err);
}

static void
xmms_eq_gain_changed (xmms_object_t *object, xmmsv_t *_data,
                      gpointer userdata)
{
	xmms_config_property_t *val;
	xmms_equalizer_data_t *priv;
	const gchar *name;
	gint i;
	gfloat gain;

	g_return_if_fail (object);
	g_return_if_fail (userdata);

        val = (xmms_config_property_t *) object;
	priv = userdata;

	name = xmms_config_property_get_name (val);

	XMMS_DBG ("gain value changed! %s => %f", name,
	          xmms_config_property_get_float (val));

	gain = xmms_config_property_get_float (val);
	if (gain < -20.0 || gain > 20.0) {
		gchar buf[20];

		gain = CLAMP (gain, -20.0, 20.0);
		g_snprintf (buf, sizeof (buf), "%g", gain);

		xmms_config_property_set_data (val, buf);
	}

	/* we are passed the full config key, not just the last token,
	 * which makes this code kinda ugly.
	 * fix when bug 97 has been resolved
	 */
	name = strrchr (name, '.') + 1;

	if (!strcmp (name, "preamp")) {
		/* scale the -20.0 - 20.0 value to correct one */
		for (i=0; i<EQ_CHANNELS; i++) {
			set_preamp (i, xmms_eq_gain_scale (gain, TRUE));
		}
	} else {
		gint band = -1;

		if (!strncmp (name, "gain", 4) && !priv->use_legacy) {
			band = atoi (name + 4);
		} else if (!strncmp (name, "legacy", 6) && priv->use_legacy) {
			band = atoi (name + 6);
		}

		if (band >= 0) {
			/* scale the -20.0 - 20.0 value to correct one */
			for (i=0; i<EQ_CHANNELS; i++) {
				set_gain (band, i, xmms_eq_gain_scale (gain, FALSE));
			}
		}
	}
}

static void
xmms_eq_config_changed (xmms_object_t * object, xmmsv_t *_data, gpointer userdata)
{
	xmms_config_property_t *val;
	xmms_equalizer_data_t *priv;
	const gchar *name;
	gint value, i, j;

	g_return_if_fail (object);
	g_return_if_fail (userdata);

	val = (xmms_config_property_t *) object;
	priv = (xmms_equalizer_data_t *) userdata;

	name = xmms_config_property_get_name (val);
	value = xmms_config_property_get_int (val);

	XMMS_DBG ("config value changed! %s => %d", name, value);

	/* we are passed the full config key, not just the last token,
	 * which makes this code kinda ugly.
	 * fix when bug 97 has been resolved
	 */
	name = strrchr (name, '.') + 1;

	if (!strcmp (name, "enabled")) {
		priv->enabled = !!value;
	} else if (!strcmp (name, "extra_filtering")) {
		priv->extra_filtering = value;
	} else if (!strcmp (name, "use_legacy")) {
		gfloat gain;

		priv->use_legacy = value;
		if (priv->use_legacy) {
			for (i=0; i<EQ_BANDS_LEGACY; i++) {
				gain = xmms_config_property_get_float (priv->legacy[i]);
				for (j=0; j<EQ_CHANNELS; j++) {
					set_gain (j, i, xmms_eq_gain_scale (gain, FALSE));
				}
			}
		} else {
			for (i=0; i<priv->bands; i++) {
				gain = xmms_config_property_get_float (priv->gain[i]);
				for (j=0; j<EQ_CHANNELS; j++) {
					set_gain (j, i, xmms_eq_gain_scale (gain, FALSE));
				}
			}
		}
	} else if (!strcmp (name, "bands")) {
		if (value != 10 && value != 15 && value != 25 && value != 31) {
			gchar buf[20];

			/* Illegal new value so we restore the old value */
			g_snprintf (buf, sizeof (buf), "%d", priv->bands);
			xmms_config_property_set_data (val, buf);
		} else {
			priv->bands = value;
			for (i=0; i<EQ_MAX_BANDS; i++) {
				xmms_config_property_set_data (priv->gain[i], "0.0");
				if (!priv->use_legacy) {
					for (j=0; j<EQ_CHANNELS; j++) {
						set_gain (j, i, xmms_eq_gain_scale (0.0, FALSE));
					}
				}
			}
		}
	}
}

static gfloat
xmms_eq_gain_scale (gfloat gain, gboolean preamp)
{
	if (preamp) {
		return (9.9999946497217584440165E-01 *
		        exp (6.9314738656671842642609E-02 * gain)
		        + 3.7119444716771825623636E-07);
	} else {
		return (2.5220207857061455181125E-01 *
		        exp (8.0178361802353992349168E-02 * gain)
		        - 2.5220207852836562523180E-01);
	}
}
