/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <stdlib.h>
#include <glib.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_xformplugin.h>

#include "sc68.h"

typedef struct {
	sc68_disk_t *disk;
	int track;
} xmms_sc68_t;

static sc68_t *sc68_api;
static void *loaded_disk;

static gboolean xmms_sc68_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_sc68_init (xmms_xform_t *xform);
static void xmms_sc68_destroy (xmms_xform_t *xform);
static gint xmms_sc68_read (xmms_xform_t *xform, gpointer buffer, gint length,
                            xmms_error_t *error);
static int sc68_load_track (sc68_disk_t disk, int track, xmms_error_t *error);

XMMS_XFORM_PLUGIN_DEFINE ("sc68","Atari ST and Amiga decoder",
                          XMMS_VERSION,
                          "Atari ST and Amiga decoder based on sc68",
                          xmms_sc68_plugin_setup);

static gboolean
xmms_sc68_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	sc68_init_t settings;
	xmms_xform_methods_t methods;

	memset (&settings, 0, sizeof (settings));
	sc68_api = xmms_sc68_api_init(&settings);

	if (!sc68_api) {
		xmms_log_info ("Could not initialize sc68");
		xmms_sc68_error (sc68_api);
		return FALSE;
	}

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_sc68_init;
	methods.destroy = xmms_sc68_destroy;
	methods.read = xmms_sc68_read;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_magic_add ("sc68 header", "audio/stsound",
	                "0 string SC68 Music-file / (c) (BeN)jamin Gerard /"
	                " SasHipA-Dev  ", NULL);
	xmms_magic_add ("sndh header", "audio/stsound",
	                "0 string ICE!", NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/stsound",
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gboolean
xmms_sc68_init (xmms_xform_t *xform)
{
	const gchar *track_string;
	sc68_music_info_t disk_info;
	xmms_sc68_t *sc68;
	GString *disk_data;

	sc68 = g_new0 (xmms_sc68_t, 1);
	g_return_val_if_fail(sc68, FALSE);

	xmms_xform_private_data_set (xform, sc68);

	disk_data = g_string_new ("");

	for (;;) {
		gchar buffer [4096];
		gint read;
		xmms_error_t error;

		read = xmms_xform_read (xform, buffer, sizeof (buffer), &error);
		g_return_val_if_fail (read != -1, FALSE);

		if (read == 0) {
			break;
		}

		g_string_append_len (disk_data, buffer, read);
	}

	if (xmms_sc68_verify_mem (disk_data->str, disk_data->len)) {
		g_string_free (disk_data, TRUE);
		xmms_log_info ("Could not verify sc68 disk");
		return FALSE;
	}

	sc68->disk = sc68_disk_load_mem (disk_data->str,
	                                 disk_data->len);

	g_string_free (disk_data, TRUE);

	if (!sc68->disk) {
		xmms_log_info ("Could not load sc68 disk");
		return FALSE;
	}

	if (sc68_music_info (sc68_api, &disk_info, 0, sc68->disk)) {
		xmms_log_info ("Could not get sc68 disk info");
		return FALSE;
	}

	if (xmms_xform_metadata_get_str (xform, "subtune", &track_string)) {
		sc68->track = atoi (track_string);

		if (0 > sc68->track || sc68->track > disk_info.tracks) {
			xmms_log_info ("track %i is not on the sc68 disk", sc68->track);
			return FALSE;
		}
	} else {
		sc68->track = 0;
	}

	if (sc68_music_info (sc68_api, &disk_info, sc68->track, sc68->disk)) {
		xmms_log_info ("Could not get sc68 disk info");
		return FALSE;
	}

	xmms_sc68_extract_metadata (xform, &disk_info);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             2,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             44100,
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static void
xmms_sc68_destroy (xmms_xform_t *xform)
{
	xmms_sc68_t *sc68;

	g_return_if_fail (xform);

	sc68 = xmms_xform_private_data_get (xform);
	g_return_if_fail (sc68);

	if (sc68->disk == loaded_disk) {
		sc68_close (sc68_api);

		loaded_disk = NULL;
	} else {
		sc68_disk_free (sc68->disk);
	}

	g_free (sc68);
}

static gint
xmms_sc68_read (xmms_xform_t *xform, gpointer buffer, gint length,
                xmms_error_t *error)
{
	int code;
	xmms_sc68_t *sc68;

	sc68 = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (sc68, -1);

	if (sc68->disk != loaded_disk) {
		if (sc68_load_track (sc68->disk, sc68->track, error) == -1) {
			return -1;
		}

		loaded_disk = sc68->disk;
	}

	length = length >> 2;
	code = xmms_sc68_process (sc68_api, buffer, &length);

	if (code == SC68_LOOP && sc68->track != 0) {
		return 0;
	} else if (code == SC68_END) {
		return 0;
	} else if (code == SC68_ERROR) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, xmms_sc68_error (sc68_api));
		return -1;
	}

	return (length << 2) & ~0x4;
}

static int
sc68_load_track (sc68_disk_t disk, int track,
                 xmms_error_t *error)
{
	sc68_close (sc68_api);

	if (sc68_open (sc68_api, disk)) {
		xmms_log_info ("Could not open disk");
		xmms_error_set (error, XMMS_ERROR_GENERIC, xmms_sc68_error (sc68_api));
		return -1;
	}

	if (sc68_play (sc68_api, track, 1)) {
		xmms_log_info ("Could not set track on sc68 disk");
		xmms_error_set (error, XMMS_ERROR_GENERIC, xmms_sc68_error (sc68_api));
		return -1;
	}

	return 0;
}
