/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2005 Daniel Svensson, <daniel@nittionio.nu> 
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

#include "xmms/xmms_defs.h"
#include "xmms/xmms_decoderplugin.h"

#include <string.h>
#include <math.h>

#include <glib.h>

#include <mpcdec/mpcdec.h>
#include <mpcdec/reader.h>

#include "ape.h"
#include "musepack.h"

/*
 * Function prototypes
 */

static gboolean xmms_mpc_new (xmms_decoder_t *decoder);
static gboolean xmms_mpc_init (xmms_decoder_t *decoder, gint mode);
static void xmms_mpc_get_mediainfo (xmms_decoder_t *decoder);
static void xmms_mpc_cache_streaminfo (xmms_decoder_t *decoder);
static gboolean xmms_mpc_seek (xmms_decoder_t *decoder, guint samples);
static gboolean xmms_mpc_decode_block (xmms_decoder_t *decoder);
static void xmms_mpc_destroy (xmms_decoder_t *decoder);


/*
 * Plugin header
 */
xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER,
	                          XMMS_DECODER_PLUGIN_API_VERSION,
	                          "mpc",
	                          "Musepack decoder " XMMS_VERSION,
	                          "Musepack Living Audio Compression");

	g_return_val_if_fail (plugin, NULL);

	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org");
	xmms_plugin_info_add (plugin, "URL", "http://www.musepack.net/");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW,
	                        xmms_mpc_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT,
	                        xmms_mpc_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO,
	                        xmms_mpc_get_mediainfo);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK,
	                        xmms_mpc_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK,
	                        xmms_mpc_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY,
	                        xmms_mpc_destroy);

	xmms_plugin_magic_add (plugin,
	                       "mpc header",
	                       "audio/x-mpc",
	                       "0 string MP+", NULL);

	return plugin;
}


static mpc_int32_t
xmms_mpc_callback_read (void *data, void *buffer, mpc_int32_t size)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *) data;
	xmms_transport_t *transport;
	xmms_error_t error;

	g_return_val_if_fail (decoder, -1);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, -1);

	return xmms_transport_read (transport, (gchar *) buffer, size, &error);
}


static mpc_bool_t
xmms_mpc_callback_seek (void *data, mpc_int32_t offset)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *) data;
	xmms_transport_t *transport;
	gint ret;

	g_return_val_if_fail (decoder, FALSE);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	ret = xmms_transport_seek (transport, offset, XMMS_TRANSPORT_SEEK_SET);
	if (ret == -1)
		return FALSE;

	return TRUE;
}


static mpc_int32_t
xmms_mpc_callback_tell (void *data)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *) data;
	xmms_transport_t *transport;

	g_return_val_if_fail (decoder, -1);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, -1);

	return xmms_transport_tell (transport);
}


static mpc_bool_t 
xmms_mpc_callback_canseek (void *data) {
	return TRUE;
}


static mpc_int32_t
xmms_mpc_callback_get_size (void *data)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *) data;
	xmms_transport_t *transport;

	g_return_val_if_fail (decoder, -1);
	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, -1);

	return xmms_transport_size (transport);
}


static gboolean
xmms_mpc_new (xmms_decoder_t *decoder)
{
	xmms_mpc_data_t *data;

	data = g_new0 (xmms_mpc_data_t, 1);
	xmms_decoder_private_data_set (decoder, data);

	data->reader.read = xmms_mpc_callback_read;
	data->reader.seek = xmms_mpc_callback_seek;
	data->reader.tell = xmms_mpc_callback_tell;
	data->reader.canseek = xmms_mpc_callback_canseek;
	data->reader.get_size = xmms_mpc_callback_get_size;
	data->reader.data = decoder;

	return TRUE;
}


static gboolean
xmms_mpc_init (xmms_decoder_t *decoder, gint mode)
{
	xmms_mpc_data_t *data;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	mpc_streaminfo_init (&data->info);

	if (mpc_streaminfo_read (&data->info, &data->reader) != ERROR_CODE_OK)
		return FALSE;

	if (mode & XMMS_DECODER_INIT_DECODING) {
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_FLOAT,
		                         data->info.channels, data->info.sample_freq);

		if (!xmms_decoder_format_finish (decoder))
			return FALSE;

		mpc_decoder_setup (&data->decoder, &data->reader);

		/* why? this is strange! dist otherwise!
		 * nobody else does this
		 */
		mpc_decoder_scale_output (&data->decoder, 0.45);

		if (!mpc_decoder_initialize (&data->decoder, &data->info))
			return FALSE;
	}

	return TRUE;
}


static void
xmms_mpc_cache_streaminfo (xmms_decoder_t *decoder)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	xmms_mpc_data_t *data;
	gint bitrate, duration;
	gchar buf[8];

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	entry = xmms_decoder_medialib_entry_get (decoder);
	session = xmms_medialib_begin ();

	bitrate = (data->info.bitrate) ? data->info.bitrate :
	                                 data->info.average_bitrate;

	xmms_medialib_entry_property_set_int (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
	                                      bitrate);

	xmms_medialib_entry_property_set_int (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE,
	                                      data->info.sample_freq);

	duration = (int) mpc_streaminfo_get_length (&data->info) * 1000;
	xmms_medialib_entry_property_set_int (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
	                                      duration);

	if (data->info.gain_album) {
		gint tmp = exp ((M_LN10 / 2000.0) * data->info.gain_album);
		g_snprintf (buf, sizeof (buf), "%f", pow (10, tmp / 20));
		xmms_medialib_entry_property_set_str (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM,
		                                      buf);
	}

	if (data->info.gain_title) {
		gint tmp = exp ((M_LN10 / 2000.0) * data->info.gain_title);
		g_snprintf (buf, sizeof (buf), "%f", pow (10, tmp / 20));
		xmms_medialib_entry_property_set_str (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK,
		                                      buf);
	}

	if (data->info.peak_album) {
		g_snprintf (buf, sizeof (buf), "%hi", data->info.peak_album);
		xmms_medialib_entry_property_set_str (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM,
		                                      buf);
	}

	if (data->info.peak_title) {
		g_snprintf (buf, sizeof (buf), "%hi", data->info.peak_title);
		xmms_medialib_entry_property_set_str (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK,
		                                      buf);
	}

	xmms_medialib_end (session);
}


typedef enum { STRING, INTEGER } ptype;
typedef struct {
	gchar *vname;
	gchar *xname;
	ptype type;
} props ;

static props properties[] = {
	{ "title",  XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,   STRING  },
	{ "album",  XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,   STRING  },
	{ "artist", XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,  STRING  },
	{ "track",  XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, INTEGER },
	{ "genre",  XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,   STRING  },
};


static void
xmms_mpc_get_mediainfo (xmms_decoder_t *decoder)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	xmms_transport_t *transport;
	xmms_mpc_data_t *data;
	xmms_error_t error;
	gchar header[XMMS_APE_HEADER_SIZE];
	gchar *buff, *val;
	gint ret, len, i, tmp;


	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);
	
	transport = xmms_decoder_transport_get (decoder);
	g_return_if_fail (transport);

	/* seek to the APEv2 footer */
	ret = xmms_transport_seek (transport, -XMMS_APE_HEADER_SIZE, XMMS_TRANSPORT_SEEK_END);
	g_return_if_fail ((ret >= 0));

	ret = xmms_transport_read (transport, header, XMMS_APE_HEADER_SIZE, &error);
	g_return_if_fail ((ret >= 0));

	/* query footer for tag length */
	len = xmms_ape_get_size (header);
	g_return_if_fail ((len >= 0));

	/* seek to first apetag item */
	ret = xmms_transport_seek (transport, -len, XMMS_TRANSPORT_SEEK_END);
	g_return_if_fail ((ret >= 0));

	/* max length? */
	buff = (gchar *) malloc (len * sizeof (gchar *));
	ret = xmms_transport_read (transport, buff, len, &error);
	if (ret != len) {
		g_free (buff);
		return;
	}

	entry = xmms_decoder_medialib_entry_get (decoder);

	session = xmms_medialib_begin ();

	for (i = 0; i < G_N_ELEMENTS (properties); i++) {
		val = xmms_ape_get_text (properties[i].vname, buff, len);

		if (val) {
			if (properties[i].type == INTEGER) {
				tmp = strtol (val, NULL, 10);
				xmms_medialib_entry_property_set_int (session, entry,
				                                      properties[i].xname, tmp);
			} else {
				xmms_medialib_entry_property_set_str (session, entry,
				                                      properties[i].xname, val);
			}
			g_free (val);
		}
	}

	xmms_medialib_end (session);

	xmms_mpc_cache_streaminfo (decoder);
}


static gboolean
xmms_mpc_seek (xmms_decoder_t *decoder, guint samples)
{
	xmms_mpc_data_t *data;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	return mpc_decoder_seek_sample (&data->decoder, samples);
}


static gboolean
xmms_mpc_decode_block (xmms_decoder_t *decoder)
{
	MPC_SAMPLE_FORMAT buffer[MPC_DECODER_BUFFER_LENGTH];
	xmms_mpc_data_t *data;
	mpc_uint32_t ret;
	guint bizarro;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	ret = mpc_decoder_decode (&data->decoder, buffer, NULL, NULL);
	if (ret == -1 || ret == 0)
		return FALSE;

	bizarro = xmms_sample_size_get (XMMS_SAMPLE_FORMAT_FLOAT) * data->info.channels;
	xmms_decoder_write (decoder, (gchar *) buffer, ret * bizarro);

	return TRUE;
}


void
xmms_mpc_destroy (xmms_decoder_t *decoder)
{
	xmms_mpc_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	g_free (data);
}
