/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
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
  * @file gme decoder.
  * @url http://xmms2.org/wiki/Notes_from_developing_an_xform_plugin
  */

#include <glib.h>
#include <stdlib.h>
#include <gme/gme.h>

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_medialib.h"

#define GME_DEFAULT_SAMPLE_RATE 44100
#define GME_DEFAULT_SONG_LENGTH 300
#define GME_DEFAULT_SONG_LOOPS 2
#define GME_DEFAULT_STEREO_DEPTH -1.0

extern "C" {

/*
 * Persistent data
 *
 */
typedef struct xmms_gme_data_St {
	Music_Emu *emu; /* An emulation instance for the GME library */
	int samplerate; /* The sample rate, set by the user */
} xmms_gme_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_gme_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_gme_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static gboolean xmms_gme_init (xmms_xform_t *decoder);
static void xmms_gme_destroy (xmms_xform_t *decoder);
static gint64 xmms_gme_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN_DEFINE ("gme",
                          "Game Music decoder", XMMS_VERSION,
                          "Game Music Emulator music decoder",
                          xmms_gme_plugin_setup);

static gboolean
xmms_gme_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_gme_init;
	methods.destroy = xmms_gme_destroy;
	methods.read = xmms_gme_read;
	methods.seek = xmms_gme_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_config_property_register (xform_plugin, "loops", G_STRINGIFY (GME_DEFAULT_SONG_LOOPS), NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "maxlength", G_STRINGIFY (GME_DEFAULT_SONG_LENGTH), NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "samplerate", G_STRINGIFY (GME_DEFAULT_SAMPLE_RATE), NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "stereodepth", G_STRINGIFY (GME_DEFAULT_STEREO_DEPTH), NULL, NULL);

	/* todo: add other mime types */
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-spc",
	                              NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-nsf",
	                              NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-nsfe",
	                              NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-gbs",
	                              NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-gym",
	                              NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-vgm",
	                              NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-sap",
	                              NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-ay",
	                              NULL);

	/* todo: add other magic */
	xmms_magic_add ("SPC700 save state",
	                "application/x-spc",
	                "0 string SNES-SPC700 Sound File Data",
	                NULL);

	xmms_magic_add ("NSF file",
	                "application/x-nsf",
	                "0 string NESM",
	                NULL);

	xmms_magic_add ("NSFE file",
	                "application/x-nsfe",
	                "0 string NSFE",
	                NULL);

	xmms_magic_add ("GBS file",
	                "application/x-gbs",
	                "0 string GBS",
	                NULL);

	xmms_magic_add ("GYM file",
	                "application/x-gym",
	                "0 string GYMX",
	                NULL);

	xmms_magic_add ("VGM file",
	                "application/x-vgm",
	                "0 string Vgm",
	                NULL);

	xmms_magic_add ("SAP file",
	                "application/x-sap",
	                "0 string SAP",
	                NULL);

	xmms_magic_add ("AY file",
	                "application/x-ay",
	                "0 string ZXAYEMU",
	                NULL);

	/* todo: add other file extensions */
	xmms_magic_extension_add ("application/x-spc", "*.spc");
	xmms_magic_extension_add ("application/x-nsf", "*.nsf");
	xmms_magic_extension_add ("application/x-nsfe", "*.nsfe");
	xmms_magic_extension_add ("application/x-gbs", "*.gbs");
	xmms_magic_extension_add ("application/x-gym", "*.gym");
	xmms_magic_extension_add ("application/x-vgm", "*.vgm");
	xmms_magic_extension_add ("application/x-sap", "*.sap");
	xmms_magic_extension_add ("application/x-ay", "*.ay");

	return TRUE;
}

static gboolean
xmms_gme_init (xmms_xform_t *xform)
{
	xmms_gme_data_t *data;
	gme_err_t error;
	GString *file_contents; /* The raw data from the file. */
	gme_info_t *metadata = NULL;
	xmms_config_property_t *val;
	int loops;
	int maxlength;
	const char *subtune_str;
	int subtune = 0;
	long fadelen = -1;
	int samplerate;
	double stereodepth;


	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_gme_data_t, 1);

	xmms_xform_private_data_set (xform, data);

	val = xmms_xform_config_lookup (xform, "samplerate");
	samplerate = xmms_config_property_get_int (val);
	if (samplerate < 1)
		samplerate = GME_DEFAULT_SAMPLE_RATE;
	data->samplerate = samplerate;

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm", /* PCM samples */
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16, /* 16-bit signed */
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             2, /* stereo */
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             samplerate,
	                             XMMS_STREAM_TYPE_END);

	file_contents = g_string_new ("");

	for (;;) {
		xmms_error_t error;
		gchar buf[4096];
		gint ret;

		ret = xmms_xform_read (xform, buf, sizeof (buf), &error);
		if (ret == -1) {
			XMMS_DBG ("Error reading emulated music data");
			return FALSE;
		}
		if (ret == 0) {
			break;
		}
		g_string_append_len (file_contents, buf, ret);
	}

	error = gme_open_data (file_contents->str, file_contents->len, &data->emu, samplerate);

	g_string_free (file_contents, TRUE);

	if (error) {
		XMMS_DBG ("gme_open_data returned an error: %s", error);
		return FALSE;
	}

	if (xmms_xform_metadata_get_str (xform, "subtune", &subtune_str)) {
		subtune = strtol (subtune_str, NULL, 10);
		XMMS_DBG ("Setting subtune to %d", subtune);
		if ((subtune < 0 || subtune > gme_track_count (data->emu))) {
			XMMS_DBG ("Invalid subtune index");
			return FALSE;
		}
	} else {
		xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_SUBTUNES, gme_track_count (data->emu));
	}

	/*
	 *  Get metadata here
	 */
	error = gme_track_info (data->emu, &metadata, subtune);
	if (error) {
		XMMS_DBG ("Couldn't get GME track info: %s", error);
	} else {
		xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, metadata->song);
		xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST, metadata->author);
		xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM, metadata->game);
		xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT, metadata->comment);
		xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR, metadata->copyright);
		xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE, metadata->system);  /* I mapped genre to the system type */

		val = xmms_xform_config_lookup (xform, "loops");
		loops = xmms_config_property_get_int (val);

		XMMS_DBG ("intro_length = %d, loops = %d, loop_length = %d", metadata->intro_length, loops, metadata->loop_length);

		if (metadata->intro_length > 0) {
			if ((loops > 0) && (metadata->loop_length > 0)) {
				fadelen = metadata->intro_length + loops * metadata->loop_length;
				XMMS_DBG ("fadelen now = %ld", fadelen);
			} else {
				fadelen = metadata->length;
				XMMS_DBG ("fadelen now = %ld", fadelen);
			}
		}
	}

	val = xmms_xform_config_lookup (xform, "maxlength");
	maxlength = xmms_config_property_get_int (val);

	XMMS_DBG ("maxlength = %d seconds", maxlength);

	if (maxlength > 0 && (fadelen < 0 || (maxlength * 1000L < fadelen))) {
		fadelen = maxlength * 1000L;
		XMMS_DBG ("fadelen now = %ld", fadelen);
	}

	XMMS_DBG ("gme.fadelen = %ld", fadelen);

	val = xmms_xform_config_lookup (xform, "stereodepth");
	stereodepth = xmms_config_property_get_float (val);
	if (stereodepth >= 0.0 && stereodepth <= 1.0) {
		XMMS_DBG ("Setting stereo depth to %f.", stereodepth);
		gme_set_stereo_depth (data->emu, stereodepth);
	} else {
		XMMS_DBG ("gme.stereodepth = %f out of range 0.0 - 1.0; not setting.", stereodepth);
	}

	error = gme_start_track (data->emu, subtune);
	if (error) {
		XMMS_DBG ("gme_start_track returned an error: %s", error);
		gme_free_info (metadata);
		return FALSE;
	}

	if (fadelen > 0) {
		XMMS_DBG ("Setting song length and fade length...");
		xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION, fadelen);
		gme_set_fade (data->emu, fadelen);
	}

	gme_free_info (metadata);
	return TRUE;
}

static void
xmms_gme_destroy (xmms_xform_t *xform)
{
	xmms_gme_data_t *data;

	g_return_if_fail (xform);

	data = (xmms_gme_data_t *)xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if (data->emu)
		gme_delete (data->emu);

	g_free (data);

}

/* Read some data */
static gint
xmms_gme_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	xmms_gme_data_t *data;
	gme_err_t play_error;

	g_return_val_if_fail (xform, -1);

	data = (xmms_gme_data_t *)xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	if (gme_track_ended (data->emu))
		return 0;

	play_error = gme_play (data->emu, len/2, (short int *)buf);
	if (play_error) {
		XMMS_DBG ("gme_play returned an error: %s", play_error);
		xmms_error_set (err, XMMS_ERROR_GENERIC, play_error);
		return -1;
	}

	return len;
}

static gint64
xmms_gme_seek (xmms_xform_t *xform, gint64 samples,
               xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_gme_data_t *data;
	gint64 target_time;
	gint duration;
	int samplerate;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = (xmms_gme_data_t *) xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	samplerate = data->samplerate;

	if (samples < 0) {
		xmms_error_set (err, XMMS_ERROR_INVAL,
		                "Trying to seek before start of stream");
		return -1;
	}

	target_time = (samples / samplerate) * 1000;
	xmms_xform_metadata_get_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION, &duration);

	if (target_time > duration) {
		xmms_error_set (err, XMMS_ERROR_INVAL,
		                "Trying to seek past end of stream");
		return -1;
	}

	gme_seek (data->emu, target_time);

	return (gme_tell (data->emu) / 1000) * samplerate;
}

}
