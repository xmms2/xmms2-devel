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
 */

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_util.h>

#include <cdio/cdda.h>
#include <cdio/cdio.h>
#include <cdio/logging.h>
#include <discid/discid.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef DISCID_HAVE_SPARSE_READ
#define discid_read_sparse(disc, dev, i) discid_read(disc, dev)
#endif

typedef struct {
	CdIo_t *cdio;
	cdrom_drive_t *drive;
	track_t track;
	lsn_t first_lsn;
	lsn_t last_lsn;
	lsn_t current_lsn;

	gchar read_buf[CDIO_CD_FRAMESIZE_RAW];
	gulong buf_used;
} xmms_cdda_data_t;

static gboolean xmms_cdda_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_cdda_init (xmms_xform_t *xform);
static gboolean xmms_cdda_browse (xmms_xform_t *xform, const gchar *url,
                                  xmms_error_t *error );
static gint xmms_cdda_read (xmms_xform_t *xform, void *buffer, gint len,
                            xmms_error_t *error);
static gint64 xmms_cdda_seek (xmms_xform_t *xform, gint64 samples,
                              xmms_xform_seek_mode_t whence, xmms_error_t *err);
static void xmms_cdda_destroy (xmms_xform_t *xform);

static CdIo_t *open_cd (xmms_xform_t *xform);
static gboolean get_disc_ids (const gchar *device, gchar **disc_id,
                              gchar **cddb_id, track_t *tracks);

XMMS_XFORM_PLUGIN ("cdda",
                   "CD Digital Audio transport",
                   XMMS_VERSION,
                   "CD Digital Audio Transport",
                   xmms_cdda_plugin_setup);

static void
log_handler (cdio_log_level_t level, const char *message)
{
	switch (level) {
		case CDIO_LOG_DEBUG:
			XMMS_DBG ("libcdio (%d): %s.", level, message);
			break;
		case CDIO_LOG_INFO:
		case CDIO_LOG_WARN:
			xmms_log_info ("libcdio (%d): %s.", level, message);
			break;
		default:
			xmms_log_error ("libcdio (%d): %s.", level, message);
			break;
	}
}

static gboolean
xmms_cdda_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	const gchar *device;
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_cdda_init;
	methods.destroy = xmms_cdda_destroy;
	methods.read = xmms_cdda_read;
	methods.seek = xmms_cdda_seek;
	methods.browse = xmms_cdda_browse;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url",
	                              XMMS_STREAM_TYPE_URL,
	                              "cdda://*",
	                              XMMS_STREAM_TYPE_END);

	device = cdio_get_default_device (NULL);
	if (!device) {
		device = "";
	}

	xmms_xform_plugin_config_property_register (xform_plugin, "device",
	                                            device, NULL, NULL);

	xmms_xform_plugin_config_property_register (xform_plugin, "accessmode",
	                                            "default", NULL, NULL);

	return TRUE;
}

static gboolean
xmms_cdda_init (xmms_xform_t *xform)
{
	CdIo_t *cdio = NULL;
	cdrom_drive_t *drive = NULL;
	const gchar *url;
	gchar **url_data = NULL;
	gchar *url_end;
	xmms_cdda_data_t *data;
	guint playtime;
	lsn_t first_lsn;
	track_t track;
	gchar *disc_id = NULL;
	gchar *cddb_id = NULL;
	xmms_config_property_t *val;
	const gchar *device;
	const gchar *metakey;
	gboolean ret = TRUE;

	g_return_val_if_fail (xform, FALSE);
	url = xmms_xform_indata_get_str (xform, XMMS_STREAM_TYPE_URL);

	if (g_ascii_strcasecmp (url, "cdda://") == 0) {
		xmms_xform_outdata_type_add (xform,
		                             XMMS_STREAM_TYPE_MIMETYPE,
		                             "application/x-xmms2-playlist-entries",
		                             XMMS_STREAM_TYPE_END);
		return TRUE;
	}

	val = xmms_xform_config_lookup (xform, "device");
	device = xmms_config_property_get_string (val);

	if (!get_disc_ids (device, &disc_id, &cddb_id, 0)) {
		return FALSE;
	}

	url += 7;
	url_data = g_strsplit (url, "/", 2);

	if (g_ascii_strcasecmp (url_data[0], disc_id)) {
		xmms_log_error ("Wrong disc inserted.");
		ret = FALSE;
		goto end;
	}

	if (url_data[1] == NULL) {
		xmms_log_error ("Missing track number.");
		ret = FALSE;
		goto end;
	}

	track = strtol (url_data[1], &url_end, 10);
	if (url_data[1] == url_end) {
		xmms_log_error ("Invalid track, need a number.");
		ret = FALSE;
		goto end;
	}

	cdio = open_cd (xform);
	if (!cdio) {
		ret = FALSE;
		goto end;
	}

	drive = cdio_cddap_identify_cdio (cdio, 1, NULL);
	if (!drive) {
		xmms_log_error ("Failed to identify drive.");
		ret = FALSE;
		goto end;
	}

	if (cdio_cddap_open (drive)) {
		xmms_log_error ("Unable to open disc.");
		ret = FALSE;
		goto end;
	}

	first_lsn = cdio_cddap_track_firstsector (drive, track);
	if (first_lsn == -1) {
		xmms_log_error ("No such track.");
		ret = FALSE;
		goto end;
	}

	data = g_new (xmms_cdda_data_t, 1);
	data->cdio = cdio;
	data->drive = drive;
	data->track = track;
	data->first_lsn = first_lsn;
	data->last_lsn = cdio_cddap_track_lastsector (drive, data->track);
	data->current_lsn = first_lsn;
	data->buf_used = CDIO_CD_FRAMESIZE_RAW;

	playtime = (data->last_lsn - data->first_lsn) *
	           1000.0 / CDIO_CD_FRAMES_PER_SEC;

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
	xmms_xform_metadata_set_int (xform, metakey, playtime);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE;
	xmms_xform_metadata_set_int (xform, metakey, 141120);

	xmms_xform_metadata_set_str (xform, "disc_id", url_data[0]);
	xmms_xform_metadata_set_str (xform, "cddb_id", cddb_id);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR;
	xmms_xform_metadata_set_int (xform, metakey, track);

	xmms_xform_private_data_set (xform, data);

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

end:
	/* These are to be destroyed in every cases... */
	g_free (cddb_id);
	g_free (disc_id);
	g_strfreev (url_data);

	/* destroy cdio/drive in case of failure */
	if (!ret) {
		if (drive) {
			cdio_cddap_close_no_free_cdio (drive);
		}
		if (cdio) {
			cdio_destroy (cdio);
		}
	}

	return ret;
}

static gboolean
xmms_cdda_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error)
{
	track_t track_count, t;
	gchar cdda_url[XMMS_PATH_MAX];
	xmms_config_property_t *val;
	const gchar *device;
	gchar *disc_id;

	g_return_val_if_fail (xform, FALSE);

	val = xmms_xform_config_lookup (xform, "device");
	device = xmms_config_property_get_string (val);
	if (!get_disc_ids (device, &disc_id, 0, &track_count)) {
		return FALSE;
	}

	for (t = 1; t <= track_count; t++) {
		gchar trackno[3];

		g_snprintf (cdda_url, XMMS_PATH_MAX, "cdda://%s/%d", disc_id, t);
		XMMS_DBG ("Adding '%s'.", cdda_url);

		g_snprintf (trackno, sizeof (trackno), "%d", t);
		xmms_xform_browse_add_symlink (xform, trackno, cdda_url);
		xmms_xform_browse_add_entry_property_int (xform, "intsort", t);
	}
	g_free (disc_id);

	return TRUE;
}

static void
xmms_cdda_destroy (xmms_xform_t *xform)
{
	xmms_cdda_data_t *data;
	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	if (data) {
		if (data->drive) {
			cdio_cddap_close_no_free_cdio (data->drive);
		}

		if (data->cdio) {
			cdio_destroy (data->cdio);
		}

		g_free (data);
	}
}

static gint
xmms_cdda_read (xmms_xform_t *xform, void *buffer,
                gint len, xmms_error_t *error)
{
	xmms_cdda_data_t *data;
	gint ret;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (error, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	if (cdio_get_media_changed (data->cdio)) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "CD ejected");
		return -1;
	}

	if (data->current_lsn >= data->last_lsn) {
		return 0;
	}

	if (data->buf_used == CDIO_CD_FRAMESIZE_RAW) {
		cdio_cddap_read (data->drive, data->read_buf, data->current_lsn, 1);
		data->current_lsn++;
		data->buf_used = 0;
	}

	if (len >= CDIO_CD_FRAMESIZE_RAW) {
		ret = CDIO_CD_FRAMESIZE_RAW - data->buf_used;
		memcpy (buffer, data->read_buf + data->buf_used, ret);
	} else {
		gulong buf_left = CDIO_CD_FRAMESIZE_RAW - data->buf_used;

		if (buf_left < len) {
			memcpy (buffer, data->read_buf + data->buf_used, buf_left);
			ret = buf_left;
		} else {
			memcpy (buffer, data->read_buf + data->buf_used, len);
			ret = len;
		}
	}
	data->buf_used += ret;

	return ret;
}

static gint64
xmms_cdda_seek (xmms_xform_t *xform, gint64 samples,
                xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_cdda_data_t *data;
	lsn_t new_lsn;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	/* Magic number 42... really should think of a better way to do this but
	 * it seemed that the lsn is off by about 42 everytime...
	 */
	new_lsn = samples / 441.0 * CDIO_CD_FRAMES_PER_SEC / 100 + 42;

	if ((data->first_lsn + new_lsn) > data->last_lsn) {
		xmms_log_error ("Trying to seek past the end of track.");
		return -1;
	}

	data->current_lsn = data->first_lsn + new_lsn;

	return samples;
}

/*
 * Private stuff
 */
static CdIo_t *
open_cd (xmms_xform_t *xform)
{
	CdIo_t *cdio;
	xmms_config_property_t *val;
	const gchar *device;
	const gchar *accessmode;

	cdio_log_set_handler (log_handler);

	val = xmms_xform_config_lookup (xform, "device");
	device = xmms_config_property_get_string (val);

	val = xmms_xform_config_lookup (xform, "accessmode");
	accessmode = xmms_config_property_get_string (val);

	XMMS_DBG ("Trying to open device '%s', using '%s' access mode.",
	          device, accessmode);

	if (g_ascii_strcasecmp (accessmode, "default") == 0) {
		cdio = cdio_open (device, DRIVER_UNKNOWN);
	} else {
		cdio = cdio_open_am (device, DRIVER_UNKNOWN, accessmode);
	}

	if (!cdio) {
		xmms_log_error ("Failed to open device '%s'.", device);
	} else {
		cdio_set_speed (cdio, 1);
		xmms_log_info ("Opened device '%s'.", device);
	}

	return cdio;
}

static gboolean
get_disc_ids (const gchar *device, gchar **disc_id,
              gchar **cddb_id, track_t *tracks)
{
	DiscId *disc = discid_new ();
	g_return_val_if_fail (disc, FALSE);

	if (discid_read_sparse (disc, device, 0) == 0) {
		xmms_log_error ("Could not read disc: %s", discid_get_error_msg (disc));
		discid_free (disc);
		return FALSE;
	}

	*disc_id = g_strdup (discid_get_id (disc));
	if (tracks) {
		*tracks = discid_get_last_track_num (disc);
	}

	if (cddb_id) {
		*cddb_id = g_strdup (discid_get_freedb_id (disc));
	}

	discid_free (disc);

	return TRUE;
}
