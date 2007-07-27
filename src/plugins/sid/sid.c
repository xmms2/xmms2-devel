/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
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


#include "xmms/xmms_log.h"
#include "xmms/xmms_xformplugin.h"


#include "sidplay_wrapper.h"

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


/*
 * Type definitions
 */

typedef struct xmms_sid_data_St {
	struct sidplay_wrapper *wrapper;
	GString *buffer;
} xmms_sid_data_t;

/*
 * Function prototypes
 */
static gboolean xmms_sid_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_sid_init (xmms_xform_t *xform);
static void xmms_sid_destroy (xmms_xform_t *xform);
static gint xmms_sid_read (xmms_xform_t *xform, void *out, gint outlen, xmms_error_t *error);
static void xmms_sid_get_media_info (xmms_xform_t *xform);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN ("sid",
                   "SID Decoder",
                   XMMS_VERSION,
                   "libsidplay2 based SID decoder",
                   xmms_sid_plugin_setup);

static gboolean
xmms_sid_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	/*
	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://sidplay2.sourceforge.net/");
	xmms_plugin_info_add (plugin, "URL", "http://www.geocities.com/SiliconValley/Lakes/5147/resid/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	*/

	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_sid_init;
	methods.destroy = xmms_sid_destroy;
	methods.read = xmms_sid_read;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/prs.sid",
	                              NULL);

	/* Can only fastforward SIDs, not rewind */
/*
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SUBTUNES);
*/

	xmms_magic_add ("sidplay infofile", "audio/prs.sid",
	                "0 string SIDPLAY INFOFILE", NULL);
	xmms_magic_add ("psid header", "audio/prs.sid",
	                "0 string PSID", NULL);
	xmms_magic_add ("rsid header", "audio/prs.sid",
	                "0 string RSID", NULL);

	return TRUE;
}

static gboolean
xmms_sid_init (xmms_xform_t *xform)
{
	xmms_sid_data_t *data;
	const char *subtune;
	gint ret;

	data = g_new0 (xmms_sid_data_t, 1);

	data->wrapper = sidplay_wrapper_init ();

	xmms_xform_private_data_set (xform, data);

	data->buffer = g_string_new ("");

	for (;;) {
		xmms_error_t error;
		gchar buf[4096];
		gint ret;

		ret = xmms_xform_read (xform, buf, sizeof (buf), &error);
		if (ret == -1) {
			XMMS_DBG ("Error reading sid file");
			return FALSE;
		}
		if (ret == 0) {
			break;
		}
		g_string_append_len (data->buffer, buf, ret);
	}

	ret = sidplay_wrapper_load (data->wrapper, data->buffer->str,
	                            data->buffer->len);

	if (ret < 0) {
		XMMS_DBG ("Couldn't load sid file");
		return FALSE;
	}

	if (xmms_xform_metadata_get_str (xform, "subtune", &subtune)) {
		int num;
		num = atoi (subtune);
		if (num < 1 || num > sidplay_wrapper_subtunes (data->wrapper)) {
			XMMS_DBG ("Invalid subtune index");
			return FALSE;
		}
		sidplay_wrapper_set_subtune (data->wrapper, num);
	}

	xmms_sid_get_media_info (xform);

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

	return TRUE;
}

static void
xmms_sid_destroy (xmms_xform_t *xform)
{
	xmms_sid_data_t *data;

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	sidplay_wrapper_destroy (data->wrapper);

	if (data->buffer)
		g_string_free (data->buffer, TRUE);

	g_free (data);
}

/**
 * @todo  Enable option to use STIL for media info and
 * timedatabase for song length.
 */
static void
xmms_sid_get_media_info (xmms_xform_t *xform)
{
	xmms_sid_data_t *data;
	const gchar *metakey;
	char artist[32];
	char title[32];
	gint err;

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	err = sidplay_wrapper_songinfo (data->wrapper, artist, title);
	if (err > 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE;
		xmms_xform_metadata_set_str (xform, metakey, title);

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST;
		xmms_xform_metadata_set_str (xform, metakey, artist);
	}

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SUBTUNES;
	xmms_xform_metadata_set_int (xform, metakey,
	                             sidplay_wrapper_subtunes (data->wrapper));

	xmms_xform_metadata_set_str (xform,
	                             "HVSCfingerprint",
	                             sidplay_wrapper_md5 (data->wrapper));

}

static gint
xmms_sid_read (xmms_xform_t *xform, void *out, gint outlen, xmms_error_t *error)
{
	xmms_sid_data_t *data;
	gint ret;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = sidplay_wrapper_play (data->wrapper, out, outlen);

	if (!ret) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                sidplay_wrapper_error (data->wrapper));
		return -1;
	}

	return ret;
}
