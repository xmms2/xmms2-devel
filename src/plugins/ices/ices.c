/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2007-2013 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 * This program is distributed under the terms of the GNU General
 * Public License, version 2. You may use, modify, and redistribute
 * it under the terms of this license. A copy should be included
 * with this source.
 *
 */

/**
 * @file Output plugin to stream Ogg Vorbis to an Icecast2 server.
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
	encoder_state *encoder;
	gint rate;
	gint channels;
} xmms_ices_data_t;

/*
 * Forward definitions.
 */
static gboolean xmms_ices_plugin_setup (xmms_output_plugin_t *plugin);
static gboolean xmms_ices_new (xmms_output_t *output);
static void xmms_ices_destroy (xmms_output_t *output);
static gboolean xmms_ices_open (xmms_output_t *output);
static void xmms_ices_close (xmms_output_t *output);
static void xmms_ices_flush (xmms_output_t *output);
static gboolean xmms_ices_format_set (xmms_output_t *output,
                                      const xmms_stream_type_t *format);
static void xmms_ices_write (xmms_output_t *output, gpointer buffer,
                             gint len, xmms_error_t *err);
static void xmms_ices_update_comment (xmms_medialib_entry_t entry,
                                      vorbis_comment *vc);
static void on_playlist_entry_changed (xmms_object_t *object, xmmsv_t *arg,
                                       gpointer udata);

/*
 * Internal helper functions.
 */
static void
xmms_ices_send_shout (xmms_ices_data_t *data, xmms_error_t *err)
{
	ogg_page og;

	while (xmms_ices_encoder_output (data->encoder, &og) == TRUE) {
		if (shout_send (data->shout, og.header, og.header_len) < 0) {
			if (err)
				xmms_error_set (err, XMMS_ERROR_GENERIC,
				                "Disconnected or something.");
			return;
		} else if (shout_send (data->shout, og.body, og.body_len) < 0) {
			if (err)
				xmms_error_set (err, XMMS_ERROR_GENERIC,
				                "Error when sending data to icecast server");
			return;
		}

		shout_sync (data->shout);
	}
}

static void
xmms_ices_flush_internal (xmms_ices_data_t *data)
{
	xmms_ices_encoder_finish (data->encoder);

	xmms_ices_send_shout (data, NULL);
}

/*
 * Main plugin definition and handler functions.
 */
XMMS_OUTPUT_PLUGIN ("ices",
                    "ICES Output",
                    XMMS_VERSION,
                    "Icecast source output plugin",
                    xmms_ices_plugin_setup);


/* Output plugin definition and setup. */
static gboolean
xmms_ices_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;
	static const struct {
		const char *name;
		const char *val;
	} *pptr, ices_properties[] = {
		{ "encodingnombr", "96000" },
		{ "encodingminbr", "-1" },
		{ "encodingmaxbr", "-1" },
		{ "host", "localhost" },
		{ "port", "8000" },
		{ "user", "source" },
		{ "password", "hackme" },
		{ "mount", "/stream.ogg" },
		{ "public", "0" },
		{ "streamname", "" },
		{ "streamdescription", "" },
		{ "streamgenre", "" },
		{ "streamurl", "" },
		{ NULL, NULL },
	};

	XMMS_OUTPUT_METHODS_INIT (methods);
	methods.new = xmms_ices_new;
	methods.destroy = xmms_ices_destroy;

	methods.open = xmms_ices_open;
	methods.close = xmms_ices_close;

	methods.flush = xmms_ices_flush;
	methods.format_set = xmms_ices_format_set;
	methods.write = xmms_ices_write;

	xmms_output_plugin_methods_set (plugin, &methods);

	for (pptr = ices_properties; pptr->name != NULL; pptr++)
		xmms_output_plugin_config_property_register (plugin, pptr->name,
		                                             pptr->val,
		                                             NULL, NULL);

	return TRUE;
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

	xmms_output_private_data_set (output, data);
	xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_FLOAT, 2, 44100);

	xmms_object_connect (XMMS_OBJECT (output),
	                     XMMS_IPC_SIGNAL_PLAYBACK_CURRENTID,
	                     on_playlist_entry_changed,
	                     data);

	return TRUE;
}

static void
xmms_ices_destroy (xmms_output_t *output)
{
	xmms_ices_data_t *data;
	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	xmms_object_disconnect (XMMS_OBJECT (output),
	                        XMMS_IPC_SIGNAL_PLAYBACK_CURRENTID,
	                        on_playlist_entry_changed,
	                        data);

	if (data->encoder)
		xmms_ices_encoder_fini (data->encoder);

	vorbis_comment_clear (&data->vc);

	shout_close (data->shout);
	shout_free (data->shout);

	g_free (data);

	shout_shutdown ();
}

static gboolean
xmms_ices_open (xmms_output_t *output)
{
	xmms_ices_data_t *data;
	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (shout_open (data->shout) == SHOUTERR_SUCCESS) {
		XMMS_DBG ("Connected to http://%s:%d/%s",
		          shout_get_host (data->shout),
		          shout_get_port (data->shout),
		          shout_get_mount (data->shout));
	} else {
		xmms_log_error ("Couldn't connect to icecast server!");
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

	if (!data->encoder) {
		shout_close (data->shout);
		return;
	}

	xmms_ices_flush_internal (data);

	shout_close (data->shout);

	xmms_ices_encoder_fini (data->encoder);
	data->encoder = NULL;
}

static void
xmms_ices_flush (xmms_output_t *output)
{
}

static gboolean
xmms_ices_format_set (xmms_output_t *output, const xmms_stream_type_t *format)
{
	xmms_ices_data_t *data;
	xmms_config_property_t *val;
	xmms_medialib_entry_t entry;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (data->encoder)
		xmms_ices_flush_internal (data);

	/* Set the Vorbis comment to the current track metadata. */
	vorbis_comment_clear (&data->vc);
	vorbis_comment_init (&data->vc);

	entry = xmms_output_current_id (output);
	xmms_ices_update_comment (entry, &data->vc);

	/* If there is no encoder around, we need to build one. */
	if (!data->encoder) {
		int minbr, nombr, maxbr;

		val = xmms_output_config_lookup (output, "encodingnombr");
		nombr = xmms_config_property_get_int (val);
		val = xmms_output_config_lookup (output, "encodingminbr");
		minbr = xmms_config_property_get_int (val);
		val = xmms_output_config_lookup (output, "encodingmaxbr");
		maxbr = xmms_config_property_get_int (val);

		data->encoder = xmms_ices_encoder_init (minbr, nombr, maxbr);
		if (!data->encoder)
			return FALSE;
	}

	/* Get this stream's data and fire up the encoder. */
	data->rate = xmms_stream_type_get_int (format,
	                                       XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	data->channels = xmms_stream_type_get_int (format,
	                                           XMMS_STREAM_TYPE_FMT_CHANNELS);

	XMMS_DBG ("Setting format to rate: %i, channels: %i",
	          data->rate,
	          data->channels);

	xmms_ices_encoder_stream_change (data->encoder, data->rate, data->channels,
	                                 &data->vc);

	return TRUE;
}

static void
xmms_ices_write (xmms_output_t *output, gpointer buffer,
                 gint len, xmms_error_t *err)
{
	xmms_ices_data_t *data;
	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (!data->encoder) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "encoding is not initialized");
		return;
	}

	xmms_ices_encoder_input (data->encoder, buffer, len);

	xmms_ices_send_shout (data, err);
}

static void
xmms_ices_update_comment (xmms_medialib_entry_t entry, vorbis_comment *vc)
{
	static const struct {
		const gchar *prop;
		const gchar *key;
	} *pptr, props[] = {
		{XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, "title"},
		{XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST, "artist"},
		{XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM, "album"},
		{NULL, NULL}
	};

	vorbis_comment_clear (vc);
	vorbis_comment_init (vc);

	/* TODO: #2333
	for (pptr = props; pptr && pptr->prop; pptr++) {
		const gchar *tmp;

		tmp = xmms_medialib_entry_property_get_str (entry, pptr->prop);
		if (tmp) {
			vorbis_comment_add_tag (vc, pptr->key, (gchar *) tmp);
		}
	}
	*/

}

static void
on_playlist_entry_changed (xmms_object_t *object, xmmsv_t *arg, gpointer udata)
{
	xmms_ices_data_t * data = udata;
	xmms_medialib_entry_t entry;

	if (!xmmsv_get_int (arg, &entry))
		return;

	if (data->encoder)
		xmms_ices_flush_internal (data);

	vorbis_comment_clear (&data->vc);
	vorbis_comment_init (&data->vc);

	xmms_ices_update_comment (entry, &data->vc);

	XMMS_DBG ("Updating comment");

	xmms_ices_encoder_stream_change (data->encoder, data->rate, data->channels,
	                                 &data->vc);

}
