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


#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/output.h"
#include "xmms/transport.h"
#include "xmms/xmms.h"

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "cdae.h"


/*
 * Function prototypes
 */

static gboolean xmms_cdae_can_handle (const gchar *mimetype);
static gboolean xmms_cdae_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_cdae_init (xmms_decoder_t *decoder);
static gboolean xmms_cdae_decode_block (xmms_decoder_t *decoder);
static void xmms_cdae_get_media_info (xmms_decoder_t *decoder);
static void xmms_cdae_destroy (xmms_decoder_t *decoder);
static void xmms_cdae_seek (xmms_decoder_t *decoder, guint samples);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, "cddecoder",
			"CDAE decoder " XMMS_VERSION,
			"CDAE decoder");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://www.xiph.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_cdae_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_cdae_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_cdae_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_cdae_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_cdae_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_cdae_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO, xmms_cdae_get_media_info);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);

	xmms_plugin_config_value_register (plugin, "cddbserver", "freedb.freedb.org", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "usecddb", "1", NULL, NULL);

	return plugin;
}

static gboolean
xmms_cdae_can_handle (const gchar *mimetype)
{
	g_return_val_if_fail (mimetype, FALSE);
	
	if ((g_strcasecmp (mimetype, "audio/pcm-data") == 0))
		return TRUE;

	return FALSE;
}

static void
xmms_cdae_get_media_info (xmms_decoder_t *decoder)
{
	xmms_cdae_data_t *data;
	xmms_playlist_entry_t *entry;
	xmms_transport_t *transport;
	xmms_config_value_t *val;
	gint duration;
	gchar *tmp;

	g_return_if_fail (decoder);

	xmms_decoder_samplerate_set (decoder, 44100);

	transport = xmms_decoder_transport_get (decoder);
	
	data = xmms_transport_private_data_get (transport);

	if (data->track == data->toc->last_track)
		duration = LBA (data->toc->leadout) - LBA (data->toc->track[data->track]);
	else
		duration = LBA (data->toc->track[data->track+1]) - LBA (data->toc->track[data->track]);

	duration = (duration * 1000) / 75;

	entry = xmms_playlist_entry_new (NULL);

	tmp = g_strdup_printf ("%d", duration);
	xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION, tmp);
	g_free (tmp);

	tmp = g_strdup_printf ("CDAE Track %d", data->track);
	xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE, tmp);
	g_free (tmp);

	xmms_decoder_entry_mediainfo_set (decoder, entry);
	xmms_object_unref (entry);

	val = xmms_plugin_config_lookup (xmms_decoder_plugin_get (decoder), "usecddb");
	if (xmms_config_value_int_get (val) == 1) {
		val = xmms_plugin_config_lookup (xmms_decoder_plugin_get (decoder), "cddbserver");
		entry = xmms_cdae_cddb_query (data->toc, (gchar *)xmms_config_value_string_get (val), data->track);
	}

	if (entry) {
		xmms_decoder_entry_mediainfo_set (decoder, entry);
		xmms_object_unref (entry);
	}
}

static gboolean
xmms_cdae_new (xmms_decoder_t *decoder, const gchar *mimetype)
{
	g_return_val_if_fail (decoder, FALSE);

	return TRUE;
}

static gboolean
xmms_cdae_init (xmms_decoder_t *decoder)
{
	xmms_decoder_samplerate_set (decoder, 44100);
	return TRUE;
}

static gboolean
xmms_cdae_decode_block (xmms_decoder_t *decoder)
{
	gint ret;
	xmms_transport_t *transport;
	gchar buffer[4096];

	g_return_val_if_fail (decoder, FALSE);
	
	transport = xmms_decoder_transport_get (decoder);

	ret = xmms_transport_read (transport, buffer, 4096);

	if (ret == 0) return FALSE;
	
	xmms_decoder_write (decoder, buffer, ret);

	return TRUE;
}


static void 
xmms_cdae_seek (xmms_decoder_t *decoder, guint samples)
{
	gint bps = 44100 * 2 * 2 * 8;
	gint bytes;

	g_return_if_fail (decoder);

	bytes = (guint)(((gdouble)samples) * bps / 44100) / 8;

	xmms_transport_seek (xmms_decoder_transport_get (decoder), bytes, XMMS_TRANSPORT_SEEK_SET);
}

	
static void
xmms_cdae_destroy (xmms_decoder_t *decoder)
{
}

