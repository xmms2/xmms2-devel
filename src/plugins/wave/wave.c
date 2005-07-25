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
  * @file wave decoder.
  * @url http://msdn.microsoft.com/library/en-us/dnnetcomp/html/WaveInOut.asp?frame=true#waveinout_topic_003
  */


#include "xmms/xmms_defs.h"
#include "xmms/xmms_decoderplugin.h"
#include "xmms/xmms_log.h"

#include <glib.h>
#include <string.h>

typedef struct xmms_wave_data_St {
	guint16 channels;
	guint32 samplerate;
	guint16 bits_per_sample;
	guint bytes_total;
} xmms_wave_data_t;

/*
 * Defines
 */
#define WAVE_HEADER_SIZE 44

#define GET_16(buf, val) \
	g_assert (sizeof (val) == 2); \
	memcpy (&val, buf, 2); \
	buf += 2; \
	val = GUINT16_TO_LE (val);

#define GET_32(buf, val) \
	g_assert (sizeof (val) == 4); \
	memcpy (&val, buf, 4); \
	buf += 4; \
	val = GUINT32_TO_LE (val);

#define GET_STR(buf, str, len) \
	strncpy ((gchar *) str, (gchar *)buf, len); \
	str[len] = '\0'; \
	buf += len;

/*
 * Function prototypes
 */

static gboolean xmms_wave_can_handle (const gchar *mimetype);
static gboolean xmms_wave_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_wave_decode_block (xmms_decoder_t *decoder);
static void xmms_wave_get_media_info (xmms_decoder_t *decoder);
static void xmms_wave_destroy (xmms_decoder_t *decoder);
static gboolean xmms_wave_init (xmms_decoder_t *decoder, gint mode);
static gboolean xmms_wave_seek (xmms_decoder_t *decoder, guint samples);

static gboolean read_wave_header (xmms_wave_data_t *data, guint8 *buf);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER,
	                          XMMS_DECODER_PLUGIN_API_VERSION,
	                          "wave",
	                          "Wave decoder " XMMS_VERSION,
	                          "Wave decoder");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://msdn.microsoft.com/"
	                                     "library/en-us/dnnetcomp/html/"
	                                     "WaveInOut.asp?frame=true"
	                                     "#waveinout_topic_003");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_wave_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_wave_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_wave_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_wave_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_wave_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_wave_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO, xmms_wave_get_media_info);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);

	return plugin;
}

static gboolean
xmms_wave_can_handle (const gchar *mimetype)
{
	g_return_val_if_fail (mimetype, FALSE);

	if (!g_strcasecmp (mimetype, "audio/x-wav")) {
		return TRUE;
	}

	if (!g_strcasecmp (mimetype, "audio/wave")) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
xmms_wave_new (xmms_decoder_t *decoder, const gchar *mimetype)
{
	xmms_wave_data_t *data;

	data = g_new0 (xmms_wave_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_decoder_private_data_set (decoder, data);

	return TRUE;
}

static void
xmms_wave_get_media_info (xmms_decoder_t *decoder)
{
	xmms_wave_data_t *data;
	xmms_medialib_entry_t entry;
	gdouble playtime;
	guint samples_total, bitrate;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	entry = xmms_decoder_medialib_entry_get (decoder);

	samples_total = data->bytes_total / (data->bits_per_sample / 8);
	playtime = (gdouble) samples_total / data->samplerate / data->channels;

	xmms_medialib_entry_property_set_int (entry,
					      XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
					      playtime * 1000);

	bitrate = data->bits_per_sample * data->samplerate / data->channels;
	xmms_medialib_entry_property_set_int (entry,
					      XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
					      bitrate);

	xmms_medialib_entry_send_update (entry);
}

static gboolean
xmms_wave_init (xmms_decoder_t *decoder, gint mode)
{
	xmms_transport_t *transport;
	xmms_wave_data_t *data;
	xmms_error_t error;
	xmms_sample_format_t sample_fmt;
	guint8 hdr[WAVE_HEADER_SIZE];
	gint read = 0;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	while (read < sizeof (hdr)) {
		gint ret = xmms_transport_read (transport, (gchar *)hdr + read,
		                                sizeof (hdr) - read, &error);

		if (ret <= 0) {
			XMMS_DBG ("Could not read wave header");
			return FALSE;
		}

		read += ret;
		g_assert (read >= 0);
	}

	if (!read_wave_header (data, hdr)) {
		XMMS_DBG ("Not a valid Wave stream");
		return FALSE;
	}

	if (!(mode & XMMS_DECODER_INIT_DECODING)) {
		return TRUE;
	}

	sample_fmt = (data->bits_per_sample == 8 ? XMMS_SAMPLE_FORMAT_U8
	                                         : XMMS_SAMPLE_FORMAT_S16);
	xmms_decoder_format_add (decoder, sample_fmt, data->channels,
	                         data->samplerate);

	return !!xmms_decoder_format_finish (decoder);
}

static gboolean
xmms_wave_decode_block (xmms_decoder_t *decoder)
{
	xmms_transport_t *transport;
	xmms_wave_data_t *data;
	xmms_error_t error;
	gchar buf[4096];
	gint ret;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	ret = xmms_transport_read (transport, buf, sizeof (buf), &error);
	if (ret <= 0) {
		return FALSE;
	}

#if G_BYTE_ORDER == G_BIG_ENDIAN
	if (data->bits_per_sample == 16) {
		gint16 *s = (gint16 *) buf;
		gint i;

		for (i = 0; i < (ret / 2); i++) {
			s[i] = GINT16_FROM_LE (s[i]);
		}
	}
#endif

	xmms_decoder_write (decoder, buf, ret);

	return TRUE;
}

static gboolean
xmms_wave_seek (xmms_decoder_t *decoder, guint samples)
{
	xmms_transport_t *transport;
	xmms_wave_data_t *data;
	guint offset = WAVE_HEADER_SIZE;
	gint ret;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	offset += samples * (data->bits_per_sample / 8) * data->channels;
	if (offset > data->bytes_total) {
		XMMS_DBG ("Trying to seek past end of stream");

		return FALSE;
	}

	ret = xmms_transport_seek (transport, offset,
	                           XMMS_TRANSPORT_SEEK_SET);

	return ret != -1;
}

static void
xmms_wave_destroy (xmms_decoder_t *decoder)
{
	g_return_if_fail (decoder);

	g_free (xmms_decoder_private_data_get (decoder));
}

static gboolean
read_wave_header (xmms_wave_data_t *data, guint8 *buf)
{
	gchar stmp[5];
	guint32 tmp32, data_size;
	guint16 tmp16;

	GET_STR (buf, stmp, 4);
	if (strcmp (stmp, "RIFF")) {
		XMMS_DBG ("No RIFF data");
		return FALSE;
	}

	GET_32 (buf, data_size);
	data_size += 8;

	GET_STR (buf, stmp, 4);
	if (strcmp (stmp, "WAVE")) {
		XMMS_DBG ("No Wave data");
		return FALSE;
	}

	GET_STR (buf, stmp, 4);
	if (strcmp (stmp, "fmt ")) {
		XMMS_DBG ("Format chunk missing");
		return FALSE;
	}

	GET_32 (buf, tmp32);
	if (tmp32 != 16) {
		XMMS_DBG ("Invalid format chunk length");
		return FALSE;
	}

	GET_16 (buf, tmp16); /* format tag */
	if (tmp16 != 1) {
		XMMS_DBG ("Unhandled format tag: %i", tmp16);
		return FALSE;
	}

	GET_16 (buf, data->channels);
	if (data->channels < 1 || data->channels > 2) {
		XMMS_DBG ("Unhandled number of channels: %i", data->channels);
		return FALSE;
	}

	GET_32 (buf, data->samplerate);
	if (data->samplerate != 8000 && data->samplerate != 11025 &&
	    data->samplerate != 22050 && data->samplerate != 44100) {
		XMMS_DBG ("Invalid samplerate: %i", data->samplerate);
		return FALSE;
	}

	GET_32 (buf, tmp32);
	GET_16 (buf, tmp16);

	GET_16 (buf, data->bits_per_sample);
	if (data->bits_per_sample != 8 && data->bits_per_sample != 16) {
		XMMS_DBG ("Unhandled bits per sample: %i",
		          data->bits_per_sample);
		return FALSE;
	}

	GET_STR (buf, stmp, 4);
	if (strcmp (stmp, "data")) {
		XMMS_DBG ("Data chunk missing");
		return FALSE;
	}

	GET_32 (buf, data->bytes_total);
	if (data->bytes_total + WAVE_HEADER_SIZE != data_size) {
		XMMS_DBG ("Data chunk size doesn't match RIFF chunk size");
		/* don't return FALSE here, we try to read it anyway */
	}

	return TRUE;
}
