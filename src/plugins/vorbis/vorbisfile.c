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
  * @file vorbisfile decoder.
  * @url http://www.xiph.org/ogg/vorbis/doc/vorbisfile/
  */


#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/output.h"
#include "xmms/transport.h"
#include "xmms/xmms.h"

#include <math.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct xmms_vorbis_data_St {
	OggVorbis_File vorbisfile;
	ov_callbacks callbacks;
	gint current;
	gint channels;
	gboolean inited;
	gboolean tell_size;
} xmms_vorbis_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_vorbis_can_handle (const gchar *mimetype);
static gboolean xmms_vorbis_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_vorbis_decode_block (xmms_decoder_t *decoder);
static void xmms_vorbis_get_media_info (xmms_decoder_t *decoder);
static void xmms_vorbis_destroy (xmms_decoder_t *decoder);
static gboolean xmms_vorbis_init (xmms_decoder_t *decoder);
static void xmms_vorbis_seek (xmms_decoder_t *decoder, guint samples);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, "vorbis",
			"Vorbis decoder " XMMS_VERSION,
			"Xiph's OGG/Vorbis decoder");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://www.xiph.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_vorbis_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_vorbis_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_vorbis_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_vorbis_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_vorbis_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_vorbis_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO, xmms_vorbis_get_media_info);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);

	return plugin;
}

static gboolean
xmms_vorbis_can_handle (const gchar *mimetype)
{
	g_return_val_if_fail (mimetype, FALSE);
	
	if ((g_strcasecmp (mimetype, "application/ogg") == 0))
		return TRUE;

	if ((g_strcasecmp (mimetype, "application/x-ogg") == 0))
		return TRUE;

	return FALSE;
}

static size_t 
vorbis_callback_read (void *ptr, size_t size, size_t nmemb, void *datasource)
{
	xmms_vorbis_data_t *data;
	xmms_decoder_t *decoder = datasource;
	xmms_transport_t *transport;
	size_t ret;

	g_return_val_if_fail (decoder, 0);

	transport = xmms_decoder_transport_get (decoder);
	data = xmms_decoder_plugin_data_get (decoder);

	g_return_val_if_fail (data, 0);
	g_return_val_if_fail (transport, 0);

	ret = xmms_transport_read (transport, ptr, size*nmemb);

	return ret;
}


/** @todo
 *  Remove fulhack here. Maybe should transport support tell?
 */
static int
vorbis_callback_seek (void *datasource, ogg_int64_t offset, int whence)
{
	xmms_vorbis_data_t *data;
	xmms_decoder_t *decoder = datasource;
	xmms_transport_t *transport;
	

	g_return_val_if_fail (decoder, 0);

	data = xmms_decoder_plugin_data_get (decoder);
	transport = xmms_decoder_transport_get (decoder);

	if (!xmms_transport_can_seek (transport))
		return -1;

	g_return_val_if_fail (transport, 0);
	g_return_val_if_fail (data, 0);

	if (whence == SEEK_CUR) {
		whence = XMMS_TRANSPORT_SEEK_CUR;
	} else if (whence == SEEK_SET) {
		whence = XMMS_TRANSPORT_SEEK_SET;
	} else if (whence == SEEK_END) {
		whence = XMMS_TRANSPORT_SEEK_END;
	}

	if (whence == XMMS_TRANSPORT_SEEK_CUR &&
		offset == 0)
		return 1;

	if (whence == XMMS_TRANSPORT_SEEK_END &&
		offset == 0) {
		data->tell_size = TRUE;
		return 1;
	}
	
	if (!xmms_transport_seek (transport, (gint) offset, whence))
		return -1;

	return 1;
}

static int
vorbis_callback_close (void *datasource)
{
	return 1;
}

static long
vorbis_callback_tell (void *datasource)
{
	xmms_decoder_t *decoder = datasource;
	xmms_vorbis_data_t *data;

	data = xmms_decoder_plugin_data_get (decoder);
	
	if (data->tell_size) {
		data->tell_size = FALSE;
		return xmms_transport_size (xmms_decoder_transport_get (decoder));
	}
		
	return 0;
}

static gboolean 
xmms_vorbis_new (xmms_decoder_t *decoder, const gchar *mimetype)
{
	xmms_vorbis_data_t *data;

	data = g_new0 (xmms_vorbis_data_t, 1);

	data->callbacks.read_func = vorbis_callback_read;
	data->callbacks.seek_func = vorbis_callback_seek;
	data->callbacks.close_func = vorbis_callback_close;
	data->callbacks.tell_func = vorbis_callback_tell;
	data->current = -1;

	xmms_decoder_plugin_data_set (decoder, data);


	return TRUE;
}

static void
xmms_vorbis_get_media_info (xmms_decoder_t *decoder)
{
	xmms_vorbis_data_t *data;
	xmms_playlist_entry_t *entry;
	vorbis_info *vi;
	double playtime;
	char **ptr;
	gchar tmp[12];

	g_return_if_fail (decoder);

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_if_fail (data);

	xmms_vorbis_init (decoder);

	entry = xmms_playlist_entry_new (NULL);

	XMMS_DBG ("Running get_media_info()");

	vi = ov_info (&data->vorbisfile, -1);

	playtime = ov_time_total (&data->vorbisfile, -1);
	if (playtime != OV_EINVAL) {
		g_snprintf (tmp, 12, "%d", (gint) playtime * 1000);
		xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION, tmp);
	}

	if (vi && vi->bitrate_nominal) {
		g_snprintf (tmp, 12, "%d", (gint) vi->bitrate_nominal/1000);
		xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE, tmp);
	}

	ptr = ov_comment (&data->vorbisfile, -1)->user_comments;

	if (ptr) {
		while (*ptr) {
			gchar **s;
			s = g_strsplit (*ptr, "=", 2);
			if (s && s[0] && s[1]) {
				xmms_playlist_entry_property_set (entry, s[0], s[1]);
			}

			g_strfreev (s);
			
			++ptr;
		}
	}
	
	xmms_decoder_samplerate_set (decoder, vi->rate);

	xmms_decoder_entry_mediainfo_set (decoder, entry);
	xmms_playlist_entry_unref (entry);

}

static gboolean
xmms_vorbis_init (xmms_decoder_t *decoder)
{
	xmms_vorbis_data_t *data;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_val_if_fail (data, FALSE);


	if (!data->inited) {
		vorbis_info *vi;
		gint ret = ov_open_callbacks (decoder, &data->vorbisfile, NULL, 0, data->callbacks);
		if (ret != 0) {
			XMMS_DBG ("Got %d from ov_open_callbacks", ret);
			return FALSE;
		}
		vi = ov_info (&data->vorbisfile, -1);
		data->channels = vi->channels;
		data->inited = TRUE;

		XMMS_DBG ("channels == %d", data->channels);
		XMMS_DBG ("Vorbis inited!!!!");
	}


	return TRUE;
}

static gboolean
xmms_vorbis_decode_block (xmms_decoder_t *decoder)
{
	gint ret = 0;
	gint c;
	xmms_vorbis_data_t *data;
	guint16 buffer[2048];
	guint16 monobuffer[1024];

	g_return_val_if_fail (decoder, FALSE);
	
	data = xmms_decoder_plugin_data_get (decoder);
	g_return_val_if_fail (decoder, FALSE);

	/* this produces 16bit signed PCM littleendian PCM data */
	if (data->channels == 2) {
		ret = ov_read (&data->vorbisfile, (gchar *)buffer, 4096, 0, 2, 1, &c);
	} else if (data->channels == 1) {
		ret = ov_read (&data->vorbisfile, (gchar *)monobuffer, 2048, 0, 2, 1, &c);
	} else {
		XMMS_DBG ("Plugin doesn't handle %d number of channels", data->channels);
		return FALSE;
	}

	if (ret == 0) {
		XMMS_DBG ("got ZERO from ov_read");
		return FALSE;
	} else if (ret < 0) {
		return TRUE;
	}

	if (c != data->current) {
		XMMS_DBG ("current = %d, c = %d", data->current, c);
		xmms_vorbis_get_media_info (decoder);
		data->current = c;
	}

	if (data->channels == 1) {
		gint i;
		gint j = 0;
		for (i = 0; i < (ret/2); i++) {
			buffer[j++] = monobuffer[i];
			buffer[j++] = monobuffer[i];
		}
	}

	xmms_decoder_write (decoder,(gchar *) buffer, ret*2);

	return TRUE;
}


static void 
xmms_vorbis_seek (xmms_decoder_t *decoder, guint samples)
{
	xmms_vorbis_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_plugin_data_get (decoder);

	g_return_if_fail (data);

	ov_pcm_seek (&data->vorbisfile, samples);
}

	
static void
xmms_vorbis_destroy (xmms_decoder_t *decoder)
{
	xmms_vorbis_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_plugin_data_get (decoder);

	g_return_if_fail (data);

	ov_clear (&data->vorbisfile);

	g_free (data);
}

