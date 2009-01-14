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

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* not available everywhere. */
#if !defined(O_BINARY)
# define O_BINARY 0
#endif
/*
 * Type definitions
 */

typedef struct {
	gint fd;
} xmms_file_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_file_init (xmms_xform_t *xform);
static void xmms_file_destroy (xmms_xform_t *xform);
static gint xmms_file_read (xmms_xform_t *xform, void *buffer, gint len, xmms_error_t *error);
static gint64 xmms_file_seek (xmms_xform_t *xform, gint64 offset, xmms_xform_seek_mode_t whence, xmms_error_t *error);
gboolean xmms_file_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error);
static gboolean xmms_file_plugin_setup (xmms_xform_plugin_t *xform_plugin);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN ("file",
                   "File transport",
                   XMMS_VERSION,
                   "Plain local file transport",
                   xmms_file_plugin_setup);

static gboolean
xmms_file_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_file_init;
	methods.destroy = xmms_file_destroy;
	methods.read = xmms_file_read;
	methods.seek = xmms_file_seek;
	methods.browse = xmms_file_browse;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url",
	                              XMMS_STREAM_TYPE_URL,
	                              "file://*",
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}

/*
 * Member functions
 */
static gboolean
xmms_file_init (xmms_xform_t *xform)
{
	gint fd;
	xmms_file_data_t *data;
	const gchar *url;
	const gchar *metakey;
	struct stat st;

	url = xmms_xform_indata_get_str (xform, XMMS_STREAM_TYPE_URL);

	g_return_val_if_fail (xform, FALSE);
	g_return_val_if_fail (url, FALSE);

	/* strip file:// */
	url += 7;

	if (stat (url, &st) == -1) {
		XMMS_DBG ("Couldn't stat file '%s': %s", url, strerror (errno));
		return FALSE;
	}

	if (!S_ISREG (st.st_mode)) {
		return FALSE;
	}

	XMMS_DBG ("Opening %s", url);
	fd = open (url, O_RDONLY | O_BINARY);
	if (fd == -1) {
		return FALSE;
	}

	data = g_new0 (xmms_file_data_t, 1);
	data->fd = fd;
	xmms_xform_private_data_set (xform, data);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/octet-stream",
	                             XMMS_STREAM_TYPE_END);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	xmms_xform_metadata_set_int (xform, metakey, st.st_size);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD;
	xmms_xform_metadata_set_int (xform, metakey, st.st_mtime);

	return TRUE;
}

static void
xmms_file_destroy (xmms_xform_t *xform)
{
	xmms_file_data_t *data;
	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	if (!data)
		return;

	if (data->fd != -1)
		close (data->fd);

	g_free (data);
}

static gint
xmms_file_read (xmms_xform_t *xform, void *buffer, gint len, xmms_error_t *error)
{
	xmms_file_data_t *data;
	gint ret;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (error, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = read (data->fd, buffer, len);

	if (ret == -1) {
		xmms_log_error ("errno(%d) %s", errno, strerror (errno));
		xmms_error_set (error, XMMS_ERROR_GENERIC, strerror (errno));
	}

	return ret;
}

static gint64
xmms_file_seek (xmms_xform_t *xform, gint64 offset, xmms_xform_seek_mode_t whence, xmms_error_t *error)
{
	xmms_file_data_t *data;
	gint w = 0;
	off_t res;

	g_return_val_if_fail (xform, -1);
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	switch (whence) {
		case XMMS_XFORM_SEEK_SET:
			w = SEEK_SET;
			break;
		case XMMS_XFORM_SEEK_END:
			w = SEEK_END;
			break;
		case XMMS_XFORM_SEEK_CUR:
			w = SEEK_CUR;
			break;
	}

	res = lseek (data->fd, offset, w);
	if (res == (off_t)-1) {
		xmms_error_set (error, XMMS_ERROR_INVAL, "Couldn't seek");
		return -1;
	}
	return res;
}
