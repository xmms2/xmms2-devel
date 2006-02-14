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


#include "xmms/xmms_defs.h"
#include "xmms/xmms_decoderplugin.h"
#include "xmms/xmms_log.h"

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
} xmms_vorbis_data_t;

typedef enum { STRING, INTEGER } ptype;
typedef struct {
	gchar *vname;
	gchar *xname;
	ptype type;
} props;

#define MUSICBRAINZ_VA_ID "89ad4ac3-39f7-470e-963a-56509c546377"

/** These are the properties that we extract from the comments */
static props properties[] = {
	{ "title",                XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,     STRING  },
	{ "artist",               XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,    STRING  },
	{ "album",                XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,     STRING  },
	{ "tracknumber",          XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,   INTEGER },
	{ "date",                 XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,      STRING  },
	{ "genre",                XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,     STRING  },
	{ "musicbrainz_albumid",  XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID,  STRING  },
	{ "musicbrainz_artistid", XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID, STRING  },
	{ "musicbrainz_trackid",  XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID,  STRING  },
};

/*
 * Function prototypes
 */

static gboolean xmms_vorbis_new (xmms_decoder_t *decoder);
static gboolean xmms_vorbis_decode_block (xmms_decoder_t *decoder);
static void xmms_vorbis_get_media_info (xmms_decoder_t *decoder);
static void xmms_vorbis_destroy (xmms_decoder_t *decoder);
static gboolean xmms_vorbis_init (xmms_decoder_t *decoder, gint mode);
static gboolean xmms_vorbis_seek (xmms_decoder_t *decoder, guint samples);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER,
				  XMMS_DECODER_PLUGIN_API_VERSION,
				  "vorbis",
				  "Vorbis decoder " XMMS_VERSION,
				  "Xiph's Ogg/Vorbis decoder");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://www.xiph.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW,
	                        xmms_vorbis_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK,
	                        xmms_vorbis_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY,
	                        xmms_vorbis_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT,
	                        xmms_vorbis_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK,
	                        xmms_vorbis_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO,
	                        xmms_vorbis_get_media_info);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);

	xmms_plugin_magic_add (plugin, "ogg/vorbis header",
	                       "application/ogg",
	                       "0 string OggS", ">4 byte 0",
	                       ">>28 string \x01vorbis", NULL);

	return plugin;
}

static size_t
vorbis_callback_read (void *ptr, size_t size, size_t nmemb,
                      void *datasource)
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

	ret = xmms_transport_read (transport, ptr, size * nmemb, &error);

	return ret / size;
}

static int
vorbis_callback_seek (void *datasource, ogg_int64_t offset, int whence)
{
	xmms_vorbis_data_t *data;
	xmms_decoder_t *decoder = datasource;
	xmms_transport_t *transport;
	gint ret;

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

	ret = xmms_transport_seek (transport, (gint) offset, whence);

	return (ret == -1) ? -1 : 0;
}

static int
vorbis_callback_close (void *datasource)
{
	return 0;
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
xmms_vorbis_new (xmms_decoder_t *decoder)
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
get_replaygain (xmms_medialib_session_t *session,
				xmms_medialib_entry_t entry,
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
		/** @todo this should probably be a int instead? */
		xmms_medialib_entry_property_set_str (session, entry,
			XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK, buf);
	}

	/* album gain */
	if (!(tmp = vorbis_comment_query (vc, "replaygain_album_gain", 0))) {
		tmp = vorbis_comment_query (vc, "rg_audiophile", 0);
	}

	if (tmp) {
		g_snprintf (buf, sizeof (buf), "%f",
		            pow (10.0, g_strtod (tmp, NULL) / 20));
		/** @todo this should probably be a int instead? */
		xmms_medialib_entry_property_set_str (session, entry,
			XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM,
			buf);
	}

	/* track peak */
	if (!(tmp = vorbis_comment_query (vc, "replaygain_track_peak", 0))) {
		tmp = vorbis_comment_query (vc, "rg_peak", 0);
	}

	if (tmp) {
		xmms_medialib_entry_property_set_str (session, entry,
			XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK,
			(gchar *) tmp);
	}

	/* album peak */
	if ((tmp = vorbis_comment_query (vc, "replaygain_album_peak", 0))) {
		xmms_medialib_entry_property_set_str (session, entry,
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
	xmms_medialib_session_t *session;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	entry = xmms_decoder_medialib_entry_get (decoder);

	XMMS_DBG ("Running get_media_info()");

	vi = ov_info (&data->vorbisfile, -1);

	session = xmms_medialib_begin_write ();

	playtime = ov_time_total (&data->vorbisfile, -1);
	if (playtime != OV_EINVAL) {
		xmms_medialib_entry_property_set_int (session, entry,
			XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
			playtime * 1000);
	}

	if (vi && vi->bitrate_nominal) {
		xmms_medialib_entry_property_set_int (session, entry,
			XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
			(gint) vi->bitrate_nominal);
	}

	if (vi && vi->rate) {
		xmms_medialib_entry_property_set_int (session, entry,
			XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE,
			(gint) vi->rate);
	}

	ptr = ov_comment (&data->vorbisfile, -1);

	if (ptr) {
		gint temp;

		for (temp = 0; temp < ptr->comments; temp++) {
			gchar **s;
			gint i = 0;

			s = g_strsplit (ptr->user_comments[temp], "=", 2);
			for (i = 0; i < G_N_ELEMENTS (properties); i++) {
				if ((g_strcasecmp (s[0], "MUSICBRAINZ_ALBUMARTISTID") == 0) &&
				    (g_strcasecmp (s[1], MUSICBRAINZ_VA_ID) == 0)) {
					xmms_medialib_entry_property_set_int (session, entry,
					                                      XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION, 1);
				} else if (g_strcasecmp (s[0], properties[i].vname) == 0) {
					if (properties[i].type == INTEGER) {
						gint tmp = strtol (s[1], NULL, 10);
						xmms_medialib_entry_property_set_int (session, entry,
						                                      properties[i].xname, tmp);
					} else {
						xmms_medialib_entry_property_set_str (session, entry,
						                                      properties[i].xname, s[1]);
					}
				}
			}

			g_strfreev (s);
		}

		get_replaygain (session, entry, ptr);
	}

	xmms_medialib_end (session);
	xmms_medialib_entry_send_update (entry);
}

static gboolean
xmms_vorbis_init (xmms_decoder_t *decoder, gint mode)
{
	xmms_vorbis_data_t *data;
	gint ret;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	ret = ov_open_callbacks (decoder, &data->vorbisfile, NULL, 0,
	                         data->callbacks);
	if (ret) {
		return FALSE;
	}

	if (mode & XMMS_DECODER_INIT_DECODING) {
		vorbis_info *vi;

		vi = ov_info (&data->vorbisfile, -1);

		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_S16,
		                         vi->channels, vi->rate);
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_U16,
		                         vi->channels, vi->rate);
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_S8,
		                         vi->channels, vi->rate);
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_U8,
		                         vi->channels, vi->rate);

		data->format = xmms_decoder_format_finish (decoder);
		if (!data->format) {
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

	ret = ov_read (&data->vorbisfile, (gchar *) buffer, sizeof (buffer),
	               G_BYTE_ORDER == G_BIG_ENDIAN,
	               xmms_sample_size_get (data->format->format),
	               xmms_sample_signed_get (data->format->format),
				   &c);

	if (!ret) {
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
