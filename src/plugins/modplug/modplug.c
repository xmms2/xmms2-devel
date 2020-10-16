/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */


#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_sample.h>
#include <xmms/xmms_medialib.h>
#include <xmms/xmms_log.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "modplug_compat.h"

/*
 * Type definitions
 */
static const struct {
	const gchar *key;
	const gchar *value;
} config_params[] = {
	{"freq", "44100"},
	{"resample", "fir"},
	{"enable_oversampling", "1"},
	{"enable_noise_reduction", "1"},
	{"enable_reverb", "0"},
	{"enable_megabass", "0"},
	{"enable_surround", "0"},
	{"reverb_depth", "100"},
	{"reverb_delay", "80"},
	{"bass_amount", "40"},
	{"bass_range", "30"},
	{"surround_depth", "10"},
	{"surround_delay", "40"},
	{"loop", "0"}
};

typedef struct xmms_modplug_data_St {
	ModPlug_Settings settings;
	ModPlugFile *mod;
	GString *buffer;
} xmms_modplug_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_modplug_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_modplug_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static void xmms_modplug_destroy (xmms_xform_t *xform);
static gboolean xmms_modplug_init (xmms_xform_t *xform);
static gint64 xmms_modplug_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *error);
static void xmms_modplug_config_changed (xmms_object_t *obj, xmmsv_t *_value, gpointer udata);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN_DEFINE ("modplug",
                          "MODPLUG decoder ",
                          XMMS_VERSION,
                          "Module file decoder",
                          xmms_modplug_plugin_setup);

static gboolean
xmms_modplug_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;
	int i;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_modplug_init;
	methods.destroy = xmms_modplug_destroy;
	methods.read = xmms_modplug_read;
	methods.seek = xmms_modplug_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	/*
	xmms_plugin_info_add (plugin, "URL", "http://xmms2.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	xmms_plugin_info_add (plugin, "License", "GPL");
	*/

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/mod",
	                              XMMS_STREAM_TYPE_END);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/xm",
	                              XMMS_STREAM_TYPE_END);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/s3m",
	                              XMMS_STREAM_TYPE_END);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/it",
	                              XMMS_STREAM_TYPE_END);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/med",
	                              XMMS_STREAM_TYPE_END);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/amf",
	                              XMMS_STREAM_TYPE_END);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/umx",
	                              XMMS_STREAM_TYPE_END);

	xmms_magic_add ("Fasttracker II module", "audio/xm",
	                "0 string Extended Module:", NULL);
	xmms_magic_add ("ScreamTracker III module", "audio/s3m",
	                "44 string SCRM", NULL);
	xmms_magic_add ("Impulse Tracker module", "audio/it",
	                "0 string IMPM", NULL);
	xmms_magic_add ("MED module", "audio/med",
	                "0 string MMD", NULL);
	xmms_magic_add ("AMF module", "audio/amf",
	                "0 string AMF", NULL);

	/* http://www.unrealwiki.com/wiki/Package_File_Format */
	xmms_magic_add ("Unreal Engine package", "audio/umx",
	                "0 belong 0xc1832a9e", NULL);

	/* these are for all (not all but should be most) various types of .mod files */
	xmms_magic_add ("4-channel Protracker module", "audio/mod",
	                "1080 string M.K.", NULL);
	xmms_magic_add ("4-channel Protracker module", "audio/mod",
	                "1080 string M!K!", NULL);
	xmms_magic_add ("4-channel Startracker module", "audio/mod",
	                "1080 string FLT4", NULL);
	xmms_magic_add ("8-channel Startracker module", "audio/mod",
	                "1080 string FLT8", NULL);
	xmms_magic_add ("4-channel Fasttracker module", "audio/mod",
	                "1080 string 4CHN", NULL);
	xmms_magic_add ("6-channel Fasttracker module", "audio/mod",
	                "1080 string 6CHN", NULL);
	xmms_magic_add ("8-channel Fasttracker module", "audio/mod",
	                "1080 string 8CHN", NULL);
	xmms_magic_add ("8-channel Octalyzer module", "audio/mod",
	                "1080 string CD81", NULL);
	xmms_magic_add ("8-channel Octalyzer module", "audio/mod",
	                "1080 string OKTA", NULL);
	xmms_magic_add ("16-channel Taketracker module", "audio/mod",
	                "1080 string 16CN", NULL);
	xmms_magic_add ("32-channel Taketracker module", "audio/mod",
	                "1080 string 32CN", NULL);

	for (i = 0; i < G_N_ELEMENTS (config_params); i++) {
		xmms_xform_plugin_config_property_register (xform_plugin,
		                                            config_params[i].key,
		                                            config_params[i].value,
		                                            NULL, NULL);
	}

	return TRUE;
}

static void
xmms_modplug_destroy (xmms_xform_t *xform)
{
	xmms_modplug_data_t *data;
	xmms_config_property_t *cfgv;
	int i;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if (data->buffer)
		g_string_free (data->buffer, TRUE);

	if (data->mod)
		ModPlug_Unload (data->mod);

	for (i = 0; i < G_N_ELEMENTS (config_params); i++) {
		cfgv = xmms_xform_config_lookup (xform, config_params[i].key);

		xmms_config_property_callback_remove (cfgv,
		                                      xmms_modplug_config_changed,
		                                      data);
	}

	g_free (data);

}

static gint64
xmms_modplug_seek (xmms_xform_t *xform, gint64 samples,
                xmms_xform_seek_mode_t whence, xmms_error_t *error)
{
	xmms_modplug_data_t *data;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (samples >= 0, -1);
	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);

	data = xmms_xform_private_data_get (xform);

	ModPlug_Seek (data->mod, (int) ((gdouble)1000 * samples / 44100));

	return samples;
}

static gboolean
xmms_modplug_init (xmms_xform_t *xform)
{
	xmms_modplug_data_t *data;
	const gchar *metakey;
	gint filesize;
	xmms_config_property_t *cfgv;
	gint i;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_modplug_data_t, 1);

	xmms_xform_private_data_set (xform, data);

	for (i = 0; i < G_N_ELEMENTS (config_params); i++) {
		cfgv = xmms_xform_config_lookup (xform, config_params[i].key);
		xmms_config_property_callback_set (cfgv,
		                                   xmms_modplug_config_changed,
		                                   data);

		xmms_modplug_config_changed (XMMS_OBJECT (cfgv), NULL, data);
	}

	/* mFrequency and mResamplingMode are set in config_changed */
	data->settings.mChannels = 2;
	data->settings.mBits = 16;
	ModPlug_SetSettings (&data->settings);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             2,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->settings.mFrequency,
	                             XMMS_STREAM_TYPE_END);

	data->buffer = g_string_new ("");

	for (;;) {
		xmms_error_t error;
		gchar buf[4096];
		gint ret;

		ret = xmms_xform_read (xform, buf, sizeof (buf), &error);
		if (ret == -1) {
			XMMS_DBG ("Error reading mod");
			return FALSE;
		}
		if (ret == 0) {
			break;
		}
		g_string_append_len (data->buffer, buf, ret);
	}

	data->mod = ModPlug_Load (data->buffer->str, data->buffer->len);
	if (!data->mod) {
		XMMS_DBG ("Error loading mod");
		return FALSE;
	}

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	if (xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		xmms_xform_metadata_set_int (xform, metakey,
		                             ModPlug_GetLength (data->mod));
	}

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE;
	xmms_xform_metadata_set_str (xform, metakey, ModPlug_GetName (data->mod));

	return TRUE;
}


static gint
xmms_modplug_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	xmms_modplug_data_t *data;

	data = xmms_xform_private_data_get (xform);

	return ModPlug_Read (data->mod, buf, len);
}

static void
xmms_modplug_config_changed (xmms_object_t *obj, xmmsv_t *_value,
                             gpointer udata)
{
	xmms_modplug_data_t *data = udata;
	xmms_config_property_t *prop = (xmms_config_property_t *) obj;
	const gchar *name;
	const gchar *value;
	gint intvalue;

	name = xmms_config_property_get_name (prop);

	if (!g_ascii_strcasecmp (name, "modplug.resample")) {
		value = xmms_config_property_get_string (prop);

		if (!g_ascii_strcasecmp (value, "fir")) {
			data->settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
		} else if (!g_ascii_strcasecmp (value, "spline")) {
			data->settings.mResamplingMode = MODPLUG_RESAMPLE_SPLINE;
		} else if (!g_ascii_strcasecmp (value, "linear")) {
			data->settings.mResamplingMode = MODPLUG_RESAMPLE_LINEAR;
		} else {
			data->settings.mResamplingMode = MODPLUG_RESAMPLE_NEAREST;
		}
	} else {
		intvalue = xmms_config_property_get_int (prop);

		if (!g_ascii_strcasecmp (name, "modplug.freq")) {
			data->settings.mFrequency = intvalue;
		} else if (!g_ascii_strcasecmp (name, "modplug.reverb_depth")) {
			data->settings.mReverbDepth = intvalue;
		} else if (!g_ascii_strcasecmp (name, "modplug.reverb_delay")) {
			data->settings.mReverbDelay = intvalue;
		} else if (!g_ascii_strcasecmp (name, "modplug.bass_amount")) {
			data->settings.mBassAmount = intvalue;
		} else if (!g_ascii_strcasecmp (name, "modplug.bass_range")) {
			data->settings.mBassRange = intvalue;
		} else if (!g_ascii_strcasecmp (name, "modplug.surround_depth")) {
			data->settings.mSurroundDepth = intvalue;
		} else if (!g_ascii_strcasecmp (name, "modplug.surround_delay")) {
			data->settings.mSurroundDelay = intvalue;
		} else if (!g_ascii_strcasecmp (name, "modplug.loop")) {
			data->settings.mLoopCount = intvalue;
		} else if (!g_ascii_strcasecmp (name, "modplug.enable_oversampling")) {
			if (intvalue) {
				data->settings.mFlags |= MODPLUG_ENABLE_OVERSAMPLING;
			} else {
				data->settings.mFlags &= ~MODPLUG_ENABLE_OVERSAMPLING;
			}
		} else if (!g_ascii_strcasecmp (name,
		                                "modplug.enable_noise_reduction")) {
			if (intvalue) {
				data->settings.mFlags |= MODPLUG_ENABLE_NOISE_REDUCTION;
			} else {
				data->settings.mFlags &= ~MODPLUG_ENABLE_NOISE_REDUCTION;
			}
		} else if (!g_ascii_strcasecmp (name, "modplug.enable_reverb")) {
			if (intvalue) {
				data->settings.mFlags |= MODPLUG_ENABLE_REVERB;
			} else {
				data->settings.mFlags &= ~MODPLUG_ENABLE_REVERB;
			}
		} else if (!g_ascii_strcasecmp (name, "modplug.enable_megabass")) {
			if (intvalue) {
				data->settings.mFlags |= MODPLUG_ENABLE_MEGABASS;
			} else {
				data->settings.mFlags &= ~MODPLUG_ENABLE_MEGABASS;
			}
		} else if (!g_ascii_strcasecmp (name, "modplug.enable_surround")) {
			if (intvalue) {
				data->settings.mFlags |= MODPLUG_ENABLE_SURROUND;
			} else {
				data->settings.mFlags &= ~MODPLUG_ENABLE_SURROUND;
			}
		}
	}

	ModPlug_SetSettings (&data->settings);
}
