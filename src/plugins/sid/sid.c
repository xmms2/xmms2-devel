/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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


#include <xmms/xmms_log.h>
#include <xmms/xmms_xformplugin.h>


#include "sidplay_wrapper.h"

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


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
static void xmms_sid_get_songlength (xmms_xform_t *xform);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN_DEFINE ("sid",
                          "SID Decoder",
                          XMMS_VERSION,
                          "libsidplay2 based SID decoder",
                          xmms_sid_plugin_setup);

static gboolean
xmms_sid_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	/*
	xmms_plugin_info_add (plugin, "URL", "http://xmms2.org/");
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
	                              XMMS_STREAM_TYPE_END);

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


	xmms_xform_plugin_config_property_register (xform_plugin, "songlength_path",
	                                            "", NULL, NULL);

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
	const gchar *md5sum;
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

	md5sum = sidplay_wrapper_md5 (data->wrapper);
	xmms_xform_metadata_set_str (xform, "HVSCfingerprint", md5sum);

	xmms_sid_get_songlength (xform);

}

static void
xmms_sid_get_songlength (xmms_xform_t *xform)
{
	xmms_config_property_t *config;
	const gchar *tmp, *md5sum, *songlength_path;
	gint subtune = 1;
	GIOChannel* io;
	GString *buf;

	g_return_if_fail (xform);

	config = xmms_xform_config_lookup (xform, "songlength_path");
	g_return_if_fail (config);
	songlength_path = xmms_config_property_get_string (config);
	if (!songlength_path[0])
		return;

	if (xmms_xform_metadata_get_str (xform, "subtune", &tmp)) {
		subtune = atoi (tmp);
	}

	if (!xmms_xform_metadata_get_str (xform, "HVSCfingerprint", &md5sum)) {
		return;
	}

	io = g_io_channel_new_file (songlength_path, "r", NULL);
	if (!io) {
		xmms_log_error ("Unable to load songlengths database '%s'", songlength_path);
		return;
	}

	buf = g_string_new ("");
	while (g_io_channel_read_line_string (io, buf, NULL, NULL) == G_IO_STATUS_NORMAL) {
		if (buf->len > 33 && g_ascii_strncasecmp (buf->str, md5sum, 32) == 0) {
			gint cur = 0;
			gchar *b;

			b = buf->str + 33;
			while (*b) {
				gint min, sec;

				/* read timestamp */
				if (sscanf (b, "%d:%d", &min, &sec) != 2) {
					/* no more timestamps on this line */
					break;
				} else {
					cur++;
				}

				if (cur == subtune) {
					const gchar *metakey;
					gchar ms_str[10 + 1]; /* LONG_MAX in str, \w NULL */
					glong ms;

					ms = (min * 60 + sec) * 1000;

					metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
					xmms_xform_metadata_set_int (xform, metakey, ms);

					if (g_snprintf (ms_str, 10 + 1, "%ld", ms) > 0) {
						metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_STARTMS;
						xmms_xform_metadata_set_str (xform, metakey, "0");

						metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_STOPMS;
						xmms_xform_metadata_set_str (xform, metakey, ms_str);
					}

					goto done;
				}

				/* forward to next possible timestamp */
				b = strchr (b, ' ');
				if (!b) {
					/* no more timestamps on this line */
					break;
				}
				b++;
			}
		}
	}
	xmms_log_info ("Couldn't find sid tune in songlength.txt");
done:
	g_io_channel_unref (io);
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
