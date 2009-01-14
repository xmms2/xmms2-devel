/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */


#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_medialib.h"
#include "xmms/xmms_log.h"
#include <modplug.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * Type definitions
 */

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
/*static gboolean xmms_modplug_seek (xmms_xform_t *xform, guint samples);*/

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN ("modplug",
                   "MODPLUG decoder ",
                   XMMS_VERSION,
                   "Module file decoder",
                   xmms_modplug_plugin_setup);

static gboolean
xmms_modplug_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_modplug_init;
	methods.destroy = xmms_modplug_destroy;
	methods.read = xmms_modplug_read;
	/*
	  methods.seek
	*/

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	/*
	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	xmms_plugin_info_add (plugin, "License", "GPL");
	*/

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/mod",
	                              NULL);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/xm",
	                              NULL);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/s3m",
	                              NULL);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/it",
	                              NULL);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/med",
	                              NULL);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/umx",
	                              NULL);

	xmms_magic_add ("Fasttracker II module", "audio/xm",
	                "0 string Extended Module:", NULL);
	xmms_magic_add ("ScreamTracker III module", "audio/s3m",
	                "44 string SCRM", NULL);
	xmms_magic_add ("Impulse Tracker module", "audio/it",
	                "0 string IMPM", NULL);
	xmms_magic_add ("MED module", "audio/med",
	                "0 string MMD", NULL);

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

	return TRUE;
}

static void
xmms_modplug_destroy (xmms_xform_t *xform)
{
	xmms_modplug_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if (data->buffer)
		g_string_free (data->buffer, TRUE);

	if (data->mod)
		ModPlug_Unload (data->mod);

	g_free (data);

}

#if 0
static gboolean
xmms_modplug_seek (xmms_xform_t *xform, guint samples)
{
	xmms_modplug_data_t *data;

	g_return_val_if_fail (xform, FALSE);

	data = xmms_xform_private_data_get (xform);

	ModPlug_Seek (data->mod, (int) ((gdouble)1000 * samples / 44100));

	return TRUE;
}
#endif

static gboolean
xmms_modplug_init (xmms_xform_t *xform)
{
	xmms_modplug_data_t *data;
	const gchar *metakey;
	gint filesize;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_modplug_data_t, 1);

	xmms_xform_private_data_set (xform, data);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             2,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             44100,
	                             XMMS_STREAM_TYPE_END);

	data->settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
	data->settings.mChannels = 2;
	data->settings.mBits = 16;
	data->settings.mFrequency = 44100;
	/* more? */
	ModPlug_SetSettings (&data->settings);

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
