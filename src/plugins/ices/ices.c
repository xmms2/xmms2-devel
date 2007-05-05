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
 * ICES2 based effect
 */

#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_medialib.h"

#include <math.h>
#include <glib.h>
#include <stdlib.h>

#include <shout/shout.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "encode.h"

typedef struct xmms_ices_data_St {
	shout_t *shout;
	vorbis_comment vc;
	encoder_state *enc;
	gint serial;
} xmms_ices_data_t;

static gboolean xmms_ices_plugin_setup (xmms_output_plugin_t *output_plugin);
static gboolean xmms_ices_open (xmms_output_t *output);
static gboolean xmms_ices_new (xmms_output_t *output);
static void xmms_ices_destroy (xmms_output_t *output);
static void xmms_ices_close (xmms_output_t *output);
static void xmms_ices_flush (xmms_output_t *output);
static void xmms_ices_write (xmms_output_t *output, gpointer buffer, gint len, xmms_error_t *err);
static gboolean xmms_ices_format_set (xmms_output_t *output, const xmms_stream_type_t *format);
/*
static gboolean xmms_oss_volume_set (xmms_output_t *output, const gchar *channel, guint volume);
static gboolean xmms_oss_volume_get (xmms_output_t *output,
                                     gchar const **names, guint *values,
                                     guint *num_channels);
 */


XMMS_OUTPUT_PLUGIN ("ices",
                    "ICES Output",
                    XMMS_VERSION,
                    "Icecast source output plugin",
                    xmms_ices_plugin_setup);

static gboolean
xmms_ices_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);
	methods.new = xmms_ices_new;
	methods.destroy = xmms_ices_destroy;

	methods.open = xmms_ices_open;
	methods.close = xmms_ices_close;

	methods.flush = xmms_ices_flush;
	methods.format_set_always = xmms_ices_format_set;
	methods.write = xmms_ices_write;

	xmms_output_plugin_methods_set (plugin, &methods);

	xmms_output_plugin_config_property_register (plugin, "encodingnombr", "64000",
	                                             NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "host", "localhost",
	                                             NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "port", "8000", NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "password", "hackme",
	                                             NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "user", "source", NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "mount", "/stream.ogg",
	                                             NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "public", "0", NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "streamname", "", NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "streamdescription", "",
	                                             NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "streamgenre", "",
	                                             NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "streamurl", "", NULL, NULL);

	return TRUE;
}

static void
xmms_ices_flush (xmms_output_t *output)
{
}

static gboolean
xmms_ices_open (xmms_output_t *output)
{
	xmms_ices_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (shout_open (data->shout) == SHOUTERR_SUCCESS) {
		XMMS_DBG ("Connected to %s:%d/%s",
		          shout_get_host (data->shout),
		          shout_get_port (data->shout),
		          shout_get_mount (data->shout));
	} else {
		xmms_log_error ("Couldn't connect to shoutserver");
		return FALSE;
	}

	return TRUE;
}

static void
xmms_ices_close (xmms_output_t *output)
{
	xmms_ices_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	shout_close (data->shout);
}

static void
xmms_ices_write (xmms_output_t *output, gpointer buffer,
                 gint len, xmms_error_t *err)
{
	xmms_ices_data_t *data;
	ogg_page og;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (!data->enc) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "encoding is not initilized");
		return;
	}

	encode_data (data->enc, buffer, len, 0);

	while (encode_dataout (data->enc, &og) > 0) {
		if (shout_send (data->shout, og.header, og.header_len) < 0) {
			xmms_error_set (err, XMMS_ERROR_GENERIC, "Disconnected or something.");
			return;
		} else if (shout_send (data->shout, og.body, og.body_len) < 0) {
			xmms_error_set (err, XMMS_ERROR_GENERIC, "Error when sending data to icecast server");
			return;
		}
		shout_sync (data->shout);
	}

}

static gboolean
xmms_ices_new (xmms_output_t *output)
{
	xmms_ices_data_t *data;
	xmms_config_property_t *val;

	shout_init ();

	data = g_new0 (xmms_ices_data_t, 1);
	data->shout = shout_new ();

	shout_set_format (data->shout, SHOUT_FORMAT_VORBIS);
	shout_set_protocol (data->shout, SHOUT_PROTOCOL_HTTP);

	val = xmms_output_config_lookup (output, "host");
	shout_set_host (data->shout, xmms_config_property_get_string (val));

	val = xmms_output_config_lookup (output, "port");
	shout_set_port (data->shout, xmms_config_property_get_int (val));

	val = xmms_output_config_lookup (output, "password");
	shout_set_password (data->shout, xmms_config_property_get_string (val));

	val = xmms_output_config_lookup (output, "user");
	shout_set_user (data->shout, xmms_config_property_get_string (val));

	shout_set_agent (data->shout, "XMMS/" XMMS_VERSION);

	val = xmms_output_config_lookup (output, "mount");
	shout_set_mount (data->shout, xmms_config_property_get_string (val));

	val = xmms_output_config_lookup (output, "public");
	shout_set_public (data->shout, xmms_config_property_get_int (val));

	val = xmms_output_config_lookup (output, "streamname");
	shout_set_name (data->shout, xmms_config_property_get_string (val));

	val = xmms_output_config_lookup (output, "streamdescription");
	shout_set_description (data->shout, xmms_config_property_get_string (val));

	val = xmms_output_config_lookup (output, "streamgenre");
	shout_set_genre (data->shout, xmms_config_property_get_string (val));

	val = xmms_output_config_lookup (output, "streamurl");
	shout_set_url (data->shout, xmms_config_property_get_string (val));

	data->serial = 1;

	xmms_output_private_data_set (output, data);

	/* static for now */
	xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_S16, 2, 44100);

	return TRUE;
}

static void
xmms_ices_destroy (xmms_output_t *output)
{
	xmms_ices_data_t *data;

	g_return_if_fail (output);

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	shout_close (data->shout);
	shout_free (data->shout);
	shout_shutdown ();
}

static gboolean
xmms_ices_format_set (xmms_output_t *output, const xmms_stream_type_t *format)
{
	xmms_ices_data_t *data;
	xmms_config_property_t *val;
	gint nombr;
	gint rate;
	gint channels;
	xmms_medialib_entry_t entry;
	xmms_medialib_session_t *session;

	static const struct {
		gchar *prop;
		gchar *key;
	} *pptr, props[] = {
		{XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, "title"},
		{XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST, "artist"},
		{XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM, "album"},
		{NULL, NULL}
	};

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	val = xmms_output_config_lookup (output, "encodingnombr");
	nombr = xmms_config_property_get_int (val);
	XMMS_DBG ("Inited a encoder with rate %d nombr %d", rate, nombr);

	entry = xmms_output_current_id (output, NULL);

	vorbis_comment_clear (&data->vc);
	vorbis_comment_init (&data->vc);

	session = xmms_medialib_begin ();

	for (pptr = props; pptr && pptr->prop; pptr++) {
		const gchar *tmp;

		tmp = xmms_medialib_entry_property_get_str (session, entry, pptr->prop);
		if (tmp) {
			vorbis_comment_add_tag (&data->vc,
			                        pptr->key, (gchar *) tmp);
		}
	}

	xmms_medialib_end (session);

	rate = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	channels = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_CHANNELS);

	data->enc = encode_initialise (channels, rate, 1, -1, nombr, -1,
	                               3, data->serial++, &data->vc);

	return TRUE;
}

