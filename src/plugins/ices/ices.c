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
 * ICES2 based effect
 */

#include "xmms/xmms.h"
#include "xmms/plugin.h"
#include "xmms/effect.h"
#include "xmms/util.h"
#include "xmms/config.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/playlist.h"
#include "xmms/playlist_entry.h"
#include "xmms/signal_xmms.h"
#include "internal/output_int.h"

#include <math.h>
#include <glib.h>
#include <stdlib.h>

#include <shout/shout.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "encode.h"

typedef struct xmms_ices_data_St {
	shout_t *shout;
	xmms_playlist_entry_t *entry;
	vorbis_comment vc;
	encoder_state *enc;
	gint serial;

	xmms_ringbuf_t *buf;
	gint buf_size;
	gboolean write_buf;
	GThread *thread;
} xmms_ices_data_t;

static void xmms_ices_new (xmms_effect_t *effect, xmms_output_t *output);
static void xmms_ices_destroy (xmms_effect_t *effect);
static void xmms_ices_samplerate_set (xmms_effect_t *effect, guint rate);
static void xmms_ices_process (xmms_effect_t *effect, gchar *buf, guint len);
static void on_playlist_entry_changed (xmms_output_t *output,
                                       const xmms_object_cmd_arg_t *arg,
                                       xmms_ices_data_t *data);

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_EFFECT, "ices",
	                          "Icecast2 Shoutplugin " XMMS_VERSION,
	                          "Icecast2 Shoutplugin");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "INFO", "http://www.icecast.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	xmms_plugin_info_add (plugin, "License", "GPL");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_ices_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_ices_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET, xmms_ices_samplerate_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_PROCESS, xmms_ices_process);

	xmms_plugin_config_value_register (plugin, "enabled", "0", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "encodingnombr", "64000", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "host", "localhost", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "port", "8000", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "password", "hackme", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "user", "source", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "mount", "/stream.ogg", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "public", "0", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "streamname", "", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "streamdescription", "", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "streamgenre", "", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "streamurl", "", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "buffersize", "65536", NULL, NULL);

	return plugin;
}

static gboolean
xmms_ices_reconnect (xmms_ices_data_t *data)
{
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

static gpointer
xmms_ices_thread (xmms_effect_t *effect)
{
	xmms_ices_data_t *data;
	GMutex *mutex;

	g_return_val_if_fail (effect, NULL);
	data = xmms_effect_private_data_get (effect);
	g_return_val_if_fail (data, NULL);

	mutex = g_mutex_new ();

	data->write_buf = TRUE;

	xmms_ringbuf_wait_used (data->buf, data->buf_size - 4096, mutex);

	XMMS_DBG ("Buffering done, starting to encode");

	if (!xmms_ices_reconnect (data)) {
		data->write_buf = FALSE;
		return NULL;
	}

	while (42) {
		gchar buf[4096];
		ogg_page og;
		gint ret;

		ret = xmms_ringbuf_read (data->buf, buf, sizeof (buf));
		if (!ret) {
			xmms_ringbuf_wait_used (data->buf,
			                        data->buf_size - sizeof (buf),
			                        mutex);
			continue;
		}

		if (!data->enc) {
			data->write_buf = FALSE;
			data->thread = NULL;
			return NULL;
		}

		encode_data (data->enc, buf, ret, 0);

		while (encode_dataout (data->enc, &og) > 0) {
			if (shout_send_raw (data->shout, og.header, og.header_len) < 0) {
				if (!xmms_ices_reconnect (data)) {
					data->write_buf = FALSE;
					return NULL;
				}
			} else if (shout_send_raw (data->shout, og.body, og.body_len) < 0) {
				if (!xmms_ices_reconnect (data)) {
					data->write_buf = FALSE;
					return NULL;
				}
			}
		}

	}

	return NULL;
}

static void
xmms_ices_new (xmms_effect_t *effect, xmms_output_t *output)
{
	xmms_ices_data_t *data;
	xmms_plugin_t *plugin = xmms_effect_plugin_get (effect);
	xmms_config_value_t *val;
	xmms_error_t error;

	shout_init ();

	data = g_new0 (xmms_ices_data_t, 1);
	data->shout = shout_new ();

	shout_set_format (data->shout, SHOUT_FORMAT_VORBIS);
	shout_set_protocol (data->shout, SHOUT_PROTOCOL_HTTP);

	val = xmms_plugin_config_lookup (plugin, "host");
	shout_set_host (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_lookup (plugin, "port");
	shout_set_port (data->shout, xmms_config_value_int_get (val));

	val = xmms_plugin_config_lookup (plugin, "password");
	shout_set_password (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_lookup (plugin, "user");
	shout_set_user (data->shout, xmms_config_value_string_get (val));

	shout_set_agent (data->shout, "XMMS/" XMMS_VERSION);

	val = xmms_plugin_config_lookup (plugin, "mount");
	shout_set_mount (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_lookup (plugin, "public");
	shout_set_public (data->shout, xmms_config_value_int_get (val));

	val = xmms_plugin_config_lookup (plugin, "streamname");
	shout_set_name (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_lookup (plugin, "streamdescription");
	shout_set_description (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_lookup (plugin, "streamgenre");
	shout_set_genre (data->shout, xmms_config_value_string_get (val));

	val = xmms_plugin_config_lookup (plugin, "streamurl");
	shout_set_url (data->shout, xmms_config_value_string_get (val));

	data->write_buf = FALSE;
	val = xmms_plugin_config_lookup (plugin, "buffersize");
	data->buf_size = xmms_config_value_int_get (val);
	data->buf = xmms_ringbuf_new (data->buf_size);

	data->serial = 1;

	xmms_error_reset (&error);
	data->entry = xmms_output_playing_entry_get (output, &error);

	xmms_object_connect (XMMS_OBJECT (output),
	                     XMMS_IPC_SIGNAL_OUTPUT_CURRENTID,
	                     (xmms_object_handler_t ) on_playlist_entry_changed,
	                     data);

	xmms_effect_private_data_set (effect, data);

	data->thread = g_thread_create ((GThreadFunc) xmms_ices_thread,
	                                (gpointer) effect, TRUE, NULL);
}

static void
xmms_ices_destroy (xmms_effect_t *effect)
{
	xmms_ices_data_t *data;

	g_return_if_fail (effect);

	data = xmms_effect_private_data_get (effect);
	g_return_if_fail (data);

	if (data->entry) {
		xmms_object_unref (data->entry);
	}

	shout_close (data->shout);
	shout_free (data->shout);
	shout_shutdown ();
}

static void
xmms_ices_samplerate_set (xmms_effect_t *effect, guint rate)
{
	xmms_ices_data_t *data;
	xmms_config_value_t *val;
	gint nombr;
	static struct {
		gchar *prop;
		gchar *key;
	} *pptr, props[] = {
		{XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE, "title"},
		{XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST, "artist"},
		{XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM, "album"},
		{NULL, NULL}
	};

	g_return_if_fail (effect);

	data = xmms_effect_private_data_get (effect);
	g_return_if_fail (data);

	val = xmms_plugin_config_lookup (xmms_effect_plugin_get (effect), "encodingnombr");
	nombr = xmms_config_value_int_get (val);
	XMMS_DBG ("Inited a encoder with rate %d nombr %d", rate, nombr);

	vorbis_comment_clear (&data->vc);
	vorbis_comment_init (&data->vc);

	for (pptr = props; pptr && pptr->prop; pptr++) {
		const gchar *tmp;

		tmp = xmms_playlist_entry_property_get (data->entry,
		                                        pptr->prop);
		if (tmp) {
			vorbis_comment_add_tag (&data->vc,
			                        pptr->key, (gchar *) tmp);
		}
	}

	data->enc = encode_initialise (2, rate, 1, -1, nombr, -1,
	                               3, data->serial++, &data->vc);
}

static void
xmms_ices_process (xmms_effect_t *effect, gchar *buf, guint len)
{
	xmms_ices_data_t *data;

	g_return_if_fail (effect);

	data = xmms_effect_private_data_get (effect);
	g_return_if_fail (data);

	if (data->write_buf) {
		xmms_ringbuf_write (data->buf, buf, len);
	}
}

static void
on_playlist_entry_changed (xmms_output_t *output,
                           const xmms_object_cmd_arg_t *arg,
                           xmms_ices_data_t *data)
{
	xmms_error_t error;

	if (data->entry) {
		xmms_object_unref (data->entry);
	}

	xmms_error_reset (&error);
	data->entry = xmms_output_playing_entry_get (output, &error);
}
