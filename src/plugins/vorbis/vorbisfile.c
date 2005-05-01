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
	xmms_audio_format_t *format;
	gboolean inited;
} xmms_vorbis_data_t;

typedef struct {
	gchar *vname;
	gchar *xname;
} props;

#define MUSICBRAINZ_VA_ID "89ad4ac3-39f7-470e-963a-56509c546377"

/** These are the properties that we extract from the comments */
static props properties[] = {
	{ "title", XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE },
	{ "artist", XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST },
	{ "album", XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM },
	{ "tracknumber", XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR },
	{ "date", XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR },
	{ "genre", XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE },
	{ "musicbrainz_albumid", XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID },
	{ "musicbrainz_artistid", XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID },
	{ "musicbrainz_trackid", XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID },
	{ NULL, 0 }
};

/*
 * Function prototypes
 */

static gboolean xmms_vorbis_can_handle (const gchar *mimetype);
static gboolean xmms_vorbis_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_vorbis_decode_block (xmms_decoder_t *decoder);
static void xmms_vorbis_get_media_info (xmms_decoder_t *decoder);
static void xmms_vorbis_destroy (xmms_decoder_t *decoder);
static gboolean xmms_vorbis_init (xmms_decoder_t *decoder);
static gboolean xmms_vorbis_seek (xmms_decoder_t *decoder, guint samples);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, "vorbis",
			"Vorbis decoder " XMMS_VERSION,
			"Xiph's Ogg/Vorbis decoder");

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
	xmms_error_t error;
	size_t ret;

	g_return_val_if_fail (decoder, 0);

	transport = xmms_decoder_transport_get (decoder);
	data = xmms_decoder_private_data_get (decoder);

	g_return_val_if_fail (data, 0);
	g_return_val_if_fail (transport, 0);

	ret = xmms_transport_read (transport, ptr, size*nmemb, &error);

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

	data = xmms_decoder_private_data_get (decoder);
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

	if (xmms_transport_seek (transport, (gint) offset, whence) == -1)
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
	xmms_transport_t *transport; 

	transport = xmms_decoder_transport_get (decoder);  

	return xmms_transport_tell (transport); 
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

	xmms_decoder_private_data_set (decoder, data);


	return TRUE;
}

static void
get_replaygain (xmms_medialib_entry_t entry,
                vorbis_comment *vc)
{
	const char *tmp = NULL;
	gchar buf[8];

	/* track gain */
	if (!(tmp = vorbis_comment_query (vc, "replaygain_track_gain", 0))) {
		tmp = vorbis_comment_query (vc, "rg_radio", 0);
	}

	if (tmp) {
		g_snprintf (buf, sizeof (buf), "%f",
		            pow (10.0, g_strtod (tmp, NULL) / 20));
		xmms_medialib_entry_property_set (entry,
		                                  XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK,
		                                  buf);
	}

	/* album gain */
	if (!(tmp = vorbis_comment_query (vc, "replaygain_album_gain", 0))) {
		tmp = vorbis_comment_query (vc, "rg_audiophile", 0);
	}

	if (tmp) {
		g_snprintf (buf, sizeof (buf), "%f",
		            pow (10.0, g_strtod (tmp, NULL) / 20));
		xmms_medialib_entry_property_set (entry,
		                                  XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM,
		                                  buf);
	}

	/* track peak */
	if (!(tmp = vorbis_comment_query (vc, "replaygain_track_peak", 0))) {
		tmp = vorbis_comment_query (vc, "rg_peak", 0);
	}

	if (tmp) {
		xmms_medialib_entry_property_set (entry,
		                                  XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK,
		                                  (gchar *) tmp);
	}

	/* album peak */
	if ((tmp = vorbis_comment_query (vc, "replaygain_album_peak", 0))) {
		xmms_medialib_entry_property_set (entry,
		                                  XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM,
		                                  (gchar *) tmp);
	}
}

static void
xmms_vorbis_get_media_info (xmms_decoder_t *decoder)
{
	xmms_vorbis_data_t *data;
	xmms_medialib_entry_t entry;
	vorbis_info *vi;
	double playtime;
	vorbis_comment *ptr;
	gchar tmp[12];

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	xmms_vorbis_init (decoder);

	entry = xmms_decoder_medialib_entry_get (decoder);

	XMMS_DBG ("Running get_media_info()");

	vi = ov_info (&data->vorbisfile, -1);

	playtime = ov_time_total (&data->vorbisfile, -1);
	if (playtime != OV_EINVAL) {
		g_snprintf (tmp, 12, "%d", (gint) playtime * 1000);
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION, tmp);
	}

	if (vi && vi->bitrate_nominal) {
		g_snprintf (tmp, 12, "%d", (gint) vi->bitrate_nominal/1000);
		xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE, tmp);
	}

	ptr = ov_comment (&data->vorbisfile, -1);

	if (ptr) {
		gint temp;

		for (temp = 0; temp < ptr->comments; temp++) {
			gchar **s; 
			gint i = 0;

			s = g_strsplit (ptr->user_comments[temp], "=", 2); 
			while (properties[i].vname) {
				if ((g_strcasecmp (s[0], "MUSICBRAINZ_ALBUMARTISTID") == 0) &&
				    (g_strcasecmp (s[1], MUSICBRAINZ_VA_ID) == 0)) {
					xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION, "1");
				} else if (g_strcasecmp (properties[i].vname, s[0]) == 0) {
					xmms_medialib_entry_property_set (entry, properties[i].xname, s[1]);
				}
				i++;
			}
			g_strfreev (s); 
		}

		get_replaygain (entry, ptr);
	}
		
	xmms_medialib_entry_send_update (entry);

}

static gboolean
xmms_vorbis_init (xmms_decoder_t *decoder)
{
	xmms_vorbis_data_t *data;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	if (!data->inited) {
		vorbis_info *vi;
		gint ret = ov_open_callbacks (decoder, &data->vorbisfile, NULL, 0, data->callbacks);
		if (ret != 0) {
			return FALSE;
		}
		vi = ov_info (&data->vorbisfile, -1);
		data->inited = TRUE;

		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_S16, vi->channels, vi->rate);
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_U16, vi->channels, vi->rate);
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_S8, vi->channels, vi->rate);
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_U8, vi->channels, vi->rate);
		data->format = xmms_decoder_format_finish (decoder);
		if (data->format == NULL) {
			return FALSE;
		}

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

	g_return_val_if_fail (decoder, FALSE);
	
	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	ret = ov_read (&data->vorbisfile, (gchar *)buffer, sizeof (buffer),
		       G_BYTE_ORDER == G_BIG_ENDIAN,
		       xmms_sample_size_get (data->format->format),
		       xmms_sample_signed_get (data->format->format),
		       &c);

	if (ret == 0) {
		return FALSE;
	} else if (ret < 0) {
		return TRUE;
	}

	if (c != data->current) {
		xmms_vorbis_get_media_info (decoder);
		data->current = c;
	}

	xmms_decoder_write (decoder,(gchar *) buffer, ret);

	return TRUE;
}


static gboolean 
xmms_vorbis_seek (xmms_decoder_t *decoder, guint samples)
{
	xmms_vorbis_data_t *data;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);

	g_return_val_if_fail (data, FALSE);

	if (samples > ov_pcm_total (&data->vorbisfile, -1)) {
		xmms_log_error ("Trying to seek past end of stream"); 
		return FALSE; 
	}

	ov_pcm_seek (&data->vorbisfile, samples);
	return TRUE;
}

	
static void
xmms_vorbis_destroy (xmms_decoder_t *decoder)
{
	xmms_vorbis_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);

	g_return_if_fail (data);

	ov_clear (&data->vorbisfile);

	g_free (data);
}

