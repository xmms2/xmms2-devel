/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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
 *  ---------------------------------
 *
 *  Standard MIDI File input plugin for XMMS2
 *
 *  This plugin reads both .mid and .rmi files, and converts them into
 *  audio/rawmidi format, ready to be rendered into audio by one of the MIDI
 *  synth plugins.
 */

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_sample.h>
#include <xmms/xmms_medialib.h>
#include <xmms/xmms_log.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * Type definitions
 */

typedef struct xmms_mid1_data_St {
	GString *chunked_data;
	gulong pos;
} xmms_mid1_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_mid1_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_mid1_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static void xmms_mid1_destroy (xmms_xform_t *xform);
static gboolean xmms_mid1_init (xmms_xform_t *xform);
static gint64 xmms_mid1_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN ("mid1",
                   "MIDI file format-1 demuxer",
                   XMMS_VERSION,
                   "MIDI file format-1 demuxer",
                   xmms_mid1_plugin_setup);

static gboolean
xmms_mid1_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_mid1_init;
	methods.destroy = xmms_mid1_destroy;
	methods.read = xmms_mid1_read;
	methods.seek = xmms_mid1_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	/*
	xmms_plugin_info_add (plugin, "URL", "http://www.xmms2.org/");
	xmms_plugin_info_add (plugin, "Author", "Adam Nielsen <malvineous@shikadi.net>");
	xmms_plugin_info_add (plugin, "License", "LGPL");
	*/

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/mid-0",
	                              NULL);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/mid-1",
	                              NULL);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/riffmidi",
	                              NULL);

	xmms_magic_add ("Standard MIDI file (format-0)", "audio/mid-0",
	                "0 string MThd",
	                ">8 beshort 0",
	                NULL);
	xmms_magic_add ("Standard MIDI file (format-1)", "audio/mid-1",
	                "0 string MThd",
	                ">8 beshort 1",
	                NULL);
	xmms_magic_add ("Microsoft RIFF MIDI file", "audio/riffmidi",
	                "0 string RIFF",
	                ">8 string RMID",
	                NULL);

	return TRUE;
}

static void
xmms_mid1_destroy (xmms_xform_t *xform)
{
	xmms_mid1_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if (data->chunked_data)
		g_string_free (data->chunked_data, TRUE);

	g_free (data);

}

/* Seeking is in bytes */
static gint64
xmms_mid1_seek (xmms_xform_t *xform, gint64 samples,
               xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_mid1_data_t *data;
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	/* Adjust our internal pointer by the seek amount */
	switch (whence) {
		case XMMS_XFORM_SEEK_CUR:
			data->pos += samples;
			break;
		case XMMS_XFORM_SEEK_SET:
			data->pos = samples;
			break;
		case XMMS_XFORM_SEEK_END:
			data->pos = data->chunked_data->len + samples;
			break;
	}

	return data->pos;
}

static gboolean
xmms_mid1_init (xmms_xform_t *xform)
{
	xmms_error_t error;
	xmms_mid1_data_t *data;
	guchar buf[4096];
	gulong len;
	gint ret;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_mid1_data_t, 1);
	g_return_val_if_fail (data, FALSE);
	xmms_xform_private_data_set (xform, data);

	ret = xmms_xform_read (xform, buf, 4, &error);
	if (strncmp ((char *)buf, "RIFF", 4) == 0) {
		/* This is an .rmi file, find the data chunk */
		gboolean is_rmid = FALSE;

		/* Skip the RIFF length and RMID type (we wouldn't be here if it wasn't
		 * RMID thanks to xmms_magic_add above.)
		 */
		xmms_xform_read (xform, buf, 8, &error); /* skip RIFF length */
		for (;;) {
			/* Get chunk type and length */
			xmms_xform_read (xform, buf, 8, &error);
			if (strncmp ((char *)buf, "data", 4) == 0) {
				/* We found the data chunk, the rest is just a normal MIDI file.  We
				 * chew up the "MThd" signature though, as the code below expects it
				 * to be gone (as it would in a normal MIDI file when we eat it to
				 * check the RIFF header above.) */
				is_rmid = TRUE;
				ret = xmms_xform_read (xform, buf, 4, &error);
				break;
			}

			/* If we're here, this isn't the "data" chunk, so skip over it */
			len = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
			while (len > 0) {
				ret = xmms_xform_read (xform, buf, MIN (len, sizeof (buf)), &error);
				if (ret < 1) break;
				len -= ret;
			}
		}

		if (!is_rmid) {
			xmms_log_error ("RMID file has no 'data' chunk!");
			goto cleanup;
		}
	}

	/* Once we get here we're just after the MThd signature */
	ret = xmms_xform_read (xform, buf, 10, &error);
	gint header_len = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	if (header_len != 6) {
		xmms_log_error ("Unexpected MThd header length");
		goto cleanup;
	}

	/* Don't need to check it's a type-0 or 1 as the magic matching did it */

	guint track_count = (buf[6] << 8) | buf[7];
	if (track_count == 0) {
		xmms_log_error ("MIDI file has no tracks?!");
		goto cleanup;
	}

	guint ticks_per_quarter_note = (buf[8] << 8) | buf[9];
	if (ticks_per_quarter_note & 0x8000) {
		/* TODO */
		xmms_log_error ("SMPTE timing not implemented");
		goto cleanup;
	}
	xmms_xform_auxdata_set_int (xform, "tempo", ticks_per_quarter_note);

	data->chunked_data = g_string_sized_new (1024);

	/* Load all the tracks */
	while (xmms_xform_read (xform, buf, 8, &error) == 8) {
		len = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
		XMMS_DBG ("%.4s len is %lu", buf, len);

		if (strncmp ((char *)buf, "MTrk", 4) != 0) {
			XMMS_DBG ("Ignoring unknown chunk: %.4s", buf);
			/* Skip over the chunk - we don't use seek as it's not always implemented
			 * by the parent xform.
			 */
			while (len > 0) {
				ret = xmms_xform_read (xform, buf, MIN (len, sizeof (buf)), &error);
				if (ret < 1) break;
				len -= ret;
			}
		} else {
			/* Append the big-endian length */
			g_string_append_len (data->chunked_data, (gchar *)&buf[4], 4);

			for (;;) {
				gulong amt = MIN (len, sizeof (buf));
				if (amt == 0) break;
				ret = xmms_xform_read (xform, (gchar *)buf, amt, &error);
				if (ret != amt) {
					/* Short read */
					XMMS_DBG ("Short read");
					goto cleanup;
				}
				g_string_append_len (data->chunked_data, (gchar *)buf, amt);
				len -= amt;
			}
		}
	}

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/miditracks",
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             16,
	                             XMMS_STREAM_TYPE_END);

	return TRUE;

cleanup:
	if (data->chunked_data)
		g_string_free (data->chunked_data, TRUE);

	g_free (data);

	return FALSE;
}

static gint
xmms_mid1_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	xmms_mid1_data_t *data;
	data = xmms_xform_private_data_get (xform);

	if (data->pos + len > data->chunked_data->len)
		len = data->chunked_data->len - data->pos;

	memcpy (buf, &(data->chunked_data->str[data->pos]), len);
	data->pos += len;
	return len;
}
