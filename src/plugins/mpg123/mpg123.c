/*  XMMS2 plugin for decoding MPEG audio using libmpg123
 *  Copyright (C) 2007-2017 XMMS2 Team
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
 *
 *  For libmpg123 API have a look at http://mpg123.org/api/ .
 *
 *  This is also a very basic mpg123 decoder plugin.
 *  Configurable goodies of libmpg123 are missing:
 *   - equalizer (fast, using the MPEG frequency bands directly)
 *   - forced mono, resampling (xmms2 can handle that itself, I guess)
 *   - choose RVA preamp (album/mix, from lame tag ReplayGain info or
 *     ID3v2 tags)
 *  This should be easy to add for an XMMS2 hacker.
 *
 *  Note on metadata:
 *  With libmpg123 you can get at least all text and comment ID3v2
 *  (version 2.2, 2.3, 2.4) frames as well as the usual id3v1 info
 *  (when you get to the end of the file...).
 *  The decoder also likes to read ID3 tags for getting RVA-related
 *  info that players like foobar2000 store there... Now the problem is:
 *  Usually, the id3 xform reads and cuts the id3 data, killing the info
 *  for mpg123...
 *  Perhaps one can make the generic id3 plugin store the necessary info
 *  for retrieval here, or just keep the id3 tags there...
 */

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_medialib.h>

#include <stdio.h>
#include <glib.h>
#include <mpg123.h>

#include "../mp3_common/id3v1.c"

#define BUFSIZE 4096

typedef struct xmms_mpg123_data_St {
	mpg123_handle *decoder;
	mpg123_pars *param;

	glong samplerate;
	gint channels;
	gboolean eof_found;
	gint filesize;

	/* input data buffer */
	guint8 buf[BUFSIZE];
} xmms_mpg123_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_mpg123_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_mpg123_read (xmms_xform_t *xform, xmms_sample_t *buf,
                              gint len, xmms_error_t *err);
static gboolean xmms_mpg123_init (xmms_xform_t *xform);
static void xmms_mpg123_destroy (xmms_xform_t *xform);
static gint64 xmms_mpg123_seek (xmms_xform_t *xform, gint64 samples,
                                xmms_xform_seek_mode_t whence,
                                xmms_error_t *err);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN_DEFINE ("mpg123", "mpg123 decoder", XMMS_VERSION,
                          "mpg123 decoder for MPEG 1.0/2.0/2.5 layer 1/2/3 audio",
                          xmms_mpg123_plugin_setup);

static gboolean
xmms_mpg123_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;
	int result;

	result = mpg123_init ();
	if (result != MPG123_OK) {
	    return FALSE;
	}

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_mpg123_init;
	methods.destroy = xmms_mpg123_destroy;
	methods.read = xmms_mpg123_read;
	methods.seek = xmms_mpg123_seek;
	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_config_property_register (xform_plugin,
	                                            "id3v1_encoding",
	                                            "ISO8859-1",
	                                            NULL,
	                                            NULL);

	xmms_xform_plugin_config_property_register (xform_plugin, "id3v1_enable",
	                                            "1", NULL, NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/mpeg",
	                              XMMS_STREAM_TYPE_PRIORITY,
	                              40,
	                              XMMS_STREAM_TYPE_END);

	/* Well, I usually only see mp3 and mp2 ... layer 1 files
	 * are quite rare.
	 */
	xmms_magic_extension_add ("audio/mpeg", "*.mp3");
	xmms_magic_extension_add ("audio/mpeg", "*.mp2");
	xmms_magic_extension_add ("audio/mpeg", "*.mp1");

	/* That's copied from the mad xform. */
	xmms_magic_add ("mpeg header", "audio/mpeg",
	                "0 beshort&0xfff6 0xfff6",
	                "0 beshort&0xfff6 0xfff4",
	                "0 beshort&0xffe6 0xffe2",
	                NULL);

	return TRUE;
}

static gboolean
xmms_mpg123_init (xmms_xform_t *xform)
{
	xmms_mpg123_data_t *data;
	const long *rates;
	size_t num_rates;
	int encoding;
	off_t length;
	int i, result;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_mpg123_data_t, 1);
	xmms_xform_private_data_set (xform, data);

	/* Get the total size of this stream and store it for later */
	if (xmms_xform_metadata_get_int (xform,
	                                 XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE,
	                                 &result)) {
		data->filesize = result;
	}

	mpg123_rates (&rates, &num_rates);

	data->param = mpg123_new_pars (&result);
	g_return_val_if_fail (data->param, FALSE);

	/* Create a quiet (stderr) decoder with auto choosen optimization.
	 * Stuff set here should be tunable via plugin config properties!
	 * You can also change some things during playback...
	 */
	mpg123_par (data->param, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
	mpg123_par (data->param, MPG123_ADD_FLAGS, MPG123_GAPLESS, 0);
	/* choose: MPG123_RVA_OFF, MPG123_RVA_MIX, MPG123_RVA_ALBUM
	 * xmms2 has its own ReplayGain plugin to handle the RVA field */
	mpg123_par (data->param, MPG123_RVA, MPG123_RVA_OFF, 0);

	/* You could choose a decoder from the list provided by
	 * mpg123_supported_decoders () and give that as second parameter.
	 */
	data->decoder = mpg123_parnew (data->param, NULL, &result);
	if (data->decoder == NULL) {
		xmms_log_error ("%s", mpg123_plain_strerror (result));
		goto bad;
	}

	/* Prepare for buffer input feeding. */
	result = mpg123_open_feed (data->decoder);
	if (result != MPG123_OK) {
		goto mpg123_bad;
	}

	/* Let's always decode to signed 16bit for a start.
	   Any mpg123-supported sample rate is accepted. */
	if (MPG123_OK != mpg123_format_none (data->decoder)) {
		goto mpg123_bad;
	}

	for (i = 0; i < num_rates; i++) {
		result = mpg123_format (data->decoder, rates[i],
		                        MPG123_MONO | MPG123_STEREO,
		                        MPG123_ENC_SIGNED_16);
		if (result != MPG123_OK) {
			goto mpg123_bad;
		}
	}

	/* Fetch ID3v1 data from the end of file if possible */
	result = xmms_id3v1_get_tags (xform);
	if (result < 0) {
		xmms_log_error ("Seeking error when reading ID3v1 tags");
		goto bad;
	} else if (data->filesize > result) {
		/* Reduce the size of tag data from the filesize */
		data->filesize -= result;
	}

	/* Read data from input until decoded data is available from decoder */
	do {
		/* Parse stream and get info. */
		gint ret;
		xmms_error_t err;

		ret = xmms_xform_read (xform, data->buf, BUFSIZE, &err);
		if (ret < 0) {
			xmms_log_error ("Error when trying to find beginning of stream");
			goto bad;
		} else if (ret == 0) {
			/* EOF reached before format was found, handled after loop */
			break;
		}

		/* With zero output size nothing is actually outputted */
		result = mpg123_decode (data->decoder, data->buf,
		                        (size_t) ret, NULL, 0, NULL);
	} while (result == MPG123_NEED_MORE); /* Keep feeding... */

	if (result != MPG123_NEW_FORMAT) {
		xmms_log_error ("Unable to find beginning of stream (%s)!",
		                result == MPG123_ERR ? mpg123_strerror (data->decoder)
		                : "unexpected EOF");
		goto bad;
	}

	result = mpg123_getformat (data->decoder, &data->samplerate,
	                           &data->channels, &encoding);
	if (result != MPG123_OK) {
		goto mpg123_bad;
	}

	/* Set the filesize so it can be used for duration estimation */
	if (data->filesize > 0) {
		mpg123_set_filesize (data->decoder, data->filesize);
	}

	/* Get duration in samples, convert to ms and save to xmms2 */
	length = mpg123_length (data->decoder);
	if (length > 0 &&
	    !xmms_xform_metadata_get_int (xform,
		                              XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
		                              &i)) {
		length = (off_t) ((gfloat) length / data->samplerate * 1000);
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
		                             (gint) length);
	}

	XMMS_DBG ("mpg123: got stream with %li Hz %i channels, encoding %i",
	          data->samplerate, data->channels, encoding);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             (gint) data->samplerate,
	                             XMMS_STREAM_TYPE_END);
	return TRUE;

mpg123_bad:
	xmms_log_error ("mpg123 error: %s", mpg123_strerror (data->decoder));

bad:
	mpg123_delete (data->decoder);
	mpg123_delete_pars (data->param);
	g_free (data);

	return FALSE;
}

static void
xmms_mpg123_destroy (xmms_xform_t *xform)
{
	xmms_mpg123_data_t *data;

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if (data != NULL) {
		mpg123_delete (data->decoder);
		mpg123_delete_pars (data->param);
	}
	g_free (data);
}

static gint
xmms_mpg123_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                  xmms_error_t *err)
{
	xmms_mpg123_data_t *data;
	int result = MPG123_OK;
	size_t read = 0;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	while (read == 0) {
		gint ret = 0;

		if (result == MPG123_NEED_MORE) {
			ret = xmms_xform_read (xform, data->buf, BUFSIZE, err);
			if (ret < 0) {
				return ret;
			} else if (ret == 0) {
				data->eof_found = TRUE;
			}
		}

		result = mpg123_decode (data->decoder, data->buf, (size_t) ret,
		                        buf, len, &read);

		if (result == MPG123_NEED_MORE && data->eof_found) {
			/* We need more data, but there's none available
			 * so libmpg123 apparently missed an EOF */
			result = MPG123_DONE;
			break;
		} else if (result != MPG123_OK && result != MPG123_NEED_MORE) {
			/* This is some uncommon result like EOF, handle outside
			 * the loop */
			break;
		}
	}

	if (result == MPG123_DONE) {
		/* This is just normal EOF reported from libmpg123 */
		XMMS_DBG ("Got EOF while decoding stream");
		return 0;
	} else if (result == MPG123_NEW_FORMAT) {
		/* FIXME: When we can handle format changes, modify this */
		xmms_error_set (err,
		                XMMS_ERROR_GENERIC,
		                "The output format changed, XMMS2 can't handle that");
		return -1;
	} else if (result == MPG123_ERR) {
		xmms_error_set (err,
		                XMMS_ERROR_GENERIC,
		                mpg123_strerror (data->decoder));
		return -1;
	}

	return (gint) read;
}

static gint64
xmms_mpg123_seek (xmms_xform_t *xform, gint64 samples,
                  xmms_xform_seek_mode_t whence,
                  xmms_error_t *err)
{
	xmms_mpg123_data_t *data;
	gint64 ret;
	off_t byteoff, samploff;
	int mwhence = -1;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	if (whence == XMMS_XFORM_SEEK_SET) {
		mwhence = SEEK_SET;
	} else if (whence == XMMS_XFORM_SEEK_CUR) {
		mwhence = SEEK_CUR;
	} else if (whence == XMMS_XFORM_SEEK_END) {
		mwhence = SEEK_END;
	}

	/* Get needed input position and possibly reached sample offset
	 * from mpg123.
	 */
	samploff = mpg123_feedseek (data->decoder, samples, mwhence,
	                            &byteoff);

	XMMS_DBG ("seeked to %li ... input stream seek following",
	          (long) samploff);

	if (samploff < 0) {
		xmms_error_set (err,
		                XMMS_ERROR_GENERIC,
		                mpg123_strerror (data->decoder));
		return -1;
	}

	/* Seek in input stream. */
	ret = xmms_xform_seek (xform, byteoff, XMMS_XFORM_SEEK_SET, err);
	if (ret < 0) {
		return ret;
	}

	return samploff;
}
