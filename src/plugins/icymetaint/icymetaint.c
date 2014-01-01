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

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_medialib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*
 * Type definitions
 */

typedef struct {

	guint bytes_since_meta, meta_offset;

	gchar *metabuffer;
	guint metabufferpos, metabufferleft;

	gint found_mp3_stream;

	xmms_error_t status;
} xmms_icymetaint_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_icymetaint_init (xmms_xform_t *xform);
static void xmms_icymetaint_destroy (xmms_xform_t *xform);
static gint xmms_icymetaint_read (xmms_xform_t *xform, void *buffer, gint len, xmms_error_t *error);
static gboolean xmms_icymetaint_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static void handle_shoutcast_metadata (xmms_xform_t *xform, gchar *metadata);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN ("icymetaint",
                   "Icy-metaint stream decoder",
                   XMMS_VERSION,
                   "Decode & use shoutcast stream metadata",
                   xmms_icymetaint_plugin_setup);

static gboolean
xmms_icymetaint_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_icymetaint_init;
	methods.destroy = xmms_icymetaint_destroy;
	methods.read = xmms_icymetaint_read;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-icy-stream",
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}

/*
 * Member functions
 */

static gboolean
xmms_icymetaint_init (xmms_xform_t *xform)
{
	xmms_icymetaint_data_t *data;
	gint32 meta_offset;
	gboolean res;

	g_return_val_if_fail (xform, FALSE);

	res = xmms_xform_auxdata_get_int (xform, "meta_offset", &meta_offset);
	g_return_val_if_fail (res, FALSE);

	XMMS_DBG ("meta_offset = %d", meta_offset);

	data = g_new0 (xmms_icymetaint_data_t, 1);

	data->metabuffer = g_malloc (256 * 16);
	data->meta_offset = meta_offset;

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/octet-stream",
	                             XMMS_STREAM_TYPE_END);

	xmms_xform_private_data_set (xform, data);

	return TRUE;
}

static void
xmms_icymetaint_destroy (xmms_xform_t *xform)
{
	xmms_icymetaint_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	g_free (data->metabuffer);

	g_free (data);
}

static gint
xmms_icymetaint_read (xmms_xform_t *xform, void *orig_ptr, gint orig_len, xmms_error_t *error)
{
	xmms_icymetaint_data_t *data;
	int bufferlen;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (orig_ptr, -1);
	g_return_val_if_fail (error, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	do {
		void *ptr;
		gint len;
		char *buffer;

		ptr = orig_ptr;
		len = orig_len;

		len = xmms_xform_read (xform, ptr, len, error);

		if (len <= 0) {
			return len;
		}

		buffer = ptr;
		bufferlen = 0;

		while (len) {
			if (data->metabufferleft) {
				gint tlen = MIN (len, data->metabufferleft);
				memcpy (data->metabuffer + data->metabufferpos, ptr, tlen);
				data->metabufferleft -= tlen;
				data->metabufferpos += tlen;
				if (!data->metabufferleft) {
					handle_shoutcast_metadata (xform, data->metabuffer);
					data->bytes_since_meta = 0;
				}
				len -= tlen;
				ptr += tlen;

			} else if (data->meta_offset &&
			           data->bytes_since_meta == data->meta_offset) {

				data->metabufferleft = (*((guchar *)ptr)) * 16;
				data->metabufferpos = 0;
				len--;
				ptr++;
				if (data->metabufferleft == 0)
					data->bytes_since_meta = 0;

			} else {
				gint tlen = len;
				gint tlen2;

				if (data->meta_offset) {
					tlen = MIN (tlen,
					            data->meta_offset - data->bytes_since_meta);
				}

				tlen2 = tlen;

				/* this is a hack to find the first mp3 frame */
				if (data->found_mp3_stream == 0) {
					unsigned char *p = ptr;
					int i;
					for (i=0; i<tlen-1; ++i) {
						if (p[i + 0] == 0xff && (p[i + 1] & 0xf0) == 0xf0) {
							break;
						}
					}
					ptr += i;
					tlen -= i;
					data->found_mp3_stream = 1;
				}
				/* end of hack */

				if (buffer + bufferlen != ptr) {
					memmove (buffer + bufferlen, ptr, tlen);
				}
				len -= tlen2;
				ptr += tlen;
				data->bytes_since_meta += tlen2;
				bufferlen += tlen;
			}
		}

		/* loop in case we got just metadata to avoid returning 0
		   which would falsely indicate end-of-file */
	} while (bufferlen == 0);

	return bufferlen;
}

static void
handle_shoutcast_metadata (xmms_xform_t *xform, gchar *metadata)
{
	gchar **tags;
	guint i = 0;

	g_return_if_fail (xform);
	g_return_if_fail (metadata);

	XMMS_DBG ("metadata: %s", metadata);

	tags = g_strsplit (metadata, ";", 0);
	while (tags[i] != NULL) {
		if (g_ascii_strncasecmp (tags[i], "StreamTitle=", 12) == 0) {
			const gchar *metakey;
			gchar *raw;

			raw = tags[i] + 13;
			raw[strlen (raw) - 1] = '\0';

			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE;
			xmms_xform_metadata_set_str (xform, metakey, raw);
		}

		i++;
	}

	g_strfreev (tags);
}
