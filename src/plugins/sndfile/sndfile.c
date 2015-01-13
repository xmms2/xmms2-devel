/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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
  * @file libsndfile decoder.
  * @url http://www.mega-nerd.com/libsndfile
  */

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_medialib.h>

#include <glib.h>
#include <string.h>
#include <sndfile.h>

#define SF2XMMS_META_TRANSCODE(STR, SF, XFORM, SF_STR_ID, XMMS_ML_PROP) \
	STR = sf_get_string (data->sndfile, SF_STR_ID); \
	if (STR != NULL && strlen (STR) > 0) { \
		xmms_xform_metadata_set_str (XFORM, XMMS_ML_PROP, str); \
	}

typedef struct xmms_sndfile_data_St {
	SF_VIRTUAL_IO sfvirtual;
	SF_INFO sf_info;
	SNDFILE *sndfile;
} xmms_sndfile_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_sndfile_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_sndfile_read (xmms_xform_t *xform, xmms_sample_t *buf,
                               gint len, xmms_error_t *error);
static void xmms_sndfile_get_media_info (xmms_xform_t *xform);
static void xmms_sndfile_destroy (xmms_xform_t *xform);
static gboolean xmms_sndfile_init (xmms_xform_t *xform);
static gint64 xmms_sndfile_seek (xmms_xform_t *xform, gint64 samples,
                                 xmms_xform_seek_mode_t whence,
                                 xmms_error_t *error);

static sf_count_t xmms_sf_virtual_get_filelen (void *priv);
static sf_count_t xmms_sf_virtual_seek (sf_count_t offset, int whence, void *priv);
static sf_count_t xmms_sf_virtual_tell (void *priv);
static sf_count_t xmms_sf_virtual_read (void *buffer, sf_count_t count, void *priv);
static sf_count_t xmms_sf_virtual_write (void const *buffer, sf_count_t count, void *priv);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN_DEFINE ("sndfile",
                          "Libsndfile decoder", XMMS_VERSION,
                          "Libsndfile decoder",
                          xmms_sndfile_plugin_setup);

static gboolean
xmms_sndfile_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_sndfile_init;
	methods.destroy = xmms_sndfile_destroy;
	methods.read = xmms_sndfile_read;
	methods.seek = xmms_sndfile_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-aiff",
	                              NULL);
	xmms_magic_add ("aiff header", "audio/x-aiff",
	                "0 string FORM", ">8 string AIFF", NULL);
	xmms_magic_add ("aiff-c header", "audio/x-aiff",
	                "0 string FORM", ">8 string AIFC", NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-au",
	                              NULL);
	xmms_magic_add ("au header", "audio/x-au",
	                "0 string .snd", NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-caf",
	                              NULL);
	xmms_magic_add ("caf header", "audio/x-caf",
	                "0 string caff", ">8 string desc", NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-paf",
	                              NULL);
	xmms_magic_add ("paf header", "audio/x-paf",
	                "0 byte 0x20", ">1 string paf", NULL);

	/* Add further libsndfile supported formats here. When adding formats based
	 * on MS wav format (w64, rf64 for instance) attention must be payed that
	 * we don't mask out the magics added by the wave plugin and vice versa.
	 */

	return TRUE;
}

static void
xmms_sndfile_get_media_info (xmms_xform_t *xform)
{
	xmms_sndfile_data_t *data;
	gdouble playtime;
	guint bitrate;
	gint filesize = 0, bps = -1;
	const gchar *metakey, *str;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	playtime = (gdouble) data->sf_info.frames / data->sf_info.samplerate;

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;

	if (xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		xmms_xform_metadata_set_int (xform, metakey, playtime * 1000);
	}

	switch (data->sf_info.format) {
		case SF_FORMAT_PCM_S8:
			bps = 8;
			break;
		case SF_FORMAT_PCM_16:
			bps = 16;
			break;
		case SF_FORMAT_PCM_24:
			bps = 24;
			break;
		case SF_FORMAT_PCM_32:
			bps = 32;
			break;
		case SF_FORMAT_PCM_U8:
			bps = 8;
			break;
		case SF_FORMAT_FLOAT:
			bps = 32;
			break;
		case SF_FORMAT_DOUBLE:
			bps = 64;
			break;
		case SF_FORMAT_ULAW:
		case SF_FORMAT_ALAW:
		case SF_FORMAT_IMA_ADPCM:
		case SF_FORMAT_MS_ADPCM:
		case SF_FORMAT_GSM610:
		case SF_FORMAT_VOX_ADPCM:
		case SF_FORMAT_G721_32:
		case SF_FORMAT_G723_24:
		case SF_FORMAT_G723_40:
		case SF_FORMAT_DWVW_12:
		case SF_FORMAT_DWVW_16:
		case SF_FORMAT_DWVW_24:
		case SF_FORMAT_DWVW_N:
		case SF_FORMAT_DPCM_8:
		case SF_FORMAT_DPCM_16:
		case SF_FORMAT_VORBIS:
		default:
			break;
	}

	if (bps >= 0) {
		bitrate = bps * data->sf_info.samplerate / data->sf_info.channels;
	} else {
		/* Approximate bitrate for compressed formats from the total file
		 * length and sample rate. */
		bitrate = filesize / playtime;
	}

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE;
	xmms_xform_metadata_set_int (xform, metakey, bitrate);

	SF2XMMS_META_TRANSCODE (str, data->sndfile, xform, SF_STR_ARTIST,
	                        XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST);
	SF2XMMS_META_TRANSCODE (str, data->sndfile, xform, SF_STR_ALBUM,
	                        XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM);
	SF2XMMS_META_TRANSCODE (str, data->sndfile, xform, SF_STR_COMMENT,
	                        XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT);
	SF2XMMS_META_TRANSCODE (str, data->sndfile, xform, SF_STR_COPYRIGHT,
	                        XMMS_MEDIALIB_ENTRY_PROPERTY_COPYRIGHT);
	SF2XMMS_META_TRANSCODE (str, data->sndfile, xform, SF_STR_DATE,
	                        XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR); /* Verify! */
/*	SF2XMMS_META_TRANSCODE (str, data->sndfile, xform, SF_STR_LICENSE,
	                        XMMS_MEDIALIB_ENTRY_PROPERTY_???); */
/*	SF2XMMS_META_TRANSCODE (str, data->sndfile, xform, SF_STR_SOFTWARE,
	                        XMMS_MEDIALIB_ENTRY_PROPERTY_???); */
	SF2XMMS_META_TRANSCODE (str, data->sndfile, xform, SF_STR_TITLE,
	                        XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE);

	/* We could fetch ID3 tags here as soon as libsndfile will support this -
	 * Erik of libsndfile has to provide the generic chunk handling though.
	 */
}

static sf_count_t
xmms_sf_virtual_get_filelen (void *priv)
{
	xmms_xform_t *xform = priv;
	gint filesize = 0; /* Return length 0 in case of failure. */

	xmms_xform_metadata_get_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE,
	                             &filesize);

	return filesize;
}

static sf_count_t
xmms_sf_virtual_seek (sf_count_t offset, int whence, void *priv)
{
	xmms_xform_t *xform = priv;
	gint64 ret = 0; /* Return offset 0 in case of failure. */
	xmms_error_t err;

	switch (whence) {
		case SEEK_SET:
			ret = xmms_xform_seek (xform, offset, XMMS_XFORM_SEEK_SET, &err);
			break;
		case SEEK_CUR:
			ret = xmms_xform_seek (xform, offset, XMMS_XFORM_SEEK_CUR, &err);
			break;
		case SEEK_END:
			ret = xmms_xform_seek (xform, offset, XMMS_XFORM_SEEK_END, &err);
			break;
	}

	return ret;
}

static sf_count_t
xmms_sf_virtual_tell (void *priv)
{
	xmms_xform_t *xform = priv;
	gint64 ret = 0; /* Return pos 0 in case of failure. */
	xmms_error_t err;

	ret = xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &err);

	return ret;
}

static sf_count_t
xmms_sf_virtual_read (void *buffer, sf_count_t count, void *priv)
{
	xmms_xform_t *xform = priv;
	gint64 ret = 0; /* Return read byte count of 0 in case of failure. */
	xmms_error_t err;

	ret = xmms_xform_read (xform, buffer, count, &err);

	return ret;
}

static sf_count_t
xmms_sf_virtual_write (const void *buffer, sf_count_t count, void *priv)
{
	/* Writing not supported - every write returns 0 bytes written. */
    return 0;
}

static gboolean
xmms_sndfile_init (xmms_xform_t *xform)
{
	xmms_sndfile_data_t *data;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_sndfile_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_xform_private_data_set (xform, data);

	data->sfvirtual.get_filelen = &xmms_sf_virtual_get_filelen;
	data->sfvirtual.seek = &xmms_sf_virtual_seek;
	data->sfvirtual.read = &xmms_sf_virtual_read;
	data->sfvirtual.write = &xmms_sf_virtual_write;
	data->sfvirtual.tell = &xmms_sf_virtual_tell;

	data->sndfile = sf_open_virtual (&data->sfvirtual, SFM_READ,
	                                 &data->sf_info, xform);
	if (data->sndfile == NULL) {
		char errstr[1024];
		sf_error_str (NULL, errstr, sizeof (errstr));
		xmms_log_error ("libsndfile: sf_open_virtual failed with \"%s\".",
		                errstr);
		g_free (data);
		return FALSE;
	}

	sf_command (data->sndfile, SFC_SET_SCALE_FLOAT_INT_READ, NULL, SF_TRUE);
	xmms_sndfile_get_media_info (xform);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S32,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->sf_info.channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->sf_info.samplerate,
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gint
xmms_sndfile_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                   xmms_error_t *error)
{
	xmms_sndfile_data_t *data;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	sf_count_t read = sf_read_int (data->sndfile, (int*)buf, len/sizeof (int));

	return read * sizeof (int);
}

static gint64
xmms_sndfile_seek (xmms_xform_t *xform, gint64 samples,
                   xmms_xform_seek_mode_t whence, xmms_error_t *error)
{
	xmms_sndfile_data_t *data;
	gint64 ret = -1;
	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (samples >= 0, -1);
	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET ||
	                      whence == XMMS_XFORM_SEEK_CUR ||
	                      whence == XMMS_XFORM_SEEK_END, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	switch ( whence ) {
		case XMMS_XFORM_SEEK_SET:
			ret = sf_seek (data->sndfile, samples, SEEK_SET);
			break;
		case XMMS_XFORM_SEEK_CUR:
			ret = sf_seek (data->sndfile, samples, SEEK_CUR);
			break;
		case XMMS_XFORM_SEEK_END:
			ret = sf_seek (data->sndfile, samples, SEEK_END);
			break;
	}

	return ret;
}

static void
xmms_sndfile_destroy (xmms_xform_t *xform)
{
	g_return_if_fail (xform);

	xmms_sndfile_data_t *data = xmms_xform_private_data_get (xform);

	sf_close (data->sndfile);

	g_free (data);
}
