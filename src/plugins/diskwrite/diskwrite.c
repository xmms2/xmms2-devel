/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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




#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"

#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

/*
 * Defines
 */
#define WAVE_HEADER_SIZE 44

#define PUT_16(buf, val) do { \
	guint16 tmp = GUINT16_TO_LE (val); \
	memcpy (buf, &tmp, 2); \
	buf += 2; \
} while (0)

#define PUT_32(buf, val) do { \
	guint32 tmp = GUINT32_TO_LE (val); \
	memcpy (buf, &tmp, 4); \
	buf += 4; \
} while (0)

#define PUT_STR(buf, str) do { \
	size_t len = strlen (str); \
	strncpy ((gchar *) buf, str, len); \
	buf += len; \
} while (0)

/*
 * Type definitions
 */

typedef struct {
	FILE *fp;
	gchar destdir[XMMS_PATH_MAX];
} xmms_diskwrite_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_null_plugin_setup (xmms_output_plugin_t *plugin);
static gboolean xmms_diskwrite_new (xmms_output_t *output);
static void xmms_diskwrite_destroy (xmms_output_t *output);
static gboolean xmms_diskwrite_open (xmms_output_t *output);
static void xmms_diskwrite_close (xmms_output_t *output);
static void xmms_diskwrite_write (xmms_output_t *output, gpointer buffer,
                                  gint len, xmms_error_t *error);
static void xmms_diskwrite_flush (xmms_output_t *output);

static void on_playlist_entry_changed (xmms_object_t *object, xmmsv_t *arg, gpointer udata);
static void on_dest_directory_changed (xmms_object_t *object, xmmsv_t *value, gpointer udata);
static void finalize_wave (xmms_diskwrite_data_t *data);

/*
 * Plugin header
 */

XMMS_OUTPUT_PLUGIN ("diskwrite", "Diskwriter Output", XMMS_VERSION,
                    "Dumps audio data to disk",
                    xmms_null_plugin_setup);

static gboolean
xmms_null_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);

	methods.new = xmms_diskwrite_new;
	methods.destroy = xmms_diskwrite_destroy;

	methods.open = xmms_diskwrite_open;
	methods.close = xmms_diskwrite_close;

	methods.flush = xmms_diskwrite_flush;
	methods.write = xmms_diskwrite_write;

	xmms_output_plugin_methods_set (plugin, &methods);

	xmms_output_plugin_config_property_register (plugin,
	                                             "destination_directory",
	                                             "/tmp", NULL, NULL);
	return TRUE;
}

/*
 * Member functions
 */

static gboolean
xmms_diskwrite_new (xmms_output_t *output)
{
	xmms_diskwrite_data_t *data;
	xmms_config_property_t *val;
	const gchar *tmp;

	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_diskwrite_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_output_private_data_set (output, data);

	xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_S16, 2, 44100);

	val = xmms_output_config_lookup (output, "destination_directory");
	xmms_config_property_callback_set (val, on_dest_directory_changed, data);

	tmp = xmms_config_property_get_string (val);
	if (tmp) {
		g_snprintf (data->destdir, sizeof (data->destdir), "%s", tmp);
	}

	xmms_object_connect (XMMS_OBJECT (output),
	                     XMMS_IPC_SIGNAL_PLAYBACK_CURRENTID,
	                     on_playlist_entry_changed,
	                     data);

	return TRUE;
}

static void
xmms_diskwrite_destroy (xmms_output_t *output)
{
	xmms_config_property_t *val;
	gpointer data;

	g_return_if_fail (output);

	data = xmms_output_private_data_get (output);

	val = xmms_output_config_lookup (output, "destination_directory");
	xmms_config_property_callback_remove (val, on_dest_directory_changed, data);

	xmms_object_disconnect (XMMS_OBJECT (output),
	                        XMMS_IPC_SIGNAL_PLAYBACK_CURRENTID,
	                        on_playlist_entry_changed,
	                        data);

	g_free (data);
}

static gboolean
xmms_diskwrite_open (xmms_output_t *output)
{
	xmms_diskwrite_data_t *data;
	gint ret;

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	/* create the destination directory if it doesn't exist yet */
	if (!g_file_test (data->destdir, G_FILE_TEST_IS_DIR)) {
		ret = g_mkdir_with_parents (data->destdir, 0755);
	} else {
		ret = access (data->destdir, W_OK);
	}

	if (ret == -1) {
		xmms_log_error ("errno (%d) %s", errno, strerror (errno));
		return FALSE;
	} else {
		return TRUE;
	}
}

static void
xmms_diskwrite_close (xmms_output_t *output)
{
	xmms_diskwrite_data_t *data;

	g_return_if_fail (output);

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->fp) {
		finalize_wave (data);
		fclose (data->fp);
		data->fp = NULL;
	}
}

static void
xmms_diskwrite_write (xmms_output_t *output, gpointer buffer, gint len,
                      xmms_error_t *error)
{
	xmms_diskwrite_data_t *data;
#if G_BYTE_ORDER == G_BIG_ENDIAN
	gint16 *s = (gint16 *) buffer;
	gint i;
#endif

	g_return_if_fail (output);
	g_return_if_fail (buffer);
	g_return_if_fail (len > 0);

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);
	g_return_if_fail (data->fp);

#if G_BYTE_ORDER == G_BIG_ENDIAN
	for (i = 0; i < (len / 2); i++) {
		s[i] = GINT16_TO_LE (s[i]);
	}
#endif

	fwrite (buffer, sizeof (gchar), len, data->fp);
}

static void
xmms_diskwrite_flush (xmms_output_t *output)
{
	xmms_diskwrite_data_t *data;

	g_return_if_fail (output);

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);
	g_return_if_fail (data->fp);

	fflush (data->fp);
}

static void
on_dest_directory_changed (xmms_object_t *object, xmmsv_t *_value, gpointer udata)
{
	xmms_diskwrite_data_t *data = udata;
	const char *value;

	g_return_if_fail (data);

	value = xmms_config_property_get_string ((xmms_config_property_t *)object);
	if (value) {
		g_snprintf (data->destdir, sizeof (data->destdir), "%s", value);
	} else {
		data->destdir[0] = '\0';
	}
}

static void
on_playlist_entry_changed (xmms_object_t *object, xmmsv_t *arg, gpointer udata)
{
	xmms_diskwrite_data_t *data = udata;
	gint32 id;
	gchar dest[XMMS_PATH_MAX];

	xmmsv_get_int (arg, &id);

	/* assemble path */
	g_snprintf (dest, sizeof (dest), "%s" G_DIR_SEPARATOR_S "%03u.wav",
	            data->destdir, id);

	if (data->fp) {
		finalize_wave (data);
		fclose (data->fp);
	}

	data->fp = fopen (dest, "wb");
	g_return_if_fail (data->fp);

	/* skip the header, it's written later when we know how
	 * large the actual payload is.
	 */
	fseek (data->fp, WAVE_HEADER_SIZE, SEEK_SET);
}

static void
finalize_wave (xmms_diskwrite_data_t *data)
{
	long pos;
	guint16 channels = 2, bits_per_sample = 16;
	guint16 bytes_per_sample = (channels * bits_per_sample) / 8;
	guint32 samplerate = 44100;
	gint8 hdr[WAVE_HEADER_SIZE];
	gint8 *ptr = hdr;

	g_return_if_fail (data->fp);

	pos = ftell (data->fp);
	g_return_if_fail (pos > WAVE_HEADER_SIZE);

	PUT_STR (ptr, "RIFF");
	PUT_32 (ptr, pos - 8);
	PUT_STR (ptr, "WAVE");

	PUT_STR (ptr, "fmt ");
	PUT_32 (ptr, 16); /* format chunk length */
	PUT_16 (ptr, 1); /* format tag */
	PUT_16 (ptr, channels);
	PUT_32 (ptr, samplerate);
	PUT_32 (ptr, bytes_per_sample * samplerate);
	PUT_16 (ptr, bytes_per_sample);
	PUT_16 (ptr, bits_per_sample);

	PUT_STR (ptr, "data");
	PUT_32 (ptr, pos - WAVE_HEADER_SIZE);

	fseek (data->fp, 0, SEEK_SET);
	fwrite (hdr, sizeof (guint8), WAVE_HEADER_SIZE, data->fp);
}
