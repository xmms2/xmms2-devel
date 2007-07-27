/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"
#include <libgnomevfs/gnome-vfs.h>

/*
 * Type definitions
 */

typedef struct {
	GnomeVFSHandle *handle;
} xmms_gnomevfs_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_gnomevfs_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_gnomevfs_init (xmms_xform_t *xform);
static void xmms_gnomevfs_destroy (xmms_xform_t *xform);
static gint xmms_gnomevfs_read (xmms_xform_t *xform, void *buffer,
                                gint len, xmms_error_t *error);
static gint64 xmms_gnomevfs_seek (xmms_xform_t *xform, gint64 offset,
                                  xmms_xform_seek_mode_t whence,
                                  xmms_error_t *error);


/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN ("gnomevfs",
                   "GnomeVFS xform",
                   XMMS_VERSION,
                   "GnomeVFS xform",
                   xmms_gnomevfs_plugin_setup);


static gboolean
xmms_gnomevfs_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_gnomevfs_init;
	methods.destroy = xmms_gnomevfs_destroy;
	methods.read = xmms_gnomevfs_read;
	methods.seek = xmms_gnomevfs_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url",
	                              XMMS_STREAM_TYPE_URL,
	                              "ssh://*",
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}


/*
 * Member functions
 */
static gboolean
xmms_gnomevfs_init (xmms_xform_t *xform)
{
	xmms_gnomevfs_data_t *data;
	GnomeVFSFileInfo *info = NULL;
	GnomeVFSHandle *handle;
	GnomeVFSResult result;
	const gchar *url;
	const gchar *metakey;
	gboolean ret = FALSE;

	url = xmms_xform_indata_get_str (xform, XMMS_STREAM_TYPE_URL);

	g_return_val_if_fail (xform, FALSE);
	g_return_val_if_fail (url, FALSE);

	if (!gnome_vfs_init ()) {
		goto init_end;
	}

	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (url, info,
	                                  GNOME_VFS_FILE_INFO_DEFAULT);

	if (result != GNOME_VFS_OK) {
		xmms_log_error ("%s", gnome_vfs_result_to_string (result));
		goto init_end;
	}

	result = gnome_vfs_open (&handle, url,
	                         GNOME_VFS_OPEN_READ |
	                         GNOME_VFS_OPEN_RANDOM);

	if (result != GNOME_VFS_OK) {
		xmms_log_error ("%s", gnome_vfs_result_to_string (result));
		goto init_end;
	}

	data = g_new0 (xmms_gnomevfs_data_t, 1);
	data->handle = handle;

	xmms_xform_private_data_set (xform, data);
	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	xmms_xform_metadata_set_int (xform, metakey, info->size);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD;
	xmms_xform_metadata_set_int (xform, metakey, info->mtime);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/octet-stream",
	                             XMMS_STREAM_TYPE_END);
	ret = TRUE;

init_end:
	if (info) {
		gnome_vfs_file_info_unref (info);
	}

	return ret;
}


static void
xmms_gnomevfs_destroy (xmms_xform_t *xform)
{
	xmms_gnomevfs_data_t *data;
	GnomeVFSResult result;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	result = gnome_vfs_close (data->handle);
	if (result != GNOME_VFS_OK) {
		xmms_log_error ("%s", gnome_vfs_result_to_string (result));
	}

	gnome_vfs_shutdown ();

	g_free (data);
}


static gint
xmms_gnomevfs_read (xmms_xform_t *xform, void *buffer, gint len,
                    xmms_error_t *error)
{
	xmms_gnomevfs_data_t *data;
	GnomeVFSFileSize bytes_read;
	GnomeVFSResult result;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (error, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	result = gnome_vfs_read (data->handle, buffer, len, &bytes_read);

	if (result == GNOME_VFS_ERROR_EOF) {
		const gchar *err = gnome_vfs_result_to_string (result);
		xmms_error_set (error, XMMS_ERROR_EOS, err);
		return 0;
	}

	if (result != GNOME_VFS_OK) {
		const gchar *err = gnome_vfs_result_to_string (result);
		xmms_error_set (error, XMMS_ERROR_GENERIC, err);
		return -1;
	}

	return bytes_read;
}


static gint64
xmms_gnomevfs_seek (xmms_xform_t *xform, gint64 offset,
                    xmms_xform_seek_mode_t whence, xmms_error_t *error)
{
	xmms_gnomevfs_data_t *data;

	GnomeVFSSeekPosition w;
	GnomeVFSFileSize ret;
	GnomeVFSResult result;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (error, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	switch (whence) {
		case XMMS_XFORM_SEEK_SET:
			w = GNOME_VFS_SEEK_START;
			break;
		case XMMS_XFORM_SEEK_END:
			w = GNOME_VFS_SEEK_END;
			break;
		case XMMS_XFORM_SEEK_CUR:
			w = GNOME_VFS_SEEK_CURRENT;
			break;
	}

	result = gnome_vfs_seek (data->handle, w, (GnomeVFSFileOffset) offset);
	if (result != GNOME_VFS_OK) {
		const gchar *err = gnome_vfs_result_to_string (result);
		xmms_error_set (error, XMMS_ERROR_INVAL, err);
	}

	result = gnome_vfs_tell (data->handle, &ret);
	if (result != GNOME_VFS_OK) {
		const gchar *err = gnome_vfs_result_to_string (result);
		xmms_error_set (error, XMMS_ERROR_INVAL, err);
	}

	return ret;
}
