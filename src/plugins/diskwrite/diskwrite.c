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




#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/xmms.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/signal_xmms.h"
#include "xmms/playlist_entry.h"

#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

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
	strncpy (buf, str, len); \
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

static gboolean xmms_diskwrite_new (xmms_output_t *output);
static void xmms_diskwrite_destroy (xmms_output_t *output);
static gboolean xmms_diskwrite_open (xmms_output_t *output);
static void xmms_diskwrite_close (xmms_output_t *output);
static guint xmms_diskwrite_samplerate_set (xmms_output_t *output, guint rate);
static void xmms_diskwrite_write (xmms_output_t *output, gchar *buffer, gint len);
static void xmms_diskwrite_flush (xmms_output_t *output);

static void on_playlist_entry_changed (xmms_object_t *object,
                                       const xmms_object_method_arg_t *arg,
                                       xmms_diskwrite_data_t *data);
static void on_dest_directory_changed (xmms_object_t *object, gconstpointer value,
                                       xmms_diskwrite_data_t *data);
static void finalize_wave (xmms_diskwrite_data_t *data);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "diskwrite",
	                          "Diskwriter output " XMMS_VERSION,
	                          "Dumps audio data to disk");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW,
	                        xmms_diskwrite_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY,
	                        xmms_diskwrite_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN,
	                        xmms_diskwrite_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE,
	                        xmms_diskwrite_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET,
	                        xmms_diskwrite_samplerate_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE,
	                        xmms_diskwrite_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH,
	                        xmms_diskwrite_flush);

	xmms_plugin_config_value_register (plugin, "destination_directory",
	                                   "/tmp", NULL, NULL);

	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_diskwrite_new (xmms_output_t *output)
{
	xmms_diskwrite_data_t *data;
	xmms_plugin_t *plugin;
	xmms_config_value_t *val;
	const gchar *tmp;

	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_diskwrite_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_output_private_data_set (output, data);

	plugin = xmms_output_plugin_get (output);
	val = xmms_plugin_config_lookup (plugin, "destination_directory");
	xmms_config_value_callback_set (val,
	                                (xmms_object_handler_t) on_dest_directory_changed,
	                                data);

	if ((tmp = xmms_config_value_string_get (val))) {
		g_snprintf (data->destdir, sizeof (data->destdir), "%s", tmp);
	}

	xmms_object_connect (XMMS_OBJECT (output),
	                     XMMS_SIGNAL_OUTPUT_CURRENT_ENTRY,
	                     (xmms_object_handler_t ) on_playlist_entry_changed,
	                     data);

	return TRUE;
}

static void
xmms_diskwrite_destroy (xmms_output_t *output)
{
	xmms_plugin_t *plugin;
	xmms_config_value_t *val;

	g_return_if_fail (output);

	plugin = xmms_output_plugin_get (output);

	val = xmms_plugin_config_lookup (plugin, "destination_directory");
	xmms_config_value_callback_remove (val,
		(xmms_object_handler_t) on_dest_directory_changed);

	g_free (xmms_output_private_data_get (output));
}

static gboolean
xmms_diskwrite_open (xmms_output_t *output)
{
	xmms_diskwrite_data_t *data;

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	/* create the destination directory if it doesn't exist yet */
	if (!g_file_test (data->destdir, G_FILE_TEST_IS_DIR)) {
		mkdir (data->destdir, 0755);
	} else {
		access (data->destdir, W_OK);
	}

	if (errno) {
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

static guint
xmms_diskwrite_samplerate_set (xmms_output_t *output, guint rate)
{
	return 44100;
}

static void
xmms_diskwrite_write (xmms_output_t *output, gchar *buffer, gint len)
{
	xmms_diskwrite_data_t *data;

	g_return_if_fail (output);
	g_return_if_fail (buffer);
	g_return_if_fail (len > 0);

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);
	g_return_if_fail (data->fp);

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
on_dest_directory_changed (xmms_object_t *object, gconstpointer value,
                           xmms_diskwrite_data_t *data)
{
	g_return_if_fail (data);

	if (value) {
		g_snprintf (data->destdir, sizeof (data->destdir), "%s",
		            (gchar *) value);
	} else {
		data->destdir[0] = '\0';
	}
}

static void
on_playlist_entry_changed (xmms_object_t *object,
                           const xmms_object_method_arg_t *arg,
                           xmms_diskwrite_data_t *data)
{
	xmms_playlist_entry_t *entry = arg->retval.playlist_entry;
	gchar dest[XMMS_PATH_MAX];

	/* assemble path */
	g_snprintf (dest, sizeof (dest), "%s" G_DIR_SEPARATOR_S "%03u.wav",
	            data->destdir, xmms_playlist_entry_id_get (entry));

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
	guint8 hdr[WAVE_HEADER_SIZE], *ptr = hdr;

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
